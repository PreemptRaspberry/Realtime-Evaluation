#include <wiringPi.h>
#include <time.h>
#include <sched.h>
#define GPIO_IN 4 //bcm 23
#define GPIO_OUT 5 //bcm 24

struct timespec ts = { .tv_sec = 0,          // seconds to wait
                       .tv_nsec = 1 };     // additional nanoseconds

void interrupt(void) {
    digitalWrite(GPIO_OUT, LOW);
    nanosleep(&ts, NULL);
    digitalWrite(GPIO_OUT, HIGH);
}

int main (void)
{
    const struct sched_param priority = {80};
    sched_setscheduler(80, SCHED_FIFO, &priority);
    //SCHED_OTHER, SCHED_BATCH, SCHED_FIFO, SCHED_RR

    wiringPiSetup() ;

    wiringPiISR (GPIO_IN, INT_EDGE_FALLING, &interrupt) ;

    interrupt();

    while(1)
        ;
    return 0;
}
