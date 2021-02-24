/*
 * Copyright (c) 2020-2021 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#ifndef __OPENQUIC_CLIENT_H__
#define __OPENQUIC_CLIENT_H__

#include "session.h"
#include "transmission.h"
#include "modules/migrate.h"
#include "lc_runtime.h"
#include "lc_eloop.h"
#include "lc_event.h"

typedef struct quic_client_s quic_client_t;
struct quic_client_s {
    liteco_eloop_t eloop;
    liteco_runtime_t rt;

    bool closed;
    liteco_event_t closed_event;

    quic_transmission_t transmission;
    size_t connid_len;
    size_t st_size;
    quic_session_t *session;
};

extern const quic_config_t quic_client_default_config;

quic_err_t quic_client_init(quic_client_t *const client, const size_t st_size);
quic_err_t quic_client_listen(quic_client_t *const client, const quic_addr_t local_addr, const uint32_t mtu);
quic_err_t quic_client_path_use(quic_client_t *const client, const quic_path_t path); 
quic_err_t quic_client_path_target_use(quic_client_t *const client, const quic_addr_t remote_addr);

quic_err_t quic_client_cert_file(quic_client_t *const client, const char *const cert_file);
quic_err_t quic_client_key_file(quic_client_t *const client, const char *const key_file);

quic_err_t quic_client_accept(quic_client_t *const client, quic_err_t (*accept_cb) (quic_session_t *const, quic_stream_t *const));
quic_stream_t *quic_client_open(quic_client_t *const client, bool bidi);
quic_err_t quic_client_handshake_done(quic_client_t *const client, quic_err_t (*handshake_done_cb) (quic_session_t *const));

quic_err_t quic_client_start_loop(quic_client_t *const client);

quic_client_t *quic_session_client(quic_session_t *const session);

#endif
