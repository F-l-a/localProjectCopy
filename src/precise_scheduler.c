#include "precise_scheduler.h"
#include <stdio.h>

/* 1. Global Definitions for Logging (Deferred Logging) */

/* We statically allocate the log array in RAM. This way, 
 writing a log only requires a memory assignment, avoiding 
 the delays and jitter that a real-time printf would cause. */
EventLogRecord_t xEventBuffer[MAX_LOG_EVENTS];
volatile uint16_t usEventIndex = 0;
volatile uint8_t ucTestFinished = 0;
volatile uint32_t ulKernelCycleCount = 0; 

/* 2. System State Variables */

/* Global pointer to the task table. Being 'extern' in tasks.c, 
 it allows the kernel to read our timeline without copying data. */
TimelineTaskConfig_t *pxSystemTasks = NULL;
uint8_t ucTaskCount = 0;
volatile uint32_t ulGlobalTimeMs = 0;

/* 3. System Initialization */

/*
 vConfigureScheduler
 Prepares the system by creating tasks in a suspended state.
 The Kernel will then take control based on the timeline.
 */
void vConfigureScheduler( TimelineTaskConfig_t *pxTable, uint8_t ucTotal )
{
    /* Link the global pointer to the table defined in main */
    pxSystemTasks = pxTable;
    ucTaskCount = ucTotal;
    
    /* Initialize the global timer
     We set the initial time to 99 instead of 0. 
     This way, as soon as the SysTick timer is enabled and the 
     first interrupt fires, the kernel will calculate (99 + 1) % 100 = 0.
     Thus, the system will start perfectly aligned at time zero.*/
    ulGlobalTimeMs = 99; 
    usEventIndex = 0;
    ucTestFinished = 0;

    for( int i = 0; i < ucTaskCount; i++ )
    {
        /* Create tasks using standard FreeRTOS APIs */
        xTaskCreate( 
            pxSystemTasks[i].function, 
            pxSystemTasks[i].task_name, 
            configMINIMAL_STACK_SIZE + 128, /* A bit of extra stack for safety */
            pxSystemTasks[i].pvParameters, 
            pxSystemTasks[i].uxPriority, 
            &pxSystemTasks[i].taskHandle 
        );
        
        /* Tasks start suspended, the kernel will wake them up */
        vTaskSuspend( pxSystemTasks[i].taskHandle );
    }
}