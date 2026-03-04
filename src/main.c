#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "precise_scheduler.h"
#include "string.h"

/* External variables defined in the kernel/bridge */
extern volatile uint32_t ulGlobalTimeMs;
extern volatile uint8_t ucTestFinished;
extern EventLogRecord_t xEventBuffer[MAX_LOG_EVENTS];
extern volatile uint16_t usEventIndex;

/* Semihosting */
extern void initialise_monitor_handles(void);

/* Forward declaration of the function used to start the next SRT task */
void vWakeNextSRT( void );

/* Global handle for the Monitor Task to allow kernel management */
TaskHandle_t xMonitorHandle = NULL;

/* HELPER: Function to save logs in RAM (Zero Jitter) */
void vLogEvent(const char* pcName, LogEventType_t xType) {
    /* Protection against buffer overflow */
    if (usEventIndex < MAX_LOG_EVENTS) {
        xEventBuffer[usEventIndex].timestamp = ulGlobalTimeMs;
        xEventBuffer[usEventIndex].task_name = pcName;
        xEventBuffer[usEventIndex].event_type = xType;
        usEventIndex++;
    }
}

/* CPU LOAD SIMULATION */
void vBurnCPU( TickType_t xTicksToWait ) {
    TickType_t xCurrentTick;
    TickType_t xPreviousTick = xTaskGetTickCount();
    TickType_t xActiveTicksWaited = 0;

    while( xActiveTicksWaited < xTicksToWait )
    {
        xCurrentTick = xTaskGetTickCount();
        
        // Check if a tick boundary has been crossed
        if( xCurrentTick != xPreviousTick )
        {
            xActiveTicksWaited++;
            xPreviousTick = xCurrentTick;
        }
    }
}

/* TASK IMPLEMENTATION */

/* Standard Hard Real-Time (HRT) Task */
void vTask_Normal_HRT( void *pvParameters ) {
    const char* pcName = pcTaskGetName( NULL );
    /* The parameter passed during task creation determines the workload */
    TickType_t xWorkTicks =  ( TickType_t ) pvParameters;

    for( ;; ) {
        /* 1. Record the current cycle */
        uint32_t ulCycleAtStart = ulKernelCycleCount;

        vLogEvent(pcName, EVENT_TASK_RUNNING);
        
        /* 2. Workload */
        vBurnCPU( xWorkTicks );
        
        /* 3. Anti-Zombie Check: prevents issues if the task finishes too close to the new cycle.
        If the cycle hasn't changed, I finished on time. I suspend myself waiting for the next round. */
        if( ulCycleAtStart == ulKernelCycleCount ) {
            vLogEvent(pcName, EVENT_TASK_END);
            vTaskSuspend( NULL ); 
        } 
        else {
            /* The kernel just woke me up for the new cycle.
             I skip vTaskSuspend and use 'continue' to start working immediately.
             */
            continue; 
        }
    }
}

/* Standard Soft Real-Time (SRT) Task */
void vTask_SRT_Generic( void *pvParameters ) {
    const char* pcName = pcTaskGetName( NULL );
    TickType_t xWorkTicks =  ( TickType_t ) pvParameters;
    
    for( ;; ) {
        vLogEvent(pcName, EVENT_TASK_RUNNING);
        vBurnCPU( xWorkTicks ); 
        vLogEvent(pcName, EVENT_TASK_END);
        
        /* SRT tasks must run one after another during idle time.
        Before suspending, this task is responsible for calling the next one. */
        vWakeNextSRT();
        
        vTaskSuspend( NULL );
    }
}

/* Task specifically created to miss deadlines and test kernel robustness */
void vTask_Stress_Criminal( void *pvParameters ) {
    const char* pcName = pcTaskGetName( NULL );
    for( ;; ) {
        vLogEvent(pcName, EVENT_TASK_RUNNING); /* WORKING... */
        
        /* Works normally until millisecond 20... */
        while( ulGlobalTimeMs < 20 ) { __asm("nop"); }

        /* ...then enters an infinite loop. The deadline check injected 
        in tasks.c will intervene here to force termination. */
        while(1) { 
            if ( ulGlobalTimeMs < 15 ) break; 
            __asm("nop"); 
        }
    }
}

/* ----------------------------
    SCENARIO CONFIGURATION
----------------------------- */

#if defined(TEST_SCENARIO_STRESS)
    static TimelineTaskConfig_t xMyProjectTasks[] = {
        /* Task_A will miss the deadline, demonstrating the kernel's emergency intervention */
        { "Task_A",  vTask_Stress_Criminal, TASK_TYPE_HRT, 5, 10, 30, NULL }, 
        { "Task_B",  vTask_Normal_HRT,      TASK_TYPE_HRT, 5, 25, 35, (void*) 5 }, 
        { "Task_C",  vTask_SRT_Generic,     TASK_TYPE_SRT, 3, 0,  0,  (void*) 5 } 
    };
    #define NUM_TASKS 3
    #define TEST_MSG "--- SCENARIO: STRESS (OVERLAP) ---"
#elif defined(TEST_SCENARIO_EDGE)
    static TimelineTaskConfig_t xMyProjectTasks[] = {
        /* Task_A ends at 19 and Task_B starts at 20: we test context switch precision */
        { "Task_A",  vTask_Normal_HRT,    TASK_TYPE_HRT, 5, 10, 20, (void*) 9 },
        { "Task_B",  vTask_Normal_HRT,    TASK_TYPE_HRT, 5, 20, 40, (void*) 19 }, 
        { "Task_C",  vTask_SRT_Generic,   TASK_TYPE_SRT, 3, 0,  0,  (void*) 5 }
    };
    #define NUM_TASKS 3
    #define TEST_MSG "--- SCENARIO: EDGE CASE ---"
#else 
    static TimelineTaskConfig_t xMyProjectTasks[] = {
        /* HRT Tasks - Keep high priority (5) */
        { "Task_A",  vTask_Normal_HRT,    TASK_TYPE_HRT, 5, 10, 20, (void*) 5 },
        { "Task_B",  vTask_Normal_HRT,    TASK_TYPE_HRT, 5, 40, 50, (void*) 5 },
        
        /* SRT Tasks - Same priority (2), strictly executed in this order */
        { "Task_C",  vTask_SRT_Generic,   TASK_TYPE_SRT, 2, 0,  0,  (void*) 15 },
        { "Task_D",  vTask_SRT_Generic,   TASK_TYPE_SRT, 2, 0,  0,  (void*) 7 },
        { "Task_E",  vTask_SRT_Generic,   TASK_TYPE_SRT, 2, 0,  0,  (void*) 3 }
    };
    #define NUM_TASKS 5
    #define TEST_MSG "--- SCENARIO: STANDARD (5 TASKS) ---"
#endif

/* HELPER: Passing the SRT baton by sequentially reading the configuration array */

void vWakeNextSRT( void ) {
    TaskHandle_t xCurrentHandle = xTaskGetCurrentTaskHandle();
    
    /* 1. Find my current position (index) within the task array */
    for( int i = 0; i < NUM_TASKS; i++ ) {
        if( xMyProjectTasks[i].taskHandle == xCurrentHandle ) {
            
            /* 2. Starting from my position onwards, look for the first SRT type task */
            for( int j = i + 1; j < NUM_TASKS; j++ ) {
                if( xMyProjectTasks[j].type == TASK_TYPE_SRT ) {
                    
                    /* 1. Record the release event in the log */
                    vLogEvent( xMyProjectTasks[j].task_name, EVENT_KERNEL_RELEASE );
                    
                    /* 2. Wake up the task */
                    vTaskResume( xMyProjectTasks[j].taskHandle );

                    /* Exit the function to avoid waking up more than one */
                    return; 
                }
            }
            /* If we reach here, this was the last SRT task in the list. No one to wake up. */
            return; 
        }
    }
}

/* -----------------------------------------------------------------------------*/
/* MONITOR TASK: Pints logs and automatically evaluates the result of the test */
/* -----------------------------------------------------------------------------*/
void vMonitorTask( void *pvParameters ) {
    
    /* 1. It suspends itself right after being created */
    vTaskSuspend( NULL );

    printf("\n--- TEST EXECUTION FINISHED (3 FULL CYCLES) ---\n");
    printf("--- DUMPING RAM BUFFER LOGS ---\n\n");
    
    /* SELF-CHECKING TEST VARIABLES */
    uint16_t usDeadlineMissCount = 0;
    uint16_t usSrtRunCount = 0;
    
    /*
     DATA-DRIVEN LOGIC: ORDER OF SRT
    */
    const char* pcExpectedSrtSequence[10]; /* Array for saving the SRT order */
    uint8_t ucTotalSrtTasks = 0;

    /* Dynamic calculation of how many tasks are in the configuration */
    uint8_t ucNumTasks = sizeof(xMyProjectTasks) / sizeof(xMyProjectTasks[0]);

    /* We scan the configuration and save the names of the SRT in order */
    for( uint8_t i = 0; i < ucNumTasks; i++ ) {
        if( xMyProjectTasks[i].type == TASK_TYPE_SRT ) {
            pcExpectedSrtSequence[ucTotalSrtTasks] = xMyProjectTasks[i].task_name;
            ucTotalSrtTasks++;
        }
    }

    /* Variables to track the sequence at runtime */
    uint8_t ucCurrentExpectedSrtIndex = 0;
    uint8_t ucSrtSequenceError = 0;



    /* Empty the RAM buffer on the console and analyze the events */
    for( uint16_t i = 0; i < usEventIndex; i++ ) {
        const char* typeStr;

        switch( xEventBuffer[i].event_type ) {
            case EVENT_KERNEL_RELEASE: 
                typeStr = "RELEASED (Kernel)"; 
                break;
                
            case EVENT_TASK_RUNNING:   
                typeStr = "RUNNING  (Task)";
                
                /* Dynamic SRT control */
                if( ucTotalSrtTasks > 0 ) {
                    uint8_t isSrt = 0;
                    uint8_t foundIndex = 0;

                    /* Search for the name of the current task in the estractred SRT list */
                    for( uint8_t j = 0; j < ucTotalSrtTasks; j++ ) {
                        if( strcmp(xEventBuffer[i].task_name, pcExpectedSrtSequence[j]) == 0 ) {
                            isSrt = 1;
                            foundIndex = j;
                            break;
                        }
                    }

                    /* If the running task is an SRT, let's check if it's his turn */
                    if( isSrt == 1 ) {
                        usSrtRunCount++;
                        
                        /* Check the sequence only if there is more than one SRT to order */
                        if( ucTotalSrtTasks > 1 ) {
                            if( foundIndex == ucCurrentExpectedSrtIndex ) {
                                /* Correct order, we can wait for the next one (wrap-around) */
                                ucCurrentExpectedSrtIndex = (ucCurrentExpectedSrtIndex + 1) % ucTotalSrtTasks;
                            } else {
                                ucSrtSequenceError = 1;
                            }
                        }
                    }
                }
                break;
                
            case EVENT_TASK_END:       
                typeStr = "FINISHED (Task)";   
                break;
                
            case EVENT_DEADLINE_MISS:  
                typeStr = "!!! DEADLINE MISS !!!"; 
                usDeadlineMissCount++;
                break;
                
            case EVENT_CYCLE_END:      
                typeStr = "== CYCLE RESET =="; 
                break;
                
            default:                   
                typeStr = "UNKNOWN EVENT";
        }
        
        printf("[ %lu ms ] %s: %s\n", 
               xEventBuffer[i].timestamp, 
               xEventBuffer[i].task_name, 
               typeStr);
    }
    
    printf("\n--- END OF REPORT ---\n");

    /* =========================================================================
     AUTOMATED TEST EVALUATION
     * ========================================================================= */
    printf("\n==================================================\n");
    printf("               FINAL TEST VERDICT                 \n");
    printf("==================================================\n");

#if defined(TEST_SCENARIO_STRESS)
    if ( usDeadlineMissCount > 0 ) {
        printf("[SUCCESS] The kernel correctly caught %u deadline miss(es).\n", usDeadlineMissCount);
        printf("          The criminal task was successfully neutralized!\n");
    } else {
        printf("[FAILED]  No deadline misses detected!\n");
        printf("          The criminal task bypassed the kernel's protection.\n");
    }
#elif defined(TEST_SCENARIO_EDGE)
    if ( usDeadlineMissCount == 0 ) {
        printf("[SUCCESS] 0 Deadline Misses detected.\n");
        printf("          The edge-case timeline was perfectly respected.\n");
    } else {
        printf("[FAILED]  %u Deadline Miss(es) detected!\n", usDeadlineMissCount);
        printf("          The system failed the 0ms gap Context Switch.\n");
    }
#else 
    if ( usDeadlineMissCount == 0 ) {
        printf("[SUCCESS] 0 Deadline Misses detected. Timeline respected.\n");
    } else {
        printf("[FAILED]  %u Deadline Miss(es) detected!\n", usDeadlineMissCount);
    }
#endif

    /* Dynamic verdict on SRT validity for all scenarios */
    if ( usSrtRunCount == 0 ) {
        printf("[WARNING] SRT Starvation detected! SRT tasks never ran.\n");
    } else if ( ucTotalSrtTasks > 1 ) {
        if ( ucSrtSequenceError == 0 ) {
            printf("[SUCCESS] SRT execution order perfectly respected.\n");
        } else {
            printf("[FAILED]  SRT execution order violated! Check daisy-chaining.\n");
        }
    } else if ( ucTotalSrtTasks == 1 ) {
        printf("[INFO]    Only 1 SRT task configured. Sequence check bypassed.\n");
    }

    printf("==================================================\n\n");
    
    printf("Shutting down QEMU...\n");
    fflush(stdout);
    
    exit(0); 
}

/* MAIN ENTRY POINT */
int main( void )
{
    initialise_monitor_handles();
    
    printf("%s\n", TEST_MSG);
    printf("Real-time logging active. Executing 3 cycles...\n");
    fflush(stdout);

    /* ASSIGN PRIORITY 6 and save the handle in our global variable */
    xTaskCreate(vMonitorTask, "Monitor", configMINIMAL_STACK_SIZE * 2, NULL, 6, &xMonitorHandle);

    /* Initialization of the task table and binding of kernel variables */
    vConfigureScheduler( xMyProjectTasks, NUM_TASKS );

    vTaskStartScheduler();
    
    for( ;; );
    return 0;
}


void vApplicationTickHook( void ) { }
void vApplicationIdleHook( void ) { __asm("nop"); }
void vApplicationMallocFailedHook( void ) { while(1); }
void vAssertCalled( const char * const pcFileName, unsigned long ulLine ) { while(1); }