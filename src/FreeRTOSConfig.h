#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOS configuration for LM3S6965 Cortex-M3 and Precise Scheduler */

/* Scheduling & System Mode */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     1
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 20000000 ) /* 20MHz for QEMU Stellaris */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )        /* 1ms tick */
#define configMAX_PRIORITIES                    ( 7 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 1024 )    /* Stack size for printf */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 50 * 1024 ) )   /* 50KB heap */
#define configMAX_TASK_NAME_LEN                 ( 12 )
#define configUSE_TRACE_FACILITY                1
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TIME_SLICING                  0   /* Disabled for deterministic SRT execution */

/* Synchronization & Resource Management */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               20
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_APPLICATION_TASK_TAG          0
#define configGENERATE_RUN_TIME_STATS           0

/* Co-routines */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/* Software Timers */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                5
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* API Inclusion Configuration */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1   /* Needed to suspend tasks */
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetSchedulerState          1

/* Debugging */
// extern void vAssertCalled( const char * const pcFileName, unsigned long ulLine );
// #define configASSERT( x ) if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )

/* Cortex-M3 interrupt configuration */
#ifdef configPRIO_BITS
    #undef configPRIO_BITS
#endif
#define configPRIO_BITS       3

/* Lowest interrupt priority */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         0x07

/* Max syscall interrupt priority */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    1

/* Priority macros (do not modify) */
#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#endif /* FREERTOS_CONFIG_H */