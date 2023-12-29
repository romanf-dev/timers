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
#define list_last(head) ((head)->prev)
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

struct timer_context_t {
    struct list_t subscribers[NQUEUE];
    timeout_t ticks;
};

struct timer_t {
    struct timer_context_t* parent;
    void (*func)(struct timer_t* self, void* arg);
    void* arg;
    timeout_t timeout;
    struct list_t link;
};

void timer_context_init(struct timer_context_t* context) {
    assert(context != 0);
    context->ticks = 0;

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

/*
 * Returns the index of the most significant bit which is different between
 * old and new values if the bit is in range 0..NQUEUE. Otherwise returns
 * maximum index NQUEUE-1.
 */
static inline unsigned diff_msb(timeout_t oldvalue, timeout_t newvalue) {
    assert(oldvalue != newvalue);
    const timeout_t diff_bits = newvalue ^ oldvalue;
    const timeout_t mask = (1U << NQUEUE) - 1;
    const timeout_t lo = diff_bits & mask;
    const timeout_t hi = diff_bits & ~mask;
    const unsigned width = sizeof(timeout_t) * CHAR_BIT;
    const unsigned msb = width - CLZ(lo) - 1;
    return hi ? NQUEUE - 1 : msb;
}

/*
 * When the delay is added to the current tick counter, some bits of the latter
 * are changed. Timer will not expire until all the different bits flip, so
 * insert the timer in the queue corresponding to the most significant one.
 */
void timer_set(struct timer_t* timer, const timeout_t delay) {
    assert((delay != 0) && (delay < INT32_MAX));
    struct timer_context_t* const context = timer->parent;
    const timeout_t timeout = context->ticks + delay;
    const unsigned qindex = diff_msb(context->ticks, timeout);
    timer->timeout = timeout;
    list_append(&context->subscribers[qindex], &timer->link);
}

/*
 * Determine the most significant changed bit between old and new tick counter
 * values. Examine timers which are in the corresponding queue. Since timers
 * with large timeouts may be re-inserted into the same queue, we should handle
 * only the timers which were inserted before this function call.
 */
void timer_context_tick(struct timer_context_t* context) {
    const unsigned int oldticks = context->ticks++;
    const unsigned int qindex = diff_msb(oldticks, context->ticks);
    struct list_t* const last = list_last(&context->subscribers[qindex]);

    while (!list_empty(&context->subscribers[qindex])) {
        struct list_t* const head = list_first(&context->subscribers[qindex]);
        struct timer_t* const timer = list_entry(head, struct timer_t, link);
        list_unlink(head);

        if (timer->timeout == context->ticks) {
            timer->func(timer, timer->arg);
        } else {
            const unsigned qnext = diff_msb(timer->timeout, context->ticks);
            list_append(&context->subscribers[qnext], &timer->link);
        }

        if (head == last) {
            break;
        }
    }
}

#endif

