```
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_OTHER -o bin/other 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_FIFO -o bin/fifo 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DSLEEP -DDEADLINE_RUNTIME=100000 -DDEADLINE_DEADLINE=100000 -DDEADLINE_PERIOD=1000000 -o bin/dead_sleep 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DSLEEP -DDEADLINE_RUNTIME=1000000 -DDEADLINE_DEADLINE=1000000 -DDEADLINE_PERIOD=10000000 -o bin/dead_sleep_x10 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DEXAMINE_DEADLINE_MISS -DDEADLINE_RUNTIME=10000 -DDEADLINE_DEADLINE=10000 -DDEADLINE_PERIOD=100000 -DMISSX=4 -o bin/dead_examine_4 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DEXAMINE_DEADLINE_MISS -DDEADLINE_RUNTIME=100000 -DDEADLINE_DEADLINE=100000 -DDEADLINE_PERIOD=1000000 -DMISSX=4 -o bin/dead_examine_x10 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DEXAMINE_DEADLINE_MISS -DDEADLINE_RUNTIME=10000 -DDEADLINE_DEADLINE=10000 -DDEADLINE_PERIOD=100000 -DMISSX=4 -o bin/dead_examine_4_reclaim -DSCHED_FLAG_RECLAIM=1 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DEXAMINE_DEADLINE_MISS -DDEADLINE_RUNTIME=100000 -DDEADLINE_DEADLINE=100000 -DDEADLINE_PERIOD=1000000 -DMISSX=4 -o bin/dead_examine_x10_reclaim -DSCHED_FLAG_RECLAIM=1 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DNOSLEEP -DDEADLINE_RUNTIME=10000 -DDEADLINE_DEADLINE=10000 -DDEADLINE_PERIOD=100000 -o bin/dead_nosleep1 
 > COMPILE: gcc sched_test.c -lpthread -DMODE_SCHED_DEADLINE -DNOSLEEP -DDEADLINE_RUNTIME=100000 -DDEADLINE_DEADLINE=100000 -DDEADLINE_PERIOD=1000000 -o bin/dead_nosleep2 
```

Output:
```
Sat 23 Jan 20:55:51 GMT 2021
 > Running bin/other 
Sat 23 Jan 20:55:57 GMT 2021
 > Running bin/fifo 
Sat 23 Jan 20:56:03 GMT 2021
 > Running bin/dead_sleep 
Sat 23 Jan 20:56:43 GMT 2021
 > Running bin/dead_sleep_x10 
Sat 23 Jan 21:00:03 GMT 2021
 > Running bin/dead_examine_4 
Sat 23 Jan 21:00:05 GMT 2021
 > Running bin/dead_examine_x10_reclaim 
Sat 23 Jan 21:00:25 GMT 2021
 > Running bin/dead_examine_4_reclaim 
Sat 23 Jan 21:00:28 GMT 2021
 > Running bin/dead_examine_x10 
Sat 23 Jan 21:00:48 GMT 2021
 > Running bin/dead_nosleep1 
Sat 23 Jan 21:00:50 GMT 2021
 > Running bin/dead_nosleep2 
```
