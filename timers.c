/*
 * Copyright (c) 2023 romanf
 * Example application for radix timers.
 * License: 2-clause BSD.
 */

#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define NQUEUE 10

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
    struct list_t timers[NQUEUE];
    timeout_t ticks;
    gray_code_t gticks;
};

struct timer_t {
    struct timer_context_t* parent;
    void (*func)(struct timer_t* self, void* arg);
    void* arg;
    gray_code_t gtimeout;
    struct list_t link;
};

static inline gray_code_t to_gray_code(timeout_t x) {
    return x ^ (x >> 1);
}

static inline unsigned int diff_msb(gray_code_t x, gray_code_t y) {
    assert(x != y);
    const gray_code_t diff_bits = x ^ y;
    const unsigned int word_width = sizeof(gray_code_t) * CHAR_BIT;
    const unsigned int msb = word_width - CLZ(diff_bits) - 1;
    return (msb < NQUEUE) ? msb : NQUEUE - 1;
}

void timer_context_init(struct timer_context_t* context) {
    assert(context != 0);
    context->ticks = 0;
    context->gticks = 0;

    for (unsigned int i = 0; i < NQUEUE; ++i) {
        list_init(&context->timers[i]);
    }
}

void timer_init(
    struct timer_t* timer, 
    struct timer_context_t* context, 
    void (*f)(struct timer_t*, void*),
    void* arg) {

    assert((timer != 0) && (f != 0));
    timer->parent = context;
    timer->func = f;
    timer->arg = arg;
}

void timer_set(struct timer_t* timer, const timeout_t timeout) {
    assert((timeout != 0) && (timeout < INT32_MAX));
    struct timer_context_t* const context = timer->parent;
    const timeout_t abs_timeout = context->ticks + timeout;
    const gray_code_t abs_gtimeout = to_gray_code(abs_timeout);
    const unsigned int qindex = diff_msb(context->gticks, abs_gtimeout);
    timer->gtimeout = abs_gtimeout;
    list_append(&context->timers[qindex], &timer->link);
}

void timer_context_tick(struct timer_context_t* context) {
    const timeout_t current_tick = ++context->ticks;
    const gray_code_t new_gticks = to_gray_code(current_tick);
    const unsigned int qindex = diff_msb(context->gticks, new_gticks);
    context->gticks = new_gticks;

    while (!list_empty(&context->timers[qindex])) {
        struct list_t* const head = list_first(&context->timers[qindex]);
        struct timer_t* const timer = list_entry(head, struct timer_t, link);
        list_unlink(head);

        if (timer->gtimeout == new_gticks) {
            timer->func(timer, timer->arg);
        } else {
            const unsigned int qnext = diff_msb(timer->gtimeout, new_gticks);
            list_append(&context->timers[qnext], &timer->link);
        }
    }
}

static struct timer_context_t g_context;

static void callback(struct timer_t* self, void* arg) {
    int* flag = arg;
    *flag = 1;
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printf("Please, specify timeout value in 10ms intervals.\r\n");
        return EXIT_FAILURE;
    }

    const timeout_t tout = atoi(argv[1]);

    if (tout == 0 || tout > INT32_MAX) {
        printf("Timeout must be 0 < t < INT32_MAX\r\n");
        return EXIT_FAILURE;
    }

    struct timer_t timer;
    int stop_flag = 0;

    timer_context_init(&g_context);
    timer_init(&timer, &g_context, callback, &stop_flag);
    timer_set(&timer, tout);

    while(!stop_flag) {
        usleep(10000); /* 10ms tick period */
        timer_context_tick(&g_context);        
    }

    return 0;
}

