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
#include "rtimers.h"

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

