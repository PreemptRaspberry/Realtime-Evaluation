#!/usr/bin/env python3

import os
import RPi.GPIO as GPIO

GPIO_IN = 23
GPIO_OUT = 24

def gpio_isr(channel):
    GPIO.output(GPIO_OUT, GPIO.LOW)

if __name__ == '__main__':
    param = os.sched_param(os.sched_get_priority_max(os.SCHED_FIFO))
    os.sched_setscheduler(80, os.SCHED_FIFO, param)

    GPIO.setmode(GPIO.BCM)
    #GPIO.setwarnings(False)

    GPIO.setup(GPIO_IN, GPIO.IN)
    GPIO.setup(GPIO_OUT, GPIO.OUT)

    GPIO.add_event_detect(GPIO_IN, GPIO.FALLING, callback=gpio_isr)

    gpio_isr(0)

    #signal.signal(signal.SIGINT, signal_handler)
    #signal.pause()
    while True:
        try:
            GPIO.output(GPIO_OUT, GPIO.HIGH)
        except KeyboardInterrupt:
            print('\nBye')
            GPIO.cleanup()
            exit(0)
