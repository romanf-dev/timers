Radix timers: algorithm for timer facility in embedded systems
==============================================================


Intro
-----

There is a great [paper](http://www.cs.columbia.edu/~nahum/w6998/papers/sosp87-timing-wheels.pdf) describing various strategies of timer implementation. The brief summary is that there is the following common algorithms used:

1. Straightforward - single unordered list of timers, O(n) per tick bookkeeping
2. Ordered list - single ordered list, O(n) on insertion
3. Tree-based algorithms - O(1) or O(log) on all operations, requires expensive balanced trees
4. With MAX intervals - suitable when maximum interval is limited, which is not the common case
5. Hash table with sorted lists in each bucket - requires sorting as in the case 2.
6. Hash table with unsorted lists in each bucket - redistribution of the timers in given bucket on each tick
7. Hierarchy-based - hierarchy of timers depending on their intervals

The most popular schemes are 2, 6 and 7. Authors recommend schemes 6 and 7 for general-purpose OS timers.


Scheme 6/7 pros and cons
------------------------

The main disadvantage of the scheme 6 is the requirement to check every timer in the system within period corresponding to the timer's list array size (number of buckets). This may be inappropriate when average timer interval is large. For example, if array size N = 32 and system tick interval is 1ms then 1-second timer will be moved between buckets 31 times (1000/32). On the other hand, good point about scheme 6 is that it has to check exactly one list in every tick, while scheme 7 may need to  check up to H lists where H is the number of hierarchy levels.

Scheme 7 exploits hierarchy and this results in significantly less unneccessary lookups, especially in case when the average timeout is large. While the paper illustrates scheme 7 using hour/min/sec analogy it would be more convenient to use binary intervals as the basis for the hierarchy: 1-tick intervals, 16-tick intervals, etc. In this case the timeout value may be split into several parts where each part corresponds to the number of buckets at each hierarchy level.
For example timeout value with three levels:

        31                                 0
        00000000 00000000 00000000 0000 0000
                                -- ---- ----
                                 |   |    |
                                 |   |    -----> 1st level index (4 bits for 16 buckets, 1 tick interval)
                                 |   ----------> 2nd level index (4 bits, 16-tick interval)
                                 --------------> 3rd level index (2 bits, 256-tick interval)

Every time when tick is incremented, some index is changed, the corresponding list is evaluated (and contained timers are possibly re-distributed to lower levels). Unfortunately, large multilevel hierarchies require many lists and that wastes too much memory. When the timeout range is fully covered by the hierarchy this is effectively reduced into scheme 4. In the simplest case when there is only one list this scheme is reduced to 6th case. Also, because of binary number's nature, when overflow happens (every 1024th tick in the example above because of 10 bit indexing (4+4+2)) we have to check all the queues and therefore all active timers, which is disadvantageous compared to scheme 6.


Radix timers overview
---------------------

One possible optimization for scheme 7 is to avoid multiple lists at each hierarchy level. Instead, lists are mapped to some amount of lowest bits in 1:1 manner. When a timer is started, its target timeout is XOR-ed with the current tick counter and the timer is placed in the list corresponding to the most significant bit of the xor result. Tick counter may be seen as a set of bits where each bit flips with different frequency, so timers with large timeouts (and high different bits) are visited rarely.

However, this algorithm suffers from the same issue as scheme 7: when the system tick counter overflows we have to check all the queues and timers. Also, the code is more complex compared to scheme 6 since we have to use nested loops to check all hierarchies and timers at each level.

This leads to another optimization: to use [Gray code](https://en.wikipedia.org/wiki/Gray_code) for the both system ticks and timer's target timeouts. The key observation here is that we're not interested in some sequence of hierarchy handling, instead we need flip of all bits that are different between tick counter and timer's timeout. While Gray encoding is not positional it flips bits with frequency roughly equal to the usual binary code. This is because Gray engoding does not change the most significant bit. So it is a good idea to place timer in queue corresponding to the most significant bit.

Gray encoding guarantees that no more than 1 bit will flip on every increment so we have to check exactly one queue on each tick as in the case of scheme 6. While the worst-case latency stays the same - O(n) when all the timers are inserted into the same list occasionally. But unlike scheme 7 this is just a possibility, not a guarantee.

Disadvantage of this scheme is the lack of correspondence between bit numbers and their 'weight'. In schemes 6 and 7 it is easy to calculate interval to the next nonempty list so it is easy to use dynamic ticks. In case of Gray code this is hard to calculate the interval when certain bit flips so it is not so easy to use this approach in tickless systems.


Example
-------

timer.c contains the algorithm implementation in C. Use the following command line to build:

        gcc -o test timers.c


The example implements simple delay using 10ms ticks. It accepts delay value in 10ms intervals as its first parameter argv[1].

