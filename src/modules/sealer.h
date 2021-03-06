/*
 * Copyright (c) 2020-2021 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#ifndef __OPENQUIC_SEALER_H__
#define __OPENQUIC_SEALER_H__

#include "module.h"
#include "sorter.h"
#include "session.h"
#include "format/frame.h"
#include "modules/framer.h"
#include "modules/ack_generator.h"
#include "liteco.h"
#include <openssl/ssl.h>
#include <openssl/aes.h>
#include <openssl/chacha.h>

#define quic_ssl_session_id_context "OpenQUIC server"

typedef struct quic_header_protector_s quic_header_protector_t;
struct quic_header_protector_s {
    uint32_t suite_id;
    quic_buf_t key;

    uint8_t mask[32];
};

__quic_header_inline quic_err_t quic_header_protector_init(quic_header_protector_t *const header_protector) {
    quic_buf_init(&header_protector->key);
    header_protector->suite_id = 0;

    return quic_err_success;
}

typedef struct quic_sealer_s quic_sealer_t;
struct quic_sealer_s {
    EVP_AEAD_CTX w_ctx;
    const EVP_AEAD *(*w_aead)();
    size_t w_aead_tag_size;
    quic_buf_t w_sec;
    quic_buf_t w_key;
    quic_buf_t w_iv;
    quic_header_protector_t w_hp;

    EVP_AEAD_CTX r_ctx;
    const EVP_AEAD *(*r_aead)();
    size_t r_aead_tag_size;
    quic_buf_t r_sec;
    quic_buf_t r_key;
    quic_buf_t r_iv;
    quic_header_protector_t r_hp;

};

__quic_header_inline quic_err_t quic_sealer_init(quic_sealer_t *const sealer) {
    EVP_AEAD_CTX_zero(&sealer->w_ctx);
    sealer->w_aead = NULL;
    sealer->w_aead_tag_size = 0;
    quic_buf_init(&sealer->w_sec);
    quic_buf_init(&sealer->w_key);
    quic_buf_init(&sealer->w_iv);
    quic_header_protector_init(&sealer->w_hp);

    EVP_AEAD_CTX_zero(&sealer->r_ctx);
    sealer->r_aead = NULL;
    sealer->r_aead_tag_size = 0;
    quic_buf_init(&sealer->r_sec);
    quic_buf_init(&sealer->r_key);
    quic_buf_init(&sealer->r_iv);
    quic_header_protector_init(&sealer->r_hp);

    return quic_err_success;
}

typedef struct quic_sealer_module_s quic_sealer_module_t;
struct quic_sealer_module_s {
    QUIC_MODULE_FIELDS

    bool transport_parameter_processed;
    SSL_CTX *ssl_ctx;
    SSL *ssl;

    int tls_alert;
    uint32_t off;

    enum ssl_encryption_level_t r_level;
    enum ssl_encryption_level_t w_level;

    enum ssl_encryption_level_t level;

    quic_sealer_t initial_sealer;
    quic_sealer_t handshake_sealer;
    quic_sealer_t app_sealer;

    quic_sorter_t initial_r_sorter;
    quic_sorter_t initial_w_sorter;
    quic_sorter_t handshake_r_sorter;
    quic_sorter_t handshake_w_sorter;

    quic_err_t (*handshake_done_cb) (quic_session_t *const);
};

extern quic_module_t quic_sealer_module;

__quic_header_inline quic_err_t quic_sealer_set_level(quic_sealer_module_t *const module, enum ssl_encryption_level_t level) {
    quic_session_t *const session = quic_module_of_session(module);
    quic_ack_generator_module_t *ag_module = NULL;
    quic_retransmission_module_t *r_module = NULL;

    switch (level) {
    case ssl_encryption_handshake:
        ag_module = quic_session_module(session, quic_initial_ack_generator_module);
        r_module = quic_session_module(session, quic_initial_retransmission_module);
        break;

    case ssl_encryption_application:
        ag_module = quic_session_module(session, quic_handshake_ack_generator_module);
        r_module = quic_session_module(session, quic_handshake_retransmission_module);

        if (module->handshake_done_cb) {
            module->handshake_done_cb(session);
        }
        break;
    default:
        break;
    }

    if (ag_module) {
        quic_ack_generator_drop(ag_module);
    }
    if (r_module) {
        quic_retransmission_drop(r_module);
    }

    module->level = level;
    
    return quic_err_success;
}

__quic_header_inline quic_err_t quic_sealer_handshake_process(quic_sealer_module_t *const module) {
    int result = SSL_do_handshake(module->ssl);
    if (result >= 0) {
        quic_session_t *const session = quic_module_of_session(module);

        if (!session->cfg.is_cli) {
            quic_framer_module_t *const f_module = quic_session_module(session, quic_framer_module);
            quic_sealer_set_level(module, ssl_encryption_application);

            quic_frame_handshake_done_t *frame = malloc(sizeof(quic_frame_handshake_done_t));
            quic_frame_init(frame, quic_frame_handshake_done_type);

            quic_framer_ctrl(f_module, (quic_frame_t *) frame);
            quic_module_activate(session, quic_sender_module);
        }
        return quic_err_success;
    }

    int err = SSL_get_error(module->ssl, result);
    switch (err) {
    case SSL_ERROR_WANT_READ:
        break;
    case SSL_ERROR_WANT_WRITE:
        break;
    case SSL_ERROR_WANT_X509_LOOKUP:
        break;
    case SSL_ERROR_SSL:
        break;
    }

    return quic_err_success;
}

__quic_header_inline uint64_t quic_sealer_append_crypto_frame(liteco_linknode_t *const frames, uint64_t len, quic_sealer_module_t *const module, enum ssl_encryption_level_t level) {
    quic_sorter_t *sorter = NULL;

    switch (level) {
    case ssl_encryption_initial:
        sorter = &module->initial_w_sorter;
        break;
    case ssl_encryption_handshake:
        sorter = &module->handshake_w_sorter;
        break;
    default:
        return 0;
    }

    if (quic_sorter_empty(sorter)) {
        return 0;
    }

    if (len > quic_sorter_readable(sorter)) {
        len = quic_sorter_readable(sorter);
    }

    // modify, same as generate max stream data
    quic_frame_crypto_t *frame = malloc(sizeof(quic_frame_crypto_t) + len);
    if (!frame) {
        return 0;
    }
    quic_frame_init(frame, quic_frame_crypto_type);
    frame->len = len;
    frame->off = sorter->readed_size;

    quic_sorter_read(sorter, len, frame->data);
    
    liteco_link_insert_before(frames, frame);

    return quic_frame_size(frame);
}

__quic_header_inline bool quic_sealer_should_send(quic_sealer_module_t *const module, enum ssl_encryption_level_t level) {
    switch (level) {
    case ssl_encryption_initial:
        return !quic_sorter_empty(&module->initial_w_sorter);
    case ssl_encryption_handshake:
        return !quic_sorter_empty(&module->handshake_w_sorter);
    default:
        return false;
    }
}

__quic_header_inline quic_err_t quic_sealer_handshake_done(quic_sealer_module_t *const module, quic_err_t (*handshake_done_cb) (quic_session_t *const)) {
    module->handshake_done_cb = handshake_done_cb;
    return quic_err_success;
}

quic_err_t quic_sealer_seal(quic_send_packet_t *const pkt, quic_sealer_t *const sealer, const quic_buf_t hdr, const size_t src_len);
quic_err_t quic_sealer_open(quic_recv_packet_t *const pkt, quic_sealer_module_t *const module, const size_t src_len);

#endif
