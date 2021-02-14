#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>


// Following values may be adjusted
//#define MODE_SCHED_OTHER
//#define MODE_SCHED_FIFO
//#define MODE_SCHED_DEADLINE

#define VERBOSE 0                                // output some additional information

//#define MODE "SCHED_OTHER"
//#define MODE "SCHED_FIFO"
//#define MODE "SCHED_DEADLINE"

#define ITERATIONS 20000
#define RUNTIME_NS 200000
#define NICE -20 // dynamic priority; SCHED_OTHER only

// Only used when using SCHED_DEADLINE
//#define NOSLEEP
//#define SLEEP
//#define EXAMINE_DEADLINE_MISS

#ifndef EXAMINE_DEADLINE_MISS
   // miss 1/MISSX deadlines in "EXAMINE_DEADLINE_MISS" mode, irrelevant for other modes/schedulers
#  define MISSX 4
#  define SCHED_FLAG_RECLAIM 0x02
#endif

#define CLK CLOCK_MONOTONIC
#define SCHED_DEADLINE 6
#define SCHED_FLAG_DL_OVERRUN 0x04

#ifndef MODE_SCHED_DEADLINE
#  define DEADLINE_RUNTIME 100000              // used 60k, 100k for SLEEP
#  define DEADLINE_DEADLINE 100000
#  define DEADLINE_PERIOD DEADLINE_RUNTIME*10
#endif


// Don't change values from here on !

#define SCHED_DEADLINE 6

/* XXX use the proper syscall numbers */
#ifdef __x86_64__
#define __NR_sched_setattr           314
#define __NR_sched_getattr           315
#endif

#ifdef __i386__
#define __NR_sched_setattr           351
#define __NR_sched_getattr           352
#endif

#ifdef __arm__
#ifndef __NR_sched_setattr			// already defined on RaspberryPi
#define __NR_sched_setattr           380
#endif
#ifndef __NR_sched_getattr
#define __NR_sched_getattr           381
#endif
#endif


static volatile int done;
static volatile sig_atomic_t dl_missC = 0;

struct sched_attr {
     __u32 size;

     __u32 sched_policy;
     __u64 sched_flags;

     /* SCHED_NORMAL, SCHED_BATCH */
     __s32 sched_nice;

     /* SCHED_FIFO, SCHED_RR */
     __u32 sched_priority;

     /* SCHED_DEADLINE (nsec) */
     __u64 sched_runtime;
     __u64 sched_deadline;
     __u64 sched_period;
};

int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags){
     return syscall(__NR_sched_setattr, pid, attr, flags);
}

int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags){
     return syscall(__NR_sched_getattr, pid, attr, size, flags);
}


void* thread_func(void* data)
{
	int niceVal = *(int*) data;
        bool isRT = false;
	struct timespec rt, t1, t2;
        __s64 time_elapsed_nanos, latency;
        __s64 maxT = 0;
        __u32 counter = 0, maxTpos;
        __s64* tmpArr = malloc((ITERATIONS+2) * sizeof(__s64));

        if(niceVal == 100)              // passed 100 in case of SCHED_FIFO (nice values in range from -20 to +19)
                isRT = true;
	if(!isRT)
		nice(niceVal);

	for(int i = 0; i < ITERATIONS; ++i){
		clock_gettime(CLK, &rt);
		t1 = rt;
		rt.tv_nsec += RUNTIME_NS;
                if(rt.tv_nsec >= 1e9){
                        ++rt.tv_sec;
                        rt.tv_nsec -= 1e9;
                }
		clock_nanosleep(CLK, TIMER_ABSTIME, &rt, NULL);
		clock_gettime(CLK, &rt);
		t2 = rt;
		time_elapsed_nanos =
                        ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) -
                        ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);
                latency = time_elapsed_nanos - RUNTIME_NS;
                if(latency > maxT){
                        maxT = latency;
			maxTpos = counter;
		}

                tmpArr[counter++] = latency;
	}
        tmpArr[counter++] = maxT;
	tmpArr[counter] = maxTpos;

        return (void*)tmpArr;
}


void dl_miss(){
	++dl_missC;
	return;
}


/** function for SCHED_DEADLINE thread with nanosleep
*
* Measures latency of clock_nanosleep when run by SCHED_DEADLINE thread
**/
#ifdef MODE_SCHED_DEADLINE
void* deadline_func_sleep(void* data){
        (void) data;
        struct timespec rt, t1, t2;
        struct sched_attr attr;
        __s64 time_elapsed_nanos, latency;
        __s64 maxT = 0;
        __u32 counter = 0, maxTpos;
        __s64* tmpArr = malloc((ITERATIONS+2) * sizeof(__s64));

        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sched_policy = SCHED_DEADLINE;
        attr.sched_runtime = DEADLINE_RUNTIME;
        //attr.sched_deadline = attr.sched_period = DEADLINE_PERIOD;
	attr.sched_period = DEADLINE_PERIOD;
	attr.sched_deadline = DEADLINE_DEADLINE;

        int flags = 0;
        int ret = sched_setattr(0, &attr, flags);
        if(ret){
                printf("SCHED_DEADLINE: failed to set attributes\n");
                exit(-2);
        }

        for(int i = 0; i < ITERATIONS; ++i){
                clock_gettime(CLK, &rt);
		t1 = rt;
		rt.tv_nsec += RUNTIME_NS;
                if(rt.tv_nsec >= 1e9){
                        ++rt.tv_sec;
                        rt.tv_nsec -= 1e9;
                }
		clock_nanosleep(CLK, TIMER_ABSTIME, &rt, NULL);
		clock_gettime(CLK, &rt);
		t2 = rt;
		time_elapsed_nanos =
                        ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) -
                        ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);
                latency = time_elapsed_nanos - RUNTIME_NS;
                if(latency > maxT){
                        maxT = latency;
			maxTpos = counter;
		}

                tmpArr[counter++] = latency;
                /*if(latency >= DEADLINE_PERIOD - DEADLINE_DEADLINE)     // was the deadline missed?
                        ++missedCounter;
                */
		sched_yield();
	}
        tmpArr[counter++] = maxT;
	tmpArr[counter] = maxTpos;
        //tmpArr[counter] = missedCounter;

        return (void*)tmpArr;
}


/** function for SCHED_DEADLINE thread
*
* Measures accuracy of SCHED_DEADLINE periodical invocations of task
* @note If this function is used, set deadline = period = runtime, and keep it small
**/
void* deadline_func_noSleep(void* data){
        (void) data;
        struct timespec rt, t1, t2;
        struct sched_attr attr;
        __s64 time_elapsed_nanos, latency;
        __s64 maxT = 0;
        __u32 counter = 0, missedCounter = 0, maxTpos;
        __s64* tmpArr = malloc((ITERATIONS+3) * sizeof(__s64));

        struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = dl_miss;
	sigaction(SIGXCPU, &action, NULL);

        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sched_policy = SCHED_DEADLINE;
        attr.sched_runtime = DEADLINE_RUNTIME;
        //attr.sched_deadline = attr.sched_period = DEADLINE_PERIOD;
	attr.sched_deadline = DEADLINE_DEADLINE;
	attr.sched_period = DEADLINE_PERIOD;

        int flags = 0;
        int ret = sched_setattr(0, &attr, flags);
        if(ret){
                printf("SCHED_DEADLINE: failed to set attributes: errno: %m\n");
                exit(-2);
        }

        for(int i = 0; i < ITERATIONS; ++i){
                clock_gettime(CLK, &rt);
		t1 = rt;

                sched_yield();

		clock_gettime(CLK, &rt);
		t2 = rt;
		time_elapsed_nanos =
                        ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) -
                        ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);
                latency = time_elapsed_nanos;
                if(latency > maxT){
                        maxT = latency;
			maxTpos = counter;
		}

                tmpArr[counter++] = latency;
                if(latency >= (DEADLINE_PERIOD + DEADLINE_DEADLINE))     // was the deadline missed?
                        ++missedCounter;
	}
        tmpArr[counter++] = maxT;
	tmpArr[counter++] = maxTpos;
        tmpArr[counter] = missedCounter;

        return (void*)tmpArr;
}
#endif //ifdef MODE_SCHED_DEADLINE


void* deadline_func_miss(void* data){
        (void) data;
        struct timespec rt, t1, t2;
        struct sched_attr attr, attr2;
        __s64 time_elapsed_nanos, time_elapsed_nanos_current, latency;
        __s64 maxT = 0;
        __u32 counter = 0, missedCounter = 0, maxTpos;
        __s64* tmpArr = malloc((ITERATIONS+3) * sizeof(__s64));

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = dl_miss;
	sigaction(SIGXCPU, &action, NULL);

        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sched_policy = SCHED_DEADLINE;
#ifdef MODE_SCHED_DEADLINE
        attr.sched_runtime = DEADLINE_RUNTIME;
	attr.sched_deadline = DEADLINE_DEADLINE;
	attr.sched_period = DEADLINE_PERIOD;
#       if defined(FLAG_DL_OVERRUN)
                attr.sched_flags = FLAG_DL_OVERRUN;
#       elif defined(FLAG_RECLAIM)
                attr.sched_flags |= FLAG_RECLAIM;
#       endif
#endif

        int flags = 0;
        int ret = sched_setattr(0, &attr, flags);
        if(ret){
                printf("SCHED_DEADLINE: failed to set attributes: errno: %m\n");
                exit(-2);
        }

        if(VERBOSE){
                ret = sched_getattr(0, &attr2, sizeof(attr2), flags);
                if(ret){
                        printf("SCHED_DEADLINE: failed to get attributes: errno: %m\n");
                        exit(-2);
                }
                printf("Flags: %llx\n", attr2.sched_flags);
        }

        for(int i = 0; i < ITERATIONS; ++i){
                clock_gettime(CLK, &rt);
                t1 = rt;

                if(i % MISSX){                      // every <<MISSX>> iterations, miss the deadline
                       do{                          // wait until runtime is used up
                                clock_gettime(CLK, &rt);
                                time_elapsed_nanos_current =
                                ((rt.tv_sec)*(unsigned long long)1e9 + rt.tv_nsec) -
                                ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);
                        }
                        while(time_elapsed_nanos_current <= DEADLINE_RUNTIME*0.5);

                        clock_gettime(CLK, &rt);
                        t2 = rt;
                        time_elapsed_nanos =
                                ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) -
                                ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);
                }
                else{
                        do{                                    // wait until runtime is used up
                                clock_gettime(CLK, &rt);
                                time_elapsed_nanos_current =
                                        ((rt.tv_sec)*(unsigned long long)1e9 + rt.tv_nsec) -
                                        ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);
                        }
                        while(time_elapsed_nanos_current <= DEADLINE_RUNTIME*1.5);

                        clock_gettime(CLK, &rt);
                        t2 = rt;
                        time_elapsed_nanos =
                                ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) -
                                ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);

                }
                latency = time_elapsed_nanos;
                if(latency > maxT){
                        maxT = latency;
                        maxTpos = counter;
                }

                tmpArr[counter++] = latency;
                if(latency >= (DEADLINE_PERIOD - DEADLINE_DEADLINE))     // was the deadline missed?
                        ++missedCounter;
                sched_yield();
	}
        tmpArr[counter++] = maxT;
	tmpArr[counter++] = maxTpos;
        tmpArr[counter] = missedCounter;

        return (void*)tmpArr;
}





int main() {

	int niceVal1 = NICE;
	int niceVal2 = 100;

        FILE* file;
        struct sched_param param1, param2;
        pthread_attr_t attr1, attr2;
        pthread_t thread1, thread2, thread3;
        int ret;


        /* Lock memory */
        if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
                printf("mlockall failed: %m\n");
                exit(-2);
        }


        /* Initialize pthread attributes (default values) */
#if defined(MODE_SCHED_OTHER)
                ret = pthread_attr_init(&attr1);
                if (ret) {
                        printf("init pthread attributes failed (1)\n");
                        goto out;
                }
#elif defined(MODE_SCHED_FIFO)
                ret = pthread_attr_init(&attr2);
                if (ret) {
                        printf("init pthread attributes failed (2)\n");
                        goto out;
                }
#endif // init pthread



        /* Set a specific stack size  */
#if defined(MODE_SCHED_OTHER)
                ret = pthread_attr_setstacksize(&attr1, PTHREAD_STACK_MIN);
                if (ret) {
                        printf("pthread setstacksize failed (1)\n");
                        goto out;
                }
#elif defined(MODE_SCHED_FIFO)
                ret = pthread_attr_setstacksize(&attr2, PTHREAD_STACK_MIN);
                if (ret) {
                        printf("pthread setstacksize failed (2)\n");
                        goto out;
                }
#endif // set stack size



	/* Set scheduler policy and priority of pthread */
#if defined(MODE_SCHED_OTHER)
                ret = pthread_attr_setschedpolicy(&attr1, SCHED_OTHER);
                if (ret) {
                        printf("pthread setschedpolicy failed (1)\n");
                        goto out;
                }
                param1.sched_priority = 0;
                ret = pthread_attr_setschedparam(&attr1, &param1);
                if (ret) {
                        printf("pthread setschedparam failed (1)\n");
                        goto out;
                }
#elif defined(MODE_SCHED_FIFO)
                ret = pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
                if (ret) {
                        printf("pthread setschedpolicy failed (2)\n");
                        goto out;
                }
                param2.sched_priority = 80;
                ret = pthread_attr_setschedparam(&attr2, &param2);
                if (ret) {
                        printf("pthread setschedparam failed (2)\n");
                        goto out;
                }
#endif // set pthread scheduler policy



        /* Use scheduling parameters of attr */
#if defined (SCHED_OTHER)
                ret = pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
                if (ret) {
                        printf("pthread setinheritsched failed (1)\n");
                        goto out;
                }
#elif defined(MODE_SCHED_FIFO)
                ret = pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
                if (ret) {
                        printf("pthread setinheritsched failed (2)\n");
                        goto out;
                }
#endif // using attr parameters



#if defined(MODE_SCHED_OTHER)
                ret = pthread_create(&thread1, &attr1, thread_func, &niceVal1);
                if (ret) {
                        printf("create pthread 1 failed: errno %d\n", errno);
                        goto out;
                }
#elif defined(MODE_SCHED_FIFO)
                ret = pthread_create(&thread2, &attr2, thread_func, &niceVal2);
                if (ret) {
                        printf("create pthread 2 failed: errno %d\n", errno);
                        goto out;
                }
#elif defined(MODE_SCHED_DEADLINE)
#  if defined(NOSLEEP)
                        ret = pthread_create(&thread3, NULL, deadline_func_noSleep, NULL);
                        if(ret){
                                printf("create pthread 3 failed: %m\n");
                                goto out;
                        }
#  elif defined(SLEEP)
                        ret = pthread_create(&thread3, NULL, deadline_func_sleep, NULL);
                        if(ret){
                                printf("create pthread 3 failed: %m\n");
                                goto out;
                        }
#  elif defined(EXAMINE_DEADLINE_MISS)
                        ret = pthread_create(&thread3, NULL, deadline_func_miss, NULL);
                        if(ret){
                                printf("create pthread 3 failed: %m\n");
                                goto out;
                        }
#  endif // SCHED_DEADLINE
#endif // create threads



        /* Join the thread and wait until it is done */
#if defined(MODE_SCHED_OTHER)
                void* tmpArr;
                char formName[150];
                ret = pthread_join(thread1, &tmpArr);
                if (ret){
                        printf("join pthread 1 failed: %m\n");
                        goto out;
                }
                sprintf(formName, "results_OTHER__sleepNS[%d]_maxT[%lld@%lld]", RUNTIME_NS, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1]);
	        file = fopen(formName, "w+");      // tmpArr[ITERATIONS-1] == max. recorded latency
                for(int i = 0; i < ITERATIONS; ++i){                          // ITERATIONS - 1 because last spot is occupied by max. latency (maxT)
		        fprintf(file, "%lld\n", ((__s64*)tmpArr)[i]);
                }
	        fclose(file);
                free(tmpArr);
#elif defined(MODE_SCHED_FIFO)
                void* tmpArr;
                char formName[150];
                ret = pthread_join(thread2, &tmpArr);
                if (ret){
                        printf("join pthread 2 failed: %m\n");
                        goto out;
                }
                sprintf(formName, "results_FIFO__sleepNS[%d]_maxT[%lld@%lld]", RUNTIME_NS, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1]);
	        file = fopen(formName, "w+");
                for(int i = 0; i < ITERATIONS; ++i){
		        fprintf(file, "%lld\n", ((__s64*)tmpArr)[i]);
                }
	        fclose(file);
                free(tmpArr);
#elif defined(MODE_SCHED_DEADLINE)
                void* tmpArr;
                char formName[150];
                ret = pthread_join(thread3, &tmpArr);
                if (ret){
                        printf("join pthread 3 failed: %m\n");
                        goto out;
                }

#  if defined(SLEEP) // save files
                        sprintf(formName, "results_DEADLINE_SLEEP__sleepNS[%d]_P[%d]_D[%d]_R[%d]_maxT[%lld@%lld]",
                                RUNTIME_NS, DEADLINE_PERIOD, DEADLINE_DEADLINE, DEADLINE_RUNTIME, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1]);
#  elif defined(NOSLEEP)
                        sprintf(formName, "results_DEADLINE_NOSLEEP__P[%d]_D[%d]_R[%d]_maxT[%lld@%lld]_missed[%lld]",
                                DEADLINE_PERIOD, DEADLINE_DEADLINE, DEADLINE_RUNTIME, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1], ((__s64*)tmpArr)[ITERATIONS+2]);
#  elif defined(EXAMINE_DEADLINE_MISS)
#    if defined(SCHED_FLAG_RECLAIM)
#        define RECLAIM_STR "true"
#    else
#        define RECLAIM_STR "false"
#    endif
                        sprintf(formName, "results_DEADLINE_MISSES__P[%d]_D[%d]_R[%d]_MISSX[%d]_maxT[%lld@%lld]_missed[%lld]_SIGXCPU[%d]_reclaim=%s",
                                DEADLINE_PERIOD, DEADLINE_DEADLINE, DEADLINE_RUNTIME, MISSX, ((__s64 *)tmpArr)[ITERATIONS], ((__s64 *)tmpArr)[ITERATIONS + 1],
                                ((__s64 *)tmpArr)[ITERATIONS + 2], dl_missC, RECLAIM_STR);
#  endif // save files

                file = fopen(formName, "w+");
                for(int i = 0; i < ITERATIONS; ++i){
		        fprintf(file, "%lld\n", ((__s64*)tmpArr)[i]);
                }
                fclose(file);
                free(tmpArr);
#endif // join thread




out:
        return ret;
}


