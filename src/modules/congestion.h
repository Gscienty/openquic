/*
 * Copyright (c) 2020 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#ifndef __OPENQUIC_CONGESTION_H__
#define __OPENQUIC_CONGESTION_H__

#include "module.h"
#include <stdbool.h>

typedef struct quic_congestion_module_s quic_congestion_module_t;
struct quic_congestion_module_s {
    QUIC_MODULE_FIELDS

    quic_err_t (*on_sent) (quic_congestion_module_t *const module, const uint64_t sent_time, const uint64_t num, const uint64_t sent_bytes, const bool include_unacked);
    quic_err_t (*on_acked) (quic_congestion_module_t *const module, const uint64_t num, const uint64_t acked_bytes, const uint64_t unacked_unacked, const uint64_t event_time);
    quic_err_t (*on_lost) (quic_congestion_module_t *const module, const uint64_t num, const uint64_t lost_bytes, const uint64_t unacked_bytes);

    quic_err_t (*update) (quic_congestion_module_t *const module, const uint64_t recv_time, const uint64_t sent_time, const uint64_t ack_delay);
    bool (*allow_send) (quic_congestion_module_t *const module, const uint64_t unacked);
    uint64_t (*next_send_time) (quic_congestion_module_t *const module, const uint64_t unacked_bytes);

    bool (*has_budget) (quic_congestion_module_t *const module);

    uint64_t (*pto) (quic_congestion_module_t *const module, const uint64_t max_delay);
    uint64_t (*smoothed_rtt) (quic_congestion_module_t *const module);

    uint64_t (*id) (quic_congestion_module_t *const module);
    quic_err_t (*migrate) (quic_congestion_module_t *const module, uint64_t id);
    quic_err_t (*new_instance) (quic_congestion_module_t *const module, const uint64_t key);

    uint8_t instance[0];
};

#define quic_congestion_update(module, recv_time, sent_time, ack_delay)    \
    if ((module)->update) {                                                \
        (module)->update((module), (recv_time), (sent_time), (ack_delay)); \
    }

#define quic_congestion_on_sent(module, sent_time, num, sent_bytes, include_unacked)      \
    if ((module)->on_sent) {                                                              \
        (module)->on_sent((module), (sent_time), (num), (sent_bytes), (include_unacked)); \
    }

#define quic_congestion_on_acked(module, num, acked_bytes, unacked_bytes, event_time)      \
    if ((module)->on_acked) {                                                              \
        (module)->on_acked((module), (num), (acked_bytes), (unacked_bytes), (event_time)); \
    }

#define quic_congestion_on_lost(module, num, lost_bytes, unacked_bytes)     \
    if ((module)->on_lost) {                                                \
        (module)->on_lost((module), (num), (lost_bytes), (unacked_bytes));  \
    }

#define quic_congestion_next_send_time(module, unacked_bytes) \
    ((module)->next_send_time ? (module)->next_send_time((module), (unacked_bytes)) : 0)

#define quic_congestion_allow_send(module, unacked_bytes) \
    ((module)->allow_send ? (module)->allow_send((module), (unacked_bytes)) : false)

#define quic_congestion_has_budget(module) \
    ((module)->has_budget ? (module)->has_budget((module)) : false)

#define quic_congestion_id(module) \
    ((module)->id ? (module)->id((module)) : 0);

#define quic_congestion_store(module) \
    ((module)->store ? (module)->store((module)) : quic_err_not_implemented)

#define quic_congestion_migrate(module, id) \
    ((module)->migrate ? (module)->migrate((module), id) : quic_err_not_implemented)

#define quic_congestion_new_instance(module, key) \
    ((module)->new_instance ? (module)->new_instance((module), (key)) : quic_err_not_implemented)

#define quic_congestion_pto(module, max_delay) \
    ((module)->pto ? (module)->pto((module), (max_delay)) : quic_err_not_implemented)

#define quic_congestion_smoothed_rtt(module) \
    ((module)->smoothed_rtt ? (module)->smoothed_rtt(module) : 0)

extern quic_module_t quic_congestion_module;

#endif
