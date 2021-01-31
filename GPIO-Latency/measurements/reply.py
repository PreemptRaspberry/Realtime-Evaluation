#!/usr/bin/env python3
#apt install python3-rpi.gpio

import os
import RPi.GPIO as GPIO

GPIO_IN = 23
GPIO_OUT = 24

if __name__ == '__main__':
    param = os.sched_param(os.sched_get_priority_max(os.SCHED_FIFO))
    os.sched_setscheduler(80, os.SCHED_FIFO, param)

    GPIO.setmode(GPIO.BCM)
    #GPIO.setwarnings(False)

    # Set up the GPIO channels - one input and one output
    GPIO.setup(GPIO_IN, GPIO.IN)
    GPIO.setup(GPIO_OUT, GPIO.OUT)

    GPIO.output(GPIO_OUT, GPIO.HIGH)
    while True:
        try:
            while(GPIO.input(GPIO_IN)):
                pass
            GPIO.output(GPIO_OUT, GPIO.LOW)
            GPIO.output(GPIO_OUT, GPIO.HIGH)
        except KeyboardInterrupt:
            print('\nBye')
            GPIO.cleanup()
            exit(0)
