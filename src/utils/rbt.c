/*
 * Copyright (c) 2020 Gscienty <gaoxiaochuan@hotmail.com>
 *
 * Distributed under the MIT software license, see the accompanying
 * file LICENSE or https://www.opensource.org/licenses/mit-license.php .
 *
 */

#include "utils/rbt.h"

const quic_rbt_t rbt_nil = {
    .rb_p = quic_rbt_nil,
    .rb_r = quic_rbt_nil,
    .rb_l = quic_rbt_nil,
    .rb_color = QUIC_RBT_BLACK
};

static inline void __rbt_lr(quic_rbt_t **const root, quic_rbt_t *const node);
static inline void __rbt_rr(quic_rbt_t **const root, quic_rbt_t *const node);
static void __rbt_fixup(quic_rbt_t **const root, quic_rbt_t *node);
static inline void __rbt_assign(quic_rbt_t **const root, quic_rbt_t *const target, quic_rbt_t *const ref);
static inline void __rbt_del_case1(quic_rbt_t **const root, quic_rbt_t *const node);
static inline void __rbt_del_case2(quic_rbt_t **const root, quic_rbt_t *const node);
static inline void __rbt_del_case3(quic_rbt_t **const root, quic_rbt_t *const node);
static inline void __rbt_del_case4(quic_rbt_t **const root, quic_rbt_t *const node);
static inline void __rbt_del_case5(quic_rbt_t **const root, quic_rbt_t *const node);
static inline void __rbt_del_case6(quic_rbt_t **const root, quic_rbt_t *const node);
static inline quic_rbt_t *__sibling(const quic_rbt_t *const node);
static quic_rbt_t *__rbt_min(quic_rbt_t *node);

quic_err_t quic_rbt_insert(quic_rbt_t **const root, quic_rbt_t *const node, quic_rbt_comparer_t comparer) {
    quic_rbt_t *rb_p = quic_rbt_nil;
    quic_rbt_t **in = root;

    while (*in != quic_rbt_nil) {
        rb_p = *in;
        switch (comparer(node, rb_p)) {
        case QUIC_RBT_EQ:
            return quic_err_conflict;
        case QUIC_RBT_LS:
            in = &rb_p->rb_l;
            break;
        case QUIC_RBT_GT:
            in = &rb_p->rb_r;
            break;
        }
    }
    node->rb_p = rb_p;
    *in = node;

    __rbt_fixup(root, node);
    return quic_err_success;
}

quic_err_t quic_rbt_remove(quic_rbt_t **const root, quic_rbt_t **const node) {
    quic_rbt_t *ref = *node;
    if (ref->rb_l != quic_rbt_nil && ref->rb_r != quic_rbt_nil) {
        quic_rbt_t *next = __rbt_min(ref->rb_r);
        quic_rbt_t tmp;
        __rbt_assign(root, &tmp, next);
        __rbt_assign(root, next, ref);
        __rbt_assign(root, ref, &tmp);
    }
    quic_rbt_t *child = ref->rb_l == quic_rbt_nil ? ref->rb_r : ref->rb_l;
    if (ref->rb_color == QUIC_RBT_BLACK) {
        ref->rb_color = child->rb_color;
        __rbt_del_case1(root, ref);
    }

    if (ref->rb_p == quic_rbt_nil && child != quic_rbt_nil) {
        child->rb_color = QUIC_RBT_BLACK;
    }
    *node = ref;

    return quic_err_success;
}

quic_rbt_t *quic_rbt_find(quic_rbt_t *const root, const void *const key, quic_rbt_key_comparer_t comparer) {
    quic_rbt_t *ret = root;
    while (ret != quic_rbt_nil) {
        switch (comparer(key, ret)) {
        case QUIC_RBT_EQ:
            return ret;
        case QUIC_RBT_LS:
            ret = ret->rb_l;
            break;
        case QUIC_RBT_GT:
            ret = ret->rb_r;
            break;
        }
    }

    return quic_rbt_nil;
}

static inline void __rbt_lr(quic_rbt_t **const root, quic_rbt_t *const node) {
    quic_rbt_t *child;

    child = node->rb_r;
    node->rb_r = child->rb_l;
    if (child->rb_l != quic_rbt_nil) {
        child->rb_l->rb_p = node;
    }
    child->rb_p = node->rb_p;
    if (node->rb_p == quic_rbt_nil) {
        *root = child;
    }
    else if (node == node->rb_p->rb_l) {
        node->rb_p->rb_l = child;
    }
    else {
        node->rb_p->rb_r = child;
    }
    child->rb_l = node;
    node->rb_p = child;
}

static inline void __rbt_rr(quic_rbt_t **const root, quic_rbt_t *const node) {
    quic_rbt_t *child;

    child = node->rb_l;
    node->rb_l = child->rb_r;
    if (child->rb_r != quic_rbt_nil) {
        child->rb_r->rb_p = node;
    }
    child->rb_p = node->rb_p;
    if (node->rb_p == quic_rbt_nil) {
        *root = child;
    }
    else if (node == node->rb_p->rb_l) {
        node->rb_p->rb_l = child;
    }
    else {
        node->rb_p->rb_r = child;
    }
    child->rb_r = node;
    node->rb_p = child;
}

static void __rbt_fixup(quic_rbt_t **const root, quic_rbt_t *node) {
    quic_rbt_t *uncle;

    while (node->rb_p->rb_color == QUIC_RBT_RED) {
        if (node->rb_p == node->rb_p->rb_p->rb_l) {
            uncle = node->rb_p->rb_p->rb_r;
            if (uncle->rb_color == QUIC_RBT_RED) {
                uncle->rb_color = QUIC_RBT_BLACK;
                node->rb_p->rb_color = QUIC_RBT_BLACK;
                node->rb_p->rb_p->rb_color = QUIC_RBT_RED;
                node = node->rb_p->rb_p;
            }
            else {
                if (node == node->rb_p->rb_r) {
                    node = node->rb_p;
                    __rbt_lr(root, node);
                }
                node->rb_p->rb_color = QUIC_RBT_BLACK;
                node->rb_p->rb_p->rb_color = QUIC_RBT_RED;
                __rbt_rr(root, node->rb_p->rb_p);
            }
        }
        else {
            uncle = node->rb_p->rb_p->rb_l;
            if (uncle->rb_color == QUIC_RBT_RED) {
                uncle->rb_color = QUIC_RBT_BLACK;
                node->rb_p->rb_color = QUIC_RBT_BLACK;
                node->rb_p->rb_p->rb_color = QUIC_RBT_RED;
                node = node->rb_p->rb_p;
            }
            else {
                if (node == node->rb_p->rb_l) {
                    node = node->rb_p;
                    __rbt_rr(root, node);
                }
                node->rb_p->rb_color = QUIC_RBT_BLACK;
                node->rb_p->rb_p->rb_color = QUIC_RBT_RED;
                __rbt_lr(root, node->rb_p->rb_p);
            }
        }
    }
    (*root)->rb_color = QUIC_RBT_BLACK;
}

static inline void __rbt_assign(quic_rbt_t **const root, quic_rbt_t *const target, quic_rbt_t *const ref) {
    target->rb_color = ref->rb_color;
    target->rb_l = ref->rb_l;
    target->rb_r = ref->rb_r;
    target->rb_p = ref->rb_p;

    if (ref->rb_p == quic_rbt_nil) {
        *root = target;
    }
    else if (ref->rb_p->rb_l == ref) {
        ref->rb_p->rb_l = target;
    }
    else {
        ref->rb_p->rb_r = target;
    }

    if (ref->rb_l != quic_rbt_nil) {
        ref->rb_l->rb_p = target;
    }
    if (ref->rb_r != quic_rbt_nil) {
        ref->rb_r->rb_p = target;
    }
}

static inline void __rbt_del_case1(quic_rbt_t **const root, quic_rbt_t *const node) {
    if (node->rb_p != quic_rbt_nil) {
        __rbt_del_case2(root, node);
    }
}

static inline void __rbt_del_case2(quic_rbt_t **const root, quic_rbt_t *const node) {
    quic_rbt_t *sibling = __sibling(node);
    if (sibling->rb_color == QUIC_RBT_RED) {
        node->rb_p->rb_color = QUIC_RBT_RED;
        sibling->rb_color = QUIC_RBT_BLACK;
        if (node == node->rb_p->rb_l) {
            __rbt_lr(root, node->rb_p);
        }
        else {
            __rbt_rr(root, node->rb_p);
        }
    }
    __rbt_del_case3(root, node);
}

static inline void __rbt_del_case3(quic_rbt_t **const root, quic_rbt_t *const node) {
    quic_rbt_t *sibling = __sibling(node);
    if (node->rb_p->rb_color == QUIC_RBT_BLACK &&
        sibling->rb_color == QUIC_RBT_BLACK &&
        sibling->rb_l->rb_color == QUIC_RBT_BLACK &&
        sibling->rb_r->rb_color == QUIC_RBT_BLACK &&
        sibling->rb_color == QUIC_RBT_RED) {
        __rbt_del_case1(root, node);
    }
    else {
        __rbt_del_case4(root, node);
    }
}

static inline void __rbt_del_case4(quic_rbt_t **const root, quic_rbt_t *const node) {
    quic_rbt_t *sibling = __sibling(node);
    if (node->rb_p->rb_color == QUIC_RBT_RED &&
        sibling->rb_color == QUIC_RBT_BLACK &&
        sibling->rb_l->rb_color == QUIC_RBT_BLACK &&
        sibling->rb_r->rb_color == QUIC_RBT_BLACK) {
        node->rb_p->rb_color = QUIC_RBT_BLACK;
    }
    else {
        __rbt_del_case5(root, node);
    }
}

static inline void __rbt_del_case5(quic_rbt_t **const root, quic_rbt_t *const node) {
    quic_rbt_t *sibling = __sibling(node);
    if (node->rb_p->rb_l == node &&
        sibling->rb_color == QUIC_RBT_BLACK &&
        sibling->rb_l->rb_color == QUIC_RBT_RED &&
        sibling->rb_r->rb_color == QUIC_RBT_BLACK) {
        sibling->rb_color = QUIC_RBT_RED;
        sibling->rb_l->rb_color = QUIC_RBT_BLACK;
        __rbt_rr(root, sibling);
    }
    else if (node->rb_p->rb_r == node &&
             sibling->rb_color == QUIC_RBT_BLACK &&
             sibling->rb_r->rb_color == QUIC_RBT_RED &&
             sibling->rb_l->rb_color == QUIC_RBT_BLACK) {
        sibling->rb_color = QUIC_RBT_RED;
        sibling->rb_r->rb_color = QUIC_RBT_BLACK;
        __rbt_lr(root, sibling);
    }
    __rbt_del_case6(root, node);
}

static inline void __rbt_del_case6(quic_rbt_t **const root, quic_rbt_t *const node) {
    quic_rbt_t *sibling = __sibling(node);
    sibling->rb_color = QUIC_RBT_BLACK;
    node->rb_p->rb_color = node->rb_p->rb_color;
    if (node == node->rb_p->rb_l) {
        sibling->rb_r->rb_color = QUIC_RBT_BLACK;
        __rbt_lr(root, node->rb_p);
    }
    else {
        sibling->rb_l->rb_color = QUIC_RBT_BLACK;
        __rbt_rr(root, node->rb_p);
    }
}

static inline quic_rbt_t *__sibling(const quic_rbt_t *const node) {
    if (node->rb_p->rb_l == node) {
        return node->rb_p->rb_r;
    }
    else {
        return node->rb_p->rb_l;
    }
}

static quic_rbt_t *__rbt_min(quic_rbt_t *node) {
    if (node == quic_rbt_nil) {
        return quic_rbt_nil;
    }

    while (node->rb_l != quic_rbt_nil) {
        node = node->rb_l;
    }

    return node;
}
