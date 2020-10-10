/*
 * Copyright (c) 2020 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#ifndef __OPENQUIC_FLOWCTRL_H__
#define __OPENQUIC_FLOWCTRL_H__

#include "utils/errno.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct quic_flowctrl_s quic_flowctrl_t;

typedef struct quic_flowctrl_module_s quic_flowctrl_module_t;
struct quic_flowctrl_module_s {
    quic_err_t (*init) (quic_flowctrl_t *const flowctrl);
    bool (*is_newly_blocked) (uint64_t *const swnd, quic_flowctrl_t *const flowctrl);
    void (*update_swnd) (quic_flowctrl_t *const flowctrl, const uint64_t swnd);
    uint64_t (*get_swnd) (quic_flowctrl_t *const flowctrl);
    void (*sent_bytes) (quic_flowctrl_t *const flowctrl, const uint64_t bytes);
    void (*update_rwnd) (quic_flowctrl_t *const flowctrl, const uint64_t rwnd, const bool fin);
    bool (*abandon) (quic_flowctrl_t *const flowctrl);
    void (*recv_bytes) (quic_flowctrl_t *const flowctrl, const uint64_t bytes);
};

extern quic_flowctrl_module_t quic_flowctrl_module;

#endif
