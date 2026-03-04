#ifndef PRECISE_SCHEDULER_H
#define PRECISE_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

/*
 Timeline Configuration (Major Frame)
 Defines the total duration of the scheduling cycle in milliseconds.
 Every 100ms the task sequence starts over.
 */

#define MAJOR_FRAME_PERIOD_MS    100

/* Global system clock. It is incremented directly by the kernel 
 (in tasks.c) to achieve the highest possible precision. */
extern volatile uint32_t ulGlobalTimeMs;
extern TaskHandle_t xMonitorHandle;

/* Kernel Bridge, task data structures */

typedef enum {
    TASK_TYPE_HRT, /* Hard Real-Time: started and preempted strictly at preset times */
    TASK_TYPE_SRT  /* Soft Real-Time: executed in the background when no HRT is active */
} TaskType_t;

typedef struct {
    const char* task_name;      /* Useful name for logging */
    TaskFunction_t function;    /* Task entry point */
    TaskType_t type;            /* HRT or SRT */
    UBaseType_t uxPriority;     /* Standard FreeRTOS priority */

    /* Parameters used ONLY for HRT tasks (in milliseconds) */
    uint32_t ulStart_time;      /* Time when the task must be woken up */
    uint32_t ulEnd_time;        /* Deadline: if the task is still active here, it gets terminated */

    void *pvParameters;         /* Optional parameters (used to pass the workload) */
    TaskHandle_t taskHandle;    /* Internal FreeRTOS pointer, automatically filled upon creation */
} TimelineTaskConfig_t;

/* These two variables export our configuration to the kernel */
extern TimelineTaskConfig_t *pxSystemTasks;
extern uint8_t ucTaskCount;

/*
 Initialization functions
 Receives the task array from main.c, physically creates the tasks in FreeRTOS
 and puts them in a suspended state waiting for the kernel to manage them.
 @param pxTaskTable Pointer to the task configuration array.
 @param ucTotalTasks Number of entries in the array.
 */
void vConfigureScheduler( TimelineTaskConfig_t *pxTaskTable, uint8_t ucTotalTasks );

/* Logging in RAM (DEFERRED LOGGING) --- */

/* 1. Event types we want to record in the kernel */
typedef enum {
    EVENT_KERNEL_RELEASE, /* The kernel woke up the task */
    EVENT_TASK_RUNNING,  /* The task actually started working */
    EVENT_TASK_END,     /* The task finished its work on time */
    EVENT_DEADLINE_MISS, /* The task missed the deadline and the kernel punished it */
    EVENT_CYCLE_END     /* A major frame has ended */
} LogEventType_t;

/* 2. Structure of the single log "record" */
typedef struct {
    uint32_t timestamp;         /* The millisecond (ulGlobalTimeMs) */
    const char* task_name;      /* The name of the involved task */
    LogEventType_t event_type;  /* What happened */
} EventLogRecord_t;

/* 3. Buffer Sizing */
/* 100 events are enough to track 3 cycles of our 5 tasks */
#define MAX_LOG_EVENTS 100

/* 4. Global buffer variables (shared between kernel and application) */
extern EventLogRecord_t xEventBuffer[MAX_LOG_EVENTS];
extern volatile uint32_t ulKernelCycleCount; /* Complete cycle counter (incremented in the kernel) */
extern volatile uint16_t usEventIndex;
extern volatile uint8_t ucTestFinished; /* Flag to tell main that the test is completed (3 cycles) */

#endif /* PRECISE_SCHEDULER_H */