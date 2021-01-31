#include <wiringPi.h>
#include <sched.h>
#define GPIO_IN 4 //bcm 23
#define GPIO_OUT 5 //bcm 24

int main(void)
{
    const struct sched_param priority = {80};
    sched_setscheduler(0, SCHED_RR, &priority);
    //SCHED_OTHER, SCHED_BATCH, SCHED_FIFO, SCHED_RR

    wiringPiSetup() ;
    pinMode(GPIO_OUT, OUTPUT);
    pinMode(GPIO_IN, INPUT);
    while(1){
        while(digitalRead(GPIO_IN))
            ;
        digitalWrite(GPIO_OUT, LOW);
        digitalWrite(GPIO_OUT, HIGH);
    }
    return 0;
}
