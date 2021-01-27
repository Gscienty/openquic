/*
 * Copyright (c) 2020 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#ifndef __OPENQUIC_CLIENT_H__
#define __OPENQUIC_CLIENT_H__

#include "session.h"
#include "modules/migrate.h"
#include "modules/udp_fd.h"
#include "lc_runtime.h"
#include "lc_eloop.h"

typedef struct quic_client_s quic_client_t;
struct quic_client_s {
    liteco_eloop_t eloop;
    liteco_runtime_t rt;

    quic_session_t *session;

    uint8_t *st;
};

extern const quic_config_t quic_client_default_config;

quic_err_t quic_client_init(quic_client_t *const client, const quic_config_t cfg);

quic_err_t quic_client_path_add(quic_client_t *const client, const uint64_t key, quic_addr_t local_addr, quic_addr_t remote_addr);
quic_err_t quic_client_path_use(quic_client_t *const client, const uint64_t key);

quic_err_t quic_client_accept(quic_client_t *const client, quic_err_t (*accept_cb) (quic_session_t *const, quic_stream_t *const));
quic_stream_t *quic_client_open(quic_client_t *const client, bool bidi);
quic_err_t quic_client_handshake_done(quic_client_t *const client, quic_err_t (*handshake_done_cb) (quic_session_t *const));

quic_err_t quic_client_start_loop(quic_client_t *const client);

#endif
