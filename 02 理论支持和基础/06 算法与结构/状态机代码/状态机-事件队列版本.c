#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MOTOR_EVENT_QUEUE_SIZE 8

typedef struct
{
    MotorEvent_t buffer[MOTOR_EVENT_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} MotorEventQueue_t;


