Radix timers: algorithm for timer facility in embedded systems
==============================================================


Radix timers overview
---------------------

The algorithm uses N queues which are correspond to lowest bits of the tick counter. When a timer is started, its target timeout is XOR-ed with the current tick counter and the timer is placed in the queue corresponding to the most significant different bit. Tick counter may be seen as a set of bits where each bit flips with different frequency, so timers with large timeouts (and high different bits) are visited rarely.

On each tick some bit of the tick counter flips from 0 to 1 (except the overflow case). The corresponding queue therefore contains the timers which are 'subscribed' to the bit change. All the timers in the queue should be visited and some of them whose timeout is not reached yet have to be re-inserted into other queues.

This scheme is better than various implementations with sorted lists since it allows for O(1) interrupt latency, since all the processing may be performed step-by-step with interrupt windows.


Example
-------

timer.c contains the algorithm implementation in C. Use the following command line to build:

        gcc -o test timers.c


The example implements simple delay using 10ms ticks. It accepts delay value in 10ms intervals as its first parameter argv[1].

