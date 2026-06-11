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

typedef struct
{
    MotorState_t state;
    MotorState_t last_state;

    MotorEvent_t current_event;
    MotorEvent_t last_event;

    MotorFault_t fault_code;
    uint8_t is_motor_on;

    MotorEventQueue_t event_queue;
} MotorCtrl_t;

static void Motor_EventQueue_Init(MotorEventQueue_t *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

void Motor_Init(void)
{
    memset(&g_motor_ctrl, 0, sizeof(g_motor_ctrl));

    Motor_Stop();
    Timer_StopStartProtect();
    Timer_StopRunLimit();
    Alarm_Off();

    g_motor_ctrl.state = MOTOR_STATE_STOP;
    g_motor_ctrl.last_state = MOTOR_STATE_STOP;
    g_motor_ctrl.current_event = MOTOR_EVENT_NONE;
    g_motor_ctrl.last_event = MOTOR_EVENT_NONE;
    g_motor_ctrl.fault_code = MOTOR_FAULT_NONE;
    g_motor_ctrl.is_motor_on = 0;

    Motor_EventQueue_Init(&g_motor_ctrl.event_queue);
}

static uint8_t Motor_EventQueue_Push(MotorEventQueue_t *queue, MotorEvent_t event)
{
    if (queue->count >= MOTOR_EVENT_QUEUE_SIZE)
    {
        return 0; // 队列满
    }

    queue->buffer[queue->tail] = event;
    queue->tail++;

    if (queue->tail >= MOTOR_EVENT_QUEUE_SIZE)
    {
        queue->tail = 0;
    }

    queue->count++;

    return 1;
}

uint8_t Motor_PostEvent(MotorEvent_t event)
{
    if (event == MOTOR_EVENT_NONE)
    {
        return 0;
    }

    return Motor_EventQueue_Push(&g_motor_ctrl.event_queue, event);
}

static uint8_t Motor_EventQueue_Pop(MotorEventQueue_t *queue, MotorEvent_t *event)
{
    if (queue->count == 0)
    {
        return 0; // 队列空
    }

    *event = queue->buffer[queue->head];
    queue->head++;

    if (queue->head >= MOTOR_EVENT_QUEUE_SIZE)
    {
        queue->head = 0;
    }

    queue->count--;

    return 1;
}

void Motor_StateMachine_Run(void)
{
    MotorCtrl_t *ctrl = &g_motor_ctrl;
    MotorEvent_t event;

    if (Motor_EventQueue_Pop(&ctrl->event_queue, &event) == 0)
    {
        return;
    }

    ctrl->current_event = event;

    switch (ctrl->state)
    {
        case MOTOR_STATE_STOP:
        {
            if (event == MOTOR_EVENT_START_KEY)
            {
                Motor_Start();
                Timer_StartStartProtect(3000);

                ctrl->last_state = ctrl->state;
                ctrl->state = MOTOR_STATE_STARTING;
                ctrl->fault_code = MOTOR_FAULT_NONE;
                ctrl->is_motor_on = 1;
            }
            break;
        }

        case MOTOR_STATE_STARTING:
        {
            if (event == MOTOR_EVENT_OVER_CURRENT)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_OVER_CURRENT);
            }
            else if (event == MOTOR_EVENT_STALL)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_STALL);
            }
            else if (event == MOTOR_EVENT_START_TIMEOUT)
            {
                Timer_StopStartProtect();
                Timer_StartRunLimit(60000);

                ctrl->last_state = ctrl->state;
                ctrl->state = MOTOR_STATE_RUNNING;
            }
            else if (event == MOTOR_EVENT_STOP_KEY)
            {
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        case MOTOR_STATE_RUNNING:
        {
            if (event == MOTOR_EVENT_OVER_CURRENT)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_OVER_CURRENT);
            }
            else if (event == MOTOR_EVENT_STALL)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_STALL);
            }
            else if (event == MOTOR_EVENT_STOP_KEY)
            {
                Motor_EnterStopState(ctrl);
            }
            else if (event == MOTOR_EVENT_RUN_TIMEOUT)
            {
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        case MOTOR_STATE_FAULT:
        {
            if (event == MOTOR_EVENT_RESET_KEY)
            {
                Alarm_Off();
                ctrl->fault_code = MOTOR_FAULT_NONE;
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        default:
        {
            Motor_EnterFaultState(ctrl, MOTOR_FAULT_NONE);
            break;
        }
    }

    ctrl->last_event = event;
    ctrl->current_event = MOTOR_EVENT_NONE;
}

//优先级队列

#define EVENT_QUEUE_SIZE 8

typedef struct
{
    MotorEvent_t buffer[EVENT_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} EventQueue_t;

typedef enum
{
    EVENT_PRIO_LOW = 0,
    EVENT_PRIO_NORMAL,
    EVENT_PRIO_HIGH,
    EVENT_PRIO_CRITICAL,

    EVENT_PRIO_MAX
} EventPriority_t;

typedef struct
{
    EventQueue_t queue[EVENT_PRIO_MAX];
} PriorityEventQueue_t;

/*
queue[0] → LOW 队列
queue[1] → NORMAL 队列
queue[2] → HIGH 队列
queue[3] → CRITICAL 队列
*/

typedef struct
{
    MotorState_t state;
    MotorState_t last_state;

    MotorEvent_t current_event;
    MotorEvent_t last_event;

    MotorFault_t fault_code;
    uint8_t is_motor_on;

    PriorityEventQueue_t event_queue;
} MotorCtrl_t;

static EventPriority_t Motor_GetEventPriority(MotorEvent_t event)
{
    switch (event)
    {
        case MOTOR_EVENT_OVER_CURRENT:
        case MOTOR_EVENT_STALL:
            return EVENT_PRIO_CRITICAL;

        case MOTOR_EVENT_STOP_KEY:
            return EVENT_PRIO_HIGH;

        case MOTOR_EVENT_START_KEY:
        case MOTOR_EVENT_RESET_KEY:
        case MOTOR_EVENT_START_TIMEOUT:
        case MOTOR_EVENT_RUN_TIMEOUT:
            return EVENT_PRIO_NORMAL;

        default:
            return EVENT_PRIO_LOW;
    }
}

/*
过流事件 → 放入 CRITICAL 队列
停止键事件 → 放入 HIGH 队列
启动键事件 → 放入 NORMAL 队列
*/
uint8_t Motor_PostEvent(MotorEvent_t event)
{
    EventPriority_t prio;

    if (event == MOTOR_EVENT_NONE)
    {
        return 0;
    }

    prio = Motor_GetEventPriority(event);

    return EventQueue_Push(&g_motor_ctrl.event_queue.queue[prio], event);
}


/*
先看 CRITICAL 队列有没有事件
有就处理 CRITICAL

没有再看 HIGH
没有再看 NORMAL
没有再看 LOW
*/
static uint8_t Motor_PriorityQueue_Pop(PriorityEventQueue_t *pq,
                                       MotorEvent_t *event)
{
    int8_t prio;

    for (prio = EVENT_PRIO_CRITICAL; prio >= EVENT_PRIO_LOW; prio--)
    {
        if (EventQueue_Pop(&pq->queue[prio], event))
        {
            return 1;
        }
    }

    return 0;
}


void Motor_StateMachine_Run(void)
{
    MotorCtrl_t *ctrl = &g_motor_ctrl;
    MotorEvent_t event;

    if (Motor_PriorityQueue_Pop(&ctrl->event_queue, &event) == 0)
    {
        return;
    }

    ctrl->current_event = event;

    switch (ctrl->state)
    {
        case MOTOR_STATE_STOP:
        {
            if (event == MOTOR_EVENT_START_KEY)
            {
                Motor_Start();
                Timer_StartStartProtect(3000);

                ctrl->last_state = ctrl->state;
                ctrl->state = MOTOR_STATE_STARTING;
                ctrl->fault_code = MOTOR_FAULT_NONE;
                ctrl->is_motor_on = 1;
            }
            break;
        }

        case MOTOR_STATE_STARTING:
        {
            if (event == MOTOR_EVENT_OVER_CURRENT)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_OVER_CURRENT);
            }
            else if (event == MOTOR_EVENT_STALL)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_STALL);
            }
            else if (event == MOTOR_EVENT_START_TIMEOUT)
            {
                Timer_StopStartProtect();
                Timer_StartRunLimit(60000);

                ctrl->last_state = ctrl->state;
                ctrl->state = MOTOR_STATE_RUNNING;
            }
            else if (event == MOTOR_EVENT_STOP_KEY)
            {
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        case MOTOR_STATE_RUNNING:
        {
            if (event == MOTOR_EVENT_OVER_CURRENT)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_OVER_CURRENT);
            }
            else if (event == MOTOR_EVENT_STALL)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_STALL);
            }
            else if (event == MOTOR_EVENT_STOP_KEY)
            {
                Motor_EnterStopState(ctrl);
            }
            else if (event == MOTOR_EVENT_RUN_TIMEOUT)
            {
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        case MOTOR_STATE_FAULT:
        {
            if (event == MOTOR_EVENT_RESET_KEY)
            {
                Alarm_Off();
                ctrl->fault_code = MOTOR_FAULT_NONE;
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        default:
        {
            Motor_EnterFaultState(ctrl, MOTOR_FAULT_NONE);
            break;
        }
    }

    ctrl->last_event = event;
    ctrl->current_event = MOTOR_EVENT_NONE;
}