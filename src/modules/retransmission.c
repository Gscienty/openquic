/*
 * Copyright (c) 2020 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#include "modules/retransmission.h"
#include "modules/congestion.h"
#include "format/header.h"
#include "utils/time.h"
#include "session.h"

typedef struct quic_dropped_pkt_s quic_dropped_pkt_t;
struct quic_dropped_pkt_s {
    QUIC_LINK_FIELDS

    quic_sent_packet_rbt_t *pkt;
};

static quic_err_t quic_retransmission_module_init(void *const module);
static quic_err_t quic_retransmission_module_loop(void *const module, const uint64_t now);

static quic_err_t quic_retransmission_drop_execute(quic_link_t *const link, quic_rbt_t **const root);

static inline quic_err_t quic_retransmission_drop(quic_link_t *const link, quic_sent_packet_rbt_t *const pkt) {
    quic_dropped_pkt_t *const dropped = malloc(sizeof(quic_dropped_pkt_t));
    if (dropped) {
        quic_link_init(dropped);
        dropped->pkt = pkt;
        quic_link_insert_after(link, dropped);
    }
    return quic_err_success;
}

static quic_err_t quic_retransmission_drop_execute(quic_link_t *const list, quic_rbt_t **const root) {
    while (!quic_link_empty(list)) {
        quic_dropped_pkt_t *node = (quic_dropped_pkt_t *) quic_link_next(list);
        quic_sent_packet_rbt_t *pkt = node->pkt;
        quic_link_remove(node);
        free(node);

        quic_rbt_remove(root, &pkt);
        free(pkt);
    }

    return quic_err_success;
}

quic_err_t quic_retransmission_module_find_newly_acked(quic_retransmission_module_t *const module, const quic_frame_ack_t *const frame) {
    quic_session_t *const session = quic_module_of_session(module);
    quic_congestion_module_t *const c_module = quic_session_module(quic_congestion_module_t, session, quic_congestion_module);

    quic_sent_packet_rbt_t *pkt = NULL;
    quic_link_t acked_list;

    quic_link_init(&acked_list);

    {
        quic_rbt_foreach(pkt, module->sent_mem) {
            uint64_t end = frame->largest_ack;
            uint64_t start = end - frame->first_range;

            bool lost_flag = frame->ranges.count != 0;
            uint32_t i = 0;
            do {
                if (start <= pkt->key && pkt->key <= end) {
                    quic_retransmission_drop(&acked_list, pkt);

                    module->sent_pkt_count--;
                    if (pkt->included_unacked) {
                        module->unacked_len -= pkt->pkt_len;
                        quic_congestion_on_acked(c_module, pkt->key, pkt->pkt_len, module->unacked_len, frame->recv_time);
                    }

                    while (!quic_link_empty(&pkt->frames)) {
                        quic_frame_t *acked_frame = (quic_frame_t *) quic_link_next(&pkt->frames);
                        quic_link_remove(acked_frame);

                        if (acked_frame->on_acked) {
                            quic_frame_on_acked(acked_frame);
                        }
                        else {
                            free(acked_frame);
                        }
                    }
                }

                if (lost_flag) {
                    end = start - quic_arr(&frame->ranges, i, quic_ack_range_t)->gap - 2;
                    start = end - quic_arr(&frame->ranges, i, quic_ack_range_t)->len;
                }
            } while (++i < frame->ranges.count);
        }
    }
    quic_retransmission_drop_execute(&acked_list, (quic_rbt_t **) &module->sent_mem);
    return quic_err_success;
}

quic_err_t quic_retransmission_module_find_newly_lost(quic_retransmission_module_t *const module) {
    quic_session_t *const session = quic_module_of_session(module);
    quic_congestion_module_t *const c_module = quic_session_module(quic_congestion_module_t, session, quic_congestion_module);

    quic_link_t lost_list;
    quic_sent_packet_rbt_t *pkt = NULL;

    quic_link_init(&lost_list);

    module->loss_time = 0;
    uint64_t max_rtt = session->rtt.smoothed_rtt;
    uint64_t lost_delay = (9 * max_rtt) >> 3;
    lost_delay = lost_delay > 1000 ? lost_delay : 1000;
    uint64_t lost_send_time = quic_now() - lost_delay;

    {
        quic_rbt_foreach(pkt, module->sent_mem) {
            if (pkt->sent_time < lost_send_time) {
                quic_retransmission_drop(&lost_list, pkt);

                module->sent_pkt_count--;
                if (pkt->included_unacked) {
                    module->unacked_len -= pkt->pkt_len;
                    quic_congestion_on_lost(c_module, pkt->key, pkt->pkt_len, module->unacked_len);
                }

                while (!quic_link_empty(&pkt->frames)) {
                    quic_frame_t *lost_frame = (quic_frame_t *) quic_link_next(&pkt->frames);
                    quic_link_remove(lost_frame);

                    if (lost_frame->on_lost) {
                        quic_frame_on_lost(lost_frame);
                    }
                    else {
                        free(lost_frame);
                    }
                }
            }
            else if (!module->loss_time || pkt->sent_time + lost_delay < module->loss_time) {
                module->loss_time = pkt->sent_time + lost_delay;
            }
        }
    }
    quic_retransmission_drop_execute(&lost_list, (quic_rbt_t **) &module->sent_mem);
    return quic_err_success;
}

uint64_t quic_retransmission_append_frame(quic_link_t *const frames, const uint64_t capa, quic_retransmission_module_t *const module) {
    uint64_t len = 0;
    quic_frame_t *frame = NULL;

    quic_link_foreach(frame, &module->retransmission_queue) {
        len = quic_frame_size(frame);
        if (len > capa) {
            len = 0;
            continue;
        }
        quic_link_remove(frame);
        quic_link_insert_before(frames, frame);
        break;
    }

    return len;
}

static quic_err_t quic_retransmission_module_init(void *const module) {
    quic_retransmission_module_t *const r_module = (quic_retransmission_module_t *) module;

    quic_rbt_tree_init(r_module->sent_mem);

    r_module->sent_pkt_count = 0;
    r_module->unacked_len = 0;

    r_module->max_delay = 0;

    r_module->loss_time = 0;
    r_module->last_sent_ack_time = 0;
    r_module->largest_ack = 0;

    r_module->alarm = 0;

    quic_link_init(&r_module->retransmission_queue);

    return quic_err_success;
}

static quic_err_t quic_retransmission_module_loop(void *const module, const uint64_t now) {
    quic_session_t *const session = quic_module_of_session(module);
    quic_retransmission_module_t *const r_module = (quic_retransmission_module_t *) module;

    if (r_module->alarm == 0) {
        return quic_err_success;
    }
    if (now < r_module->alarm) {
        quic_session_update_loop_deadline(session, r_module->alarm);
        return quic_err_success;
    }

    if (r_module->unacked_len) {
        quic_retransmission_module_find_newly_lost(r_module);
    }
    quic_retransmission_update_alarm(r_module);

    return quic_err_success;
}

quic_module_t quic_initial_retransmission_module = {
    .name        = "initial_retransmission",
    .module_size = sizeof(quic_retransmission_module_t),
    .init        = quic_retransmission_module_init,
    .process     = NULL,
    .loop        = quic_retransmission_module_loop,
    .destory     = NULL
};

quic_module_t quic_handshake_retransmission_module = {
    .name        = "handshake_retransmission",
    .module_size = sizeof(quic_retransmission_module_t),
    .init        = quic_retransmission_module_init,
    .process     = NULL,
    .loop        = quic_retransmission_module_loop,
    .destory     = NULL
};

quic_module_t quic_app_retransmission_module = {
    .name        = "app_retransmission",
    .module_size = sizeof(quic_retransmission_module_t),
    .init        = quic_retransmission_module_init,
    .process     = NULL,
    .loop        = quic_retransmission_module_loop,
    .destory     = NULL
};

quic_err_t quic_session_handle_ack_frame(quic_session_t *const session, const quic_frame_t *const frame) {
    const quic_frame_ack_t *const ack_frame = (const quic_frame_ack_t *) frame;
    quic_retransmission_module_t *r_module = NULL;
    quic_congestion_module_t *const c_module = quic_session_module(quic_congestion_module_t, session, quic_congestion_module);

    switch (ack_frame->packet_type) {
    case quic_packet_initial_type:
        r_module = quic_session_module(quic_retransmission_module_t, session, quic_initial_retransmission_module);
        break;
    case quic_packet_handshake_type:
        r_module = quic_session_module(quic_retransmission_module_t, session, quic_handshake_retransmission_module);
        break;
    case quic_packet_short_type:
        r_module = quic_session_module(quic_retransmission_module_t, session, quic_app_retransmission_module);
        break;
    default:
        return quic_err_internal_error;
    }

    r_module->largest_ack = r_module->largest_ack > ack_frame->largest_ack ? r_module->largest_ack : ack_frame->largest_ack;

    quic_sent_packet_rbt_t *pkt = quic_sent_pkts_find(r_module->sent_mem, &ack_frame->largest_ack);
    if (!quic_rbt_is_nil(pkt)) {
        uint64_t delay = 0;
        if (ack_frame->packet_type == quic_packet_short_type) {
            delay = ack_frame->delay < r_module->max_delay ? ack_frame->delay : r_module->max_delay;
        }

        quic_congestion_update(c_module, ack_frame->recv_time, pkt->sent_time, delay);
    }

    quic_retransmission_module_find_newly_acked(r_module, ack_frame);
    quic_retransmission_module_find_newly_lost(r_module);

    quic_retransmission_update_alarm(r_module);

    return quic_err_success;
}
