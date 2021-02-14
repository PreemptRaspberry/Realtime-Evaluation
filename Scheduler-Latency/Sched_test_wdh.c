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


/*****  Following values may be adjusted  *****/

#define VERBOSE 0                               // output some additional information

//#define MODE "SCHED_OTHER"                    // select one of these scheduling policies
//#define MODE "SCHED_FIFO"
#define MODE "SCHED_DEADLINE"

#define ITERATIONS 20000                        // number of measurements
#define RUNTIME_NS 200000                       // duration used for all clock_nanosleep() calls
#define NICE -20                                // dynamic priority; SCHED_OTHER only

/* Only used when using SCHED_DEADLINE */
#define D_MODE "SLEEP"                          // select one of these modes, depending on what you want to measure
//#define D_MODE "NOSLEEP"                      // (not all modes available for all schedulers; refer to the sourcecode
//#define D_MODE "EXAMINE_DEADLINE_MISS"        // for accurate information)

#define FLAG_RECLAIM 0                          // enable GRUB, relevant in "EXAMINE_DEADLINE_MISS" mode
#define FLAG_DL_OVERRUN 1                       // enable deadline-miss-detection, relevant in "EXAMINE_DEADLINE_MISS" mode
#define MISSX 4                                 // miss 1/MISSX deadlines in "EXAMINE_DEADLINE_MISS" mode
#define DEADLINE_RUNTIME 100000
#define DEADLINE_DEADLINE 100000
#define DEADLINE_PERIOD DEADLINE_RUNTIME*10     // for reliable results, choose P to be several times bigger than D, R !!



/*****  Don't change values from here on !  *****/

#define CLK CLOCK_MONOTONIC
#define SCHED_DEADLINE 6
#define SCHED_FLAG_RECLAIM 0x02
#define SCHED_FLAG_DL_OVERRUN 0x04

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

/* Wrappers for syscalls */
int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags){
     return syscall(__NR_sched_setattr, pid, attr, flags);
}

int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags){
     return syscall(__NR_sched_getattr, pid, attr, size, flags);
}


/** dl_miss
* Helper function for detecting
* missed deadlines
*/
void dl_miss(){     
	++dl_missC;
	return;
}


/** thread_func
* Function used to run SCHED_OTHER and SCHED_FIFO threads
* Measures latency when executing clock_nanosleep(),
* duration may be defined through RUNTIME_NS
*/
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
                if(rt.tv_nsec >= 1e9){          // check if previous line has exceeded max. amount of nanoseconds
                        ++rt.tv_sec;
                        rt.tv_nsec -= 1e9;
                }
		clock_nanosleep(CLK, TIMER_ABSTIME, &rt, NULL);
		clock_gettime(CLK, &rt);
		t2 = rt;
		time_elapsed_nanos = ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) - ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);	
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


/** deadline_func_sleep
* Function used to run SCHED_DEADLINE thread
* Measures latency when executing clock_nanosleep(),
* duration may be defined through RUNTIME_NS
*
* @note For meaningful results, P, D and R have to be
* chosen somewhat bigger than RUNTIME_NS
*/
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
		time_elapsed_nanos = ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) - ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);	
                latency = time_elapsed_nanos - RUNTIME_NS;
                if(latency > maxT){
                        maxT = latency;
			maxTpos = counter;
		}

                tmpArr[counter++] = latency;
		sched_yield();
	}
        tmpArr[counter++] = maxT;
	tmpArr[counter] = maxTpos;

        return (void*)tmpArr;
}


/** deadline_func_noSleep
* Function used to run SCHED_DEADLINE thread
* Measures time passed between two scheduled
* periods by the SCHED_DEADLINE scheduler
*/
void* deadline_func_noSleep(void* data){
        (void) data;
        struct timespec rt, t1, t2;
        struct sched_attr attr;
        __s64 time_elapsed_nanos, latency;
        __s64 maxT = 0;
        __u32 counter = 0, missedCounter = 0, maxTpos;
        __s64* tmpArr = malloc((ITERATIONS+3) * sizeof(__s64)); 

        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sched_policy = SCHED_DEADLINE;
        attr.sched_runtime = DEADLINE_RUNTIME;
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
		time_elapsed_nanos = ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) - ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);	
                latency = time_elapsed_nanos;
                if(latency > maxT){
                        maxT = latency;
			maxTpos = counter;
		}

                tmpArr[counter++] = latency;
                if(latency >= (DEADLINE_PERIOD + DEADLINE_DEADLINE))              // was the deadline missed?
                        ++missedCounter;
	}
        tmpArr[counter++] = maxT;
	tmpArr[counter++] = maxTpos;
        tmpArr[counter] = missedCounter;

        return (void*)tmpArr;
}


/** deadline_func_miss
* Function used to run SCHED_DEADLINE thread
* Purposefully misses set amount of deadlines
* Used to examine cases in which the SCHED_DEADLINE
* scheduler fails in its function
*
* @note Deadlines to be missed equals 1/MISSX
* @note May use GRUB algorithm by setting FLAG_RECLAIM
*/
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
        attr.sched_runtime = DEADLINE_RUNTIME;
	attr.sched_deadline = DEADLINE_DEADLINE;
	attr.sched_period = DEADLINE_PERIOD;
        if(FLAG_DL_OVERRUN)
	        attr.sched_flags = SCHED_FLAG_DL_OVERRUN;       // enable detection of missed deadlines through system-signals (SIGXCPU)
        if(FLAG_RECLAIM)
                attr.sched_flags |= SCHED_FLAG_RECLAIM;         // enable Greedy Reclamation of Unused Bandwith (GRUB)

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

                if(i % MISSX){                                // every <<MISSX>> iterations, miss the deadline
                       do{                                    // wait until runtime is used up by 50%
                                clock_gettime(CLK, &rt);
                                time_elapsed_nanos_current = ((rt.tv_sec)*(unsigned long long)1e9 + rt.tv_nsec) - ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);	
                        }
                        while(time_elapsed_nanos_current <= DEADLINE_RUNTIME*0.5);

                        clock_gettime(CLK, &rt);
                        t2 = rt;
                        time_elapsed_nanos = ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) - ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);		
                }
                else{
                        do{                                    // wait until least 150% of runtime have passed (deadline was missed if R=D)
                                clock_gettime(CLK, &rt);
                                time_elapsed_nanos_current = ((rt.tv_sec)*(unsigned long long)1e9 + rt.tv_nsec) - ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);	
                        }
                        while(time_elapsed_nanos_current <= DEADLINE_RUNTIME*1.5);

                        clock_gettime(CLK, &rt);
                        t2 = rt;
                        time_elapsed_nanos = ((t2.tv_sec)*(unsigned long long)1e9 + t2.tv_nsec) - ((t1.tv_sec)*(unsigned long long)1e9 + t1.tv_nsec);	

                }
                latency = time_elapsed_nanos;
                if(latency > maxT){
                        maxT = latency;
                        maxTpos = counter;
                }

                tmpArr[counter++] = latency;
                if(latency >= (DEADLINE_PERIOD - DEADLINE_DEADLINE))              // was the deadline missed?
                        ++missedCounter;
                sched_yield();
	}
        tmpArr[counter++] = maxT;
	tmpArr[counter++] = maxTpos;
        tmpArr[counter] = missedCounter;

        return (void*)tmpArr;
}




/** main
* Prepares the processor (mlockall),
* Creates and joins Threads and sets
* their attributes and behavior according
* to the macros at the beginning of this
* file
*/
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
        if(!strcmp(MODE, "SCHED_OTHER")){
                ret = pthread_attr_init(&attr1);
                if (ret) {
                        printf("init pthread attributes failed (1)\n");
                        goto out;
                }
        }
        else if(!strcmp(MODE, "SCHED_FIFO")){
                ret = pthread_attr_init(&attr2);
                if (ret) {
                        printf("init pthread attributes failed (2)\n");
                        goto out;
                }
        }


        /* Set a specific stack size  */
        if(!strcmp(MODE, "SCHED_OTHER")){
                ret = pthread_attr_setstacksize(&attr1, PTHREAD_STACK_MIN);
                if (ret) {
                        printf("pthread setstacksize failed (1)\n");
                        goto out;
                }
        }
        else if(!strcmp(MODE, "SCHED_FIFO")){
                ret = pthread_attr_setstacksize(&attr2, PTHREAD_STACK_MIN);
                if (ret) {
                        printf("pthread setstacksize failed (2)\n");
                        goto out;
                }
        }


	/* Set scheduler policy and priority of pthread */
        if(!strcmp(MODE, "SCHED_OTHER")){
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
        }
        else if(!strcmp(MODE, "SCHED_FIFO")){
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
        }


        /* Use scheduling parameters of attr */
        if(!strcmp(MODE, "SCHED_OTHER")){
                ret = pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);
                if (ret) {
                        printf("pthread setinheritsched failed (1)\n");
                        goto out;
                }
        }
        else if(!strcmp(MODE, "SCHED_FIFO")){
                ret = pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);
                if (ret) {
                        printf("pthread setinheritsched failed (2)\n");
                        goto out;
                }
        }


        if(!strcmp(MODE, "SCHED_OTHER")){
                ret = pthread_create(&thread1, &attr1, thread_func, &niceVal1);
                if (ret) {
                        printf("create pthread 1 failed: errno %d\n", errno);
                        goto out;
                }
        }
	else if(!strcmp(MODE, "SCHED_FIFO")){
                ret = pthread_create(&thread2, &attr2, thread_func, &niceVal2);
                if (ret) {
                        printf("create pthread 2 failed: errno %d\n", errno);
                        goto out;
                }
        }
        else if(!strcmp(MODE, "SCHED_DEADLINE")){
                if(!strcmp(D_MODE, "NOSLEEP")){
                        ret = pthread_create(&thread3, NULL, deadline_func_noSleep, NULL);
                        if(ret){
                                printf("create pthread 3 failed: %m\n");
                                goto out;
                        }
                }
                else if(!strcmp(D_MODE, "SLEEP")){
                        ret = pthread_create(&thread3, NULL, deadline_func_sleep, NULL);
                        if(ret){
                                printf("create pthread 3 failed: %m\n");
                                goto out;
                        }
                }
                else if(!strcmp(D_MODE, "EXAMINE_DEADLINE_MISS")){
                        ret = pthread_create(&thread3, NULL, deadline_func_miss, NULL);
                        if(ret){
                                printf("create pthread 3 failed: %m\n");
                                goto out;
                        }
                }
        }


        /* Join the thread and wait until it is done; format output-filenames to include useful metadata */
        if(!strcmp(MODE, "SCHED_OTHER")){
                void* tmpArr;
                char formName[150];
                ret = pthread_join(thread1, &tmpArr);
                if (ret){
                        printf("join pthread 1 failed: %m\n");
                        goto out;
                }
                sprintf(formName, "results_OTHER__sleepNS[%d]_maxT[%lld@%lld]", RUNTIME_NS, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1]);
	        file = fopen(formName, "w+");
                for(int i = 0; i < ITERATIONS; ++i){
		        fprintf(file, "%lld\n", ((__s64*)tmpArr)[i]);
                }
	        fclose(file);
                free(tmpArr);
        }
        else if(!strcmp(MODE, "SCHED_FIFO")){
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
        }
        else if(!strcmp(MODE, "SCHED_DEADLINE")){
                void* tmpArr;
                char formName[150];
                ret = pthread_join(thread3, &tmpArr);
                if (ret){
                        printf("join pthread 3 failed: %m\n");
                        goto out;
                }
                
                if(!strcmp(D_MODE, "SLEEP")){
                        sprintf(formName, "results_DEADLINE_SLEEP__sleepNS[%d]_P[%d]_D[%d]_R[%d]_maxT[%lld@%lld]", RUNTIME_NS, DEADLINE_PERIOD, DEADLINE_DEADLINE, DEADLINE_RUNTIME, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1]);
                }
                else if(!strcmp(D_MODE, "NOSLEEP")){
                        sprintf(formName, "results_DEADLINE_NOSLEEP__P[%d]_D[%d]_R[%d]_maxT[%lld@%lld]_missed[%lld]", DEADLINE_PERIOD, DEADLINE_DEADLINE, DEADLINE_RUNTIME, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1], ((__s64*)tmpArr)[ITERATIONS+2]);
                }
                else if(!strcmp(D_MODE, "EXAMINE_DEADLINE_MISS")){
                        if(FLAG_DL_OVERRUN){
                                sprintf(formName, "results_DEADLINE_MISSES__P[%d]_D[%d]_R[%d]_MISSX[%d]_maxT[%lld@%lld]_missed[%lld]_SIGXCPU[%d]_reclaim=%s", DEADLINE_PERIOD, DEADLINE_DEADLINE, DEADLINE_RUNTIME, MISSX, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1], ((__s64*)tmpArr)[ITERATIONS+2], dl_missC, (FLAG_RECLAIM ? "true":"false"));
                        }
                        else{
                                sprintf(formName, "results_DEADLINE_MISSES__P[%d]_D[%d]_R[%d]_MISSX[%d]_maxT[%lld@%lld]_missed[%lld]_reclaim=%s", DEADLINE_PERIOD, DEADLINE_DEADLINE, DEADLINE_RUNTIME, MISSX, ((__s64*)tmpArr)[ITERATIONS], ((__s64*)tmpArr)[ITERATIONS+1], ((__s64*)tmpArr)[ITERATIONS+2], (FLAG_RECLAIM ? "true":"false"));
                        }
                }
                file = fopen(formName, "w+");
                for(int i = 0; i < ITERATIONS; ++i){
		        fprintf(file, "%lld\n", ((__s64*)tmpArr)[i]);
                }
                fclose(file);
                free(tmpArr);
        }


out:
        return ret;
}

