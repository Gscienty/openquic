/*
 * Copyright (c) 2020 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#include "client.h"
#include "modules/recver.h"
#include "modules/stream.h"
#include <openssl/rand.h>
#include <stdlib.h>

const quic_config_t quic_client_default_config = {
    .is_cli = true,
    .stream_recv_timeout = 0,
    .active_connid_count = 2,
    .disable_prr = false,
    .initial_cwnd = 1460,
    .min_cwnd = 1460,
    .max_cwnd = 10 * 1460,
    .slowstart_large_reduction = true,
    .stream_flowctrl_initial_rwnd = 1460,
    .stream_flowctrl_max_rwnd_size = 5 * 1460,
    .stream_flowctrl_initial_swnd = 5 * 1460,
    .conn_flowctrl_initial_rwnd = 1460,
    .conn_flowctrl_max_rwnd_size = 5 * 1460,
    .conn_flowctrl_initial_swnd = 1460,
    .tls_ciphers = NULL,
    .tls_curve_groups = NULL,
    .tls_cert_chain_file = NULL,
    .tls_key_file = NULL,
    .tls_verify_client_ca = NULL,
    .tls_ca = NULL,
    .tls_capath = NULL,
    .stream_destory_timeout = 0,
    .disable_migrate = false,
};

static int quic_client_session_free_st_cb(void *const args);
static quic_err_t quic_client_transmission_recv_cb(quic_transmission_t *const transmission, quic_recv_packet_t *const recvpkt);

static void quic_client_eloop_close_cb(liteco_async_t *const event);
static void quic_client_session_replace_close_cb(quic_session_t *const session, const quic_buf_t pkt);

quic_err_t quic_client_init(quic_client_t *const client, const size_t extends_size, const size_t st_size) {
    uint8_t rand = 0;
    quic_buf_t src;

    void *st = quic_malloc(st_size);
    if (!st) {
        return quic_err_internal_error;
    }

    /*if (RAND_bytes(&rand, 1) <= 0) {*/
        /*return quic_err_internal_error;*/
    /*}*/
    client->st_size = st_size;
    client->connid_len = 8 + rand % 11;

    quic_buf_init(&src);
    src.capa = client->connid_len;
    if (!(src.buf = malloc(src.capa))) {
        return quic_err_internal_error;
    }
    if (RAND_bytes(src.buf, src.capa) <= 0) {
        return quic_err_internal_error;
    }
    quic_buf_setpl(&src);

    liteco_eloop_init(&client->eloop);
    liteco_runtime_init(&client->eloop, &client->rt);
    quic_transmission_init(&client->transmission, &client->rt);
    quic_transmission_recv(&client->transmission, quic_client_transmission_recv_cb);

    client->session = quic_session_create(&client->transmission, quic_client_default_config, extends_size);
    client->session->src = src;
    client->session->replace_close = quic_client_session_replace_close_cb;

    quic_buf_t *const dst = &client->session->dst;
    dst->capa = client->connid_len;
    if (!(dst->buf = quic_malloc(dst->capa))) {
        return quic_err_internal_error;
    }
    if (RAND_bytes(dst->buf, dst->capa) <= 0) {
        return quic_err_internal_error;
    }
    quic_buf_setpl(dst);

    quic_session_init(client->session, &client->eloop, &client->rt, st, st_size);
    quic_session_finished(client->session, quic_client_session_free_st_cb, st);

    liteco_runtime_join(&client->rt, &client->session->co);

    client->closed = false;
    liteco_async_init(&client->eloop, &client->closed_event, quic_client_eloop_close_cb);

    liteco_rbt_init(client->srcs);

    return quic_err_success;
}

quic_err_t quic_client_start_loop(quic_client_t *const client) {
    if (client->closed) {
        return quic_err_success;
    }

    for ( ;; ) {
        liteco_eloop_run(&client->eloop);
        if (client->closed) {
            break;
        }
    }
    return quic_err_success;
}

quic_err_t quic_client_listen(quic_client_t *const client, const liteco_addr_t loc_addr, const uint32_t mtu) {
    return quic_transmission_listen(&client->eloop, &client->transmission, loc_addr, mtu);
}

quic_err_t quic_client_path_use(quic_client_t *const client, const quic_path_t path) {
    if (!quic_transmission_exist(&client->transmission, path.loc_addr)) {
        quic_client_listen(client, path.loc_addr, 1460);
    }

    return quic_session_path_use(client->session, path);
}

quic_err_t quic_client_path_target_use(quic_client_t *const client, const liteco_addr_t rmt_addr) {
    return quic_session_path_target_use(client->session, rmt_addr);
}

quic_err_t quic_client_cert_file(quic_client_t *const client, const char *const cert_file) {
    return quic_session_cert_file(client->session, cert_file);
}

quic_err_t quic_client_key_file(quic_client_t *const client, const char *const key_file) {
    return quic_session_key_file(client->session, key_file);
}

quic_err_t quic_client_accept(quic_client_t *const client, const size_t extends_size, quic_err_t (*accept_cb) (quic_stream_t *const)) {
    return quic_session_accept(client->session, extends_size, accept_cb);
}

quic_stream_t *quic_client_open(quic_client_t *const client, const size_t extends_size, bool bidi) {
    return quic_session_open(client->session, extends_size, bidi);
}

quic_err_t quic_client_handshake_done(quic_client_t *const client, quic_err_t (*handshake_done_cb) (quic_session_t *const)) {
    return quic_session_handshake_done(client->session, handshake_done_cb);
}

static quic_err_t quic_client_transmission_recv_cb(quic_transmission_t *const transmission, quic_recv_packet_t *const recvpkt) {
    quic_client_t *const client = ((void *) transmission) - offsetof(quic_client_t, transmission);

    // TODO check client

    quic_recver_module_t *const r_module = quic_session_module(client->session, quic_recver_module);

    return quic_recver_push(r_module, recvpkt);
}

quic_client_t *quic_session_client(quic_session_t *const session) {
    return ((void *) session->transmission) - offsetof(quic_client_t, transmission);
}

static void quic_client_eloop_close_cb(liteco_async_t *const event) {
    quic_client_t *const client = ((void *) event) - offsetof(quic_client_t, closed_event);
    if (client->closed) {
        return;
    }

    client->closed = true;
    liteco_eloop_close(&client->eloop);
}

static void quic_client_session_replace_close_cb(quic_session_t *const session, const quic_buf_t pkt) {
    quic_client_t *const client = ((void *) session->transmission) - offsetof(quic_client_t, transmission);

    quic_transmission_send(&client->transmission, session->path, pkt.buf, quic_buf_size(&pkt));
    liteco_async_send(&client->closed_event);
}

static int quic_client_session_free_st_cb(void *const args) {
    free(args);

    return 0;
}
