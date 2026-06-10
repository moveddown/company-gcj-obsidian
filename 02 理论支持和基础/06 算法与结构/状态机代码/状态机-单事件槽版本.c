#include <stdio.h>
#include <string.h>
#include <stdint.h>

/*
场景：一个小电机控制模块，有启动键、停止键、电流检测、堵转检测、运行超时保护。

系统要求：

上电后处于停止状态，电机关闭。
按下启动键后，进入启动状态。
启动状态下，先打开电机，并等待电机稳定。
启动 3 秒后，如果没有故障，进入运行状态。
运行状态下，电机持续运行。
任意时候按下停止键，进入停止状态，关闭电机。
启动或运行过程中，如果检测到过流，进入故障状态。
启动或运行过程中，如果检测到堵转，进入故障状态。
运行状态下，如果连续运行超过 60 秒，进入停止状态。
故障状态下，电机关闭。
故障状态下，按下复位键，回到停止状态。
故障状态下，启动键无效。
*/
typedef enum
{
    MOTOR_STATE_STOP = 0,
    MOTOR_STATE_STARTING,
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_FAULT,
} MotorState_t;

typedef enum
{
    MOTOR_EVENT_NONE = 0,

    MOTOR_EVENT_START_KEY,
    MOTOR_EVENT_STOP_KEY,
    MOTOR_EVENT_RESET_KEY,

    MOTOR_EVENT_START_TIMEOUT,
    MOTOR_EVENT_RUN_TIMEOUT,

    MOTOR_EVENT_OVER_CURRENT,
    MOTOR_EVENT_STALL,
} MotorEvent_t;

typedef enum
{
    MOTOR_FAULT_NONE = 0,
    MOTOR_FAULT_OVER_CURRENT,
    MOTOR_FAULT_STALL,
} MotorFault_t;

void Motor_Start(void);
void Motor_Stop(void);

void Timer_StartStartProtect(uint32_t ms);
void Timer_StopStartProtect(void);

void Timer_StartRunLimit(uint32_t ms);
void Timer_StopRunLimit(void);

void Alarm_On(void);
void Alarm_Off(void);

typedef struct
{
    MotorState_t state;
    MotorEvent_t event;

    MotorState_t last_state;
    MotorEvent_t last_event;

    MotorFault_t fault_code;

    uint32_t time_count;
    uint8_t is_motor_on;
} MotorCtrl_t;

static MotorCtrl_t g_motor_ctrl;

void Motor_Init(void)
{
    memset(&g_motor_ctrl, 0, sizeof(g_motor_ctrl));

    Motor_Stop();
    Timer_StopStartProtect();
    Timer_StopRunLimit();
    Alarm_Off();

    g_motor_ctrl.state = MOTOR_STATE_STOP;
    g_motor_ctrl.event = MOTOR_EVENT_NONE;
    g_motor_ctrl.last_state = MOTOR_STATE_STOP;
    g_motor_ctrl.last_event = MOTOR_EVENT_NONE;
    g_motor_ctrl.fault_code = MOTOR_FAULT_NONE;
    g_motor_ctrl.time_count = 0;
    g_motor_ctrl.is_motor_on = 0;
}

void Motor_PostEvent(MotorEvent_t event)
{
    g_motor_ctrl.event = event;
}

static void Motor_EnterStopState(MotorCtrl_t *ctrl)
{
    Motor_Stop();
    Timer_StopStartProtect();
    Timer_StopRunLimit();

    ctrl->last_state = ctrl->state;
    ctrl->state = MOTOR_STATE_STOP;

    ctrl->time_count = 0;
    ctrl->is_motor_on = 0;
}

static void Motor_EnterFaultState(MotorCtrl_t *ctrl, MotorFault_t fault)
{
    Motor_Stop();
    Timer_StopStartProtect();
    Timer_StopRunLimit();
    Alarm_On();

    ctrl->last_state = ctrl->state;
    ctrl->state = MOTOR_STATE_FAULT;

    ctrl->fault_code = fault;
    ctrl->is_motor_on = 0;
}

void Motor_StateMachine_Run(void)
{
    MotorCtrl_t *ctrl = &g_motor_ctrl;

    if (ctrl->event == MOTOR_EVENT_NONE)
    {
        return;
    }

    switch (ctrl->state)
    {
        case MOTOR_STATE_STOP:
        {
            if (ctrl->event == MOTOR_EVENT_START_KEY)
            {
                Motor_Start();
                Timer_StartStartProtect(3000);

                ctrl->last_state = ctrl->state;
                ctrl->state = MOTOR_STATE_STARTING;

                ctrl->fault_code = MOTOR_FAULT_NONE;
                ctrl->time_count = 0;
                ctrl->is_motor_on = 1;
            }
            else if (ctrl->event == MOTOR_EVENT_STOP_KEY)
            {
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        case MOTOR_STATE_STARTING:
        {
            if (ctrl->event == MOTOR_EVENT_OVER_CURRENT)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_OVER_CURRENT);
            }
            else if (ctrl->event == MOTOR_EVENT_STALL)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_STALL);
            }
            else if (ctrl->event == MOTOR_EVENT_START_TIMEOUT)
            {
                Timer_StopStartProtect();
                Timer_StartRunLimit(60000);

                ctrl->last_state = ctrl->state;
                ctrl->state = MOTOR_STATE_RUNNING;
            }
            else if (ctrl->event == MOTOR_EVENT_STOP_KEY)
            {
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        case MOTOR_STATE_RUNNING:
        {
            if (ctrl->event == MOTOR_EVENT_OVER_CURRENT)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_OVER_CURRENT);
            }
            else if (ctrl->event == MOTOR_EVENT_STALL)
            {
                Motor_EnterFaultState(ctrl, MOTOR_FAULT_STALL);
            }
            else if (ctrl->event == MOTOR_EVENT_STOP_KEY)
            {
                Motor_EnterStopState(ctrl);
            }
            else if (ctrl->event == MOTOR_EVENT_RUN_TIMEOUT)
            {
                Motor_EnterStopState(ctrl);
            }
            break;
        }

        case MOTOR_STATE_FAULT:
        {
            if (ctrl->event == MOTOR_EVENT_RESET_KEY)
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

    ctrl->last_event = ctrl->event;
    ctrl->event = MOTOR_EVENT_NONE;
}