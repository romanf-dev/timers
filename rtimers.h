/*
 * Copyright (c) 2023 romanf
 * Example application for radix timers.
 * License: 2-clause BSD.
 */

#ifndef _RTIMERS_H_
#define _RTIMERS_H_

#ifndef NQUEUE
#define NQUEUE 10
#endif

#if defined (__GNUC__)
#define CLZ(x) __builtin_clz(x)
#else
#error TODO: CLZ is not available out of the box...
#endif

struct list_t {
    struct list_t* next;
    struct list_t* prev;
};

#define list_init(head) ((head)->next = (head)->prev = (head))
#define list_empty(head) ((head)->next == (head))
#define list_first(head) ((head)->next)
#define list_entry(p, type, member) \
    ((type*)((char*)(p) - (size_t)(&((type*)0)->member)))

static inline void 
list_append(struct list_t* head, struct list_t* node) {
    assert((head->next != 0) && (head->prev != 0));
    node->next = head;
    node->prev = head->prev;
    node->prev->next = node;
    head->prev = node;
}

static inline void list_unlink(struct list_t* node) {
    assert((node->next != 0) && (node->prev != 0));
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node->prev = 0;
}

typedef uint32_t timeout_t;
typedef uint32_t gray_code_t;

struct timer_context_t {
    struct list_t subscribers[NQUEUE];
    timeout_t ticks;
    gray_code_t gray_ticks; /* Equal to ticks but in Gray code. */
};

struct timer_t {
    struct timer_context_t* parent;
    void (*func)(struct timer_t* self, void* arg);
    void* arg;
    gray_code_t gray_timeout;
    struct list_t link;
};

void timer_context_init(struct timer_context_t* context) {
    assert(context != 0);
    context->ticks = 0;
    context->gray_ticks = 0;

    for (unsigned i = 0; i < NQUEUE; ++i) {
        list_init(&context->subscribers[i]);
    }
}

void timer_init(
    struct timer_t* timer, 
    struct timer_context_t* context, 
    void (*f)(struct timer_t*, void*),
    void* arg
) {
    assert((timer != 0) && (f != 0));
    timer->parent = context;
    timer->func = f;
    timer->arg = arg;
}

static inline gray_code_t to_gray_code(timeout_t x) {
    return x ^ (x >> 1);
}

/*
 * Return the most significant different bit between two values.
 * Since x != y, this bit is always exist.
 */
static inline unsigned diff_msb(gray_code_t x, gray_code_t y) {
    assert(x != y);
    const gray_code_t diff_bits = x ^ y;
    const unsigned int width = sizeof(gray_code_t) * CHAR_BIT;
    const unsigned int msb = width - CLZ(diff_bits) - 1;
    return (msb < NQUEUE) ? msb : NQUEUE - 1;
}

/*
 * Inserts the timer into queue corresponding to the most significant bit
 * that is different between Gray-encoded timeout of the timer and the
 * Gray-encoded current tick counter.
 * This function has O(1) time to complete.
 */
void timer_set(struct timer_t* timer, const timeout_t delay) {
    assert((delay != 0) && (delay < INT32_MAX));
    struct timer_context_t* const context = timer->parent;
    const timeout_t timeout = context->ticks + delay;
    const gray_code_t gray_timeout = to_gray_code(timeout);
    const unsigned qindex = diff_msb(context->gray_ticks, gray_timeout);
    timer->gray_timeout = gray_timeout;
    list_append(&context->subscribers[qindex], &timer->link);
}

/* 
 * On each tick exactly one bit may be changed (Hamming distance between two 
 * consecutive Gray-encoded value is 1). The corresponding queue N contains 
 * timers that wait until the N bit changes, so there are two possibility for 
 * each timer: it is either reached its timeout or should be moved to another
 * queue in case when there are other bits that are different between target
 * timeout and the tick counter.
 * While this function requires O(n) time to complete, it may be implemented to
 * have O(1) interrupt latency unlike the case when all the timers reside in
 * sorted queues. Doubly-linked next/prev list may be copied to local list
 * and further procesing may be performed step-by-step with interrupt window.
 */
void timer_context_tick(struct timer_context_t* context) {
    const gray_code_t new_gray_ticks = to_gray_code(++context->ticks);
    const unsigned int qindex = diff_msb(context->gray_ticks, new_gray_ticks);
    context->gray_ticks = new_gray_ticks;

    while (!list_empty(&context->subscribers[qindex])) {
        struct list_t* const head = list_first(&context->subscribers[qindex]);
        struct timer_t* const timer = list_entry(head, struct timer_t, link);
        list_unlink(head);

        if (timer->gray_timeout == new_gray_ticks) {
            timer->func(timer, timer->arg);
        } else {
            const unsigned qnext = diff_msb(timer->gray_timeout, new_gray_ticks);
            list_append(&context->subscribers[qnext], &timer->link);
        }
    }
}

#endif

