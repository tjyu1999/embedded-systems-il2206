// File: Handshake.c 

# include <stdio.h>
# include "includes.h"
# include <string.h>
# include <altera_avalon_performance_counter.h>

# define DEBUG 1

/* Definition of Task Stacks */
/* Stack grows from HIGH to LOW memory */
# define TASK_STACKSIZE 2048
OS_STK task1_stk[TASK_STACKSIZE];
OS_STK task2_stk[TASK_STACKSIZE];
OS_STK stat_stk[TASK_STACKSIZE];

OS_EVENT* semaphore1;
OS_EVENT* semaphore2;

/* Definition of Task Priorities */
# define TASK1_PRIORITY      6 // highest priority
# define TASK2_PRIORITY      7
# define TASK_STAT_PRIORITY 12 // lowest priority

int prev_average = 0;
int average = 0;
int counter = 0;

void printStackSize(char* name, INT8U prio){
    INT8U err;
    OS_STK_DATA stk_data;
    
    err = OSTaskStkChk(prio, &stk_data);
    if(err == OS_NO_ERR){
        if(DEBUG == 1) printf("%s (priority %d) - Used: %d; Free: %d\n", name, prio, (int) stk_data.OSUsed, (int) stk_data.OSFree);
    }
    else{
        if(DEBUG == 1) printf("Stack Check Error!\n");    
    }
}

/* Prints a message and sleeps for given time interval */
void task1(void* pdata){
    INT8U err;
    int state = 0;
    int ticks;
    
    while(1){
        int prev_average = average;
        OSSemPend(semaphore1, 0, &err);

        while(1){
            // PERF_RESET(PERFORMANCE_COUNTER_BASE);
            printf("Task 0 - State %d\n", state);
            if(state == 0){
                state = 1;
                OSSemPost(semaphore2);

                // PERF_START_MEASURING(PERFORMANCE_COUNTER_BASE);
                PERF_BEGIN(PERFORMANCE_COUNTER_BASE, 1);
                break;
            }
            else state = 0;
        }

        printf("\n");
        ticks = perf_get_section_time((void*) PERFORMANCE_COUNTER_BASE, 1);
        average = (average * counter + ticks) / (counter + 1);
        counter++;
        if((float)ticks > average * 2.0){
            printf("WARNING: Detect outlier!!!\n");
            average = prev_average;
        }

        printf("Context switch time        : %d\n", ticks);
        printf("Average context swtich time: %d\n\n", average);
    }
}

/* Prints a message and sleeps for given time interval */
void task2(void* pdata){
    INT8U err;
    int state = 0;

    while(1){
        OSSemPend(semaphore2, 0, &err);
        PERF_END(PERFORMANCE_COUNTER_BASE, 1);
        // PERF_STOP_MEASURING(PERFORMANCE_COUNTER_BASE);

        while(1){
            printf("Task 1 - State %d\n", state);
            if(state == 1){
                state = 0;
                OSSemPost(semaphore1);
                break;
            }
            else state = 1;
        }
    }
}

/* Printing Statistics */
void statisticTask(void* pdata){
    while(1){
        printStackSize("Task1", TASK1_PRIORITY);
        printStackSize("Task2", TASK2_PRIORITY);
        printStackSize("StatisticTask", TASK_STAT_PRIORITY);
    }
}

/* The main function creates two task and starts multi-tasking */
int main(void){
    printf("Lab 3 - Two Tasks\n");
    
    semaphore1 = OSSemCreate(1);
    semaphore2 = OSSemCreate(0);

    OSTaskCreateExt(task1,                          // Pointer to task code
                    NULL,                           // Pointer to argument passed to task
                    &task1_stk[TASK_STACKSIZE - 1], // Pointer to top of task stack
                    TASK1_PRIORITY,                 // Desired Task priority
                    TASK1_PRIORITY,                 // Task ID
                    &task1_stk[0],                  // Pointer to bottom of task stack
                    TASK_STACKSIZE,                 // Stacksize
                    NULL,                           // Pointer to user supplied memory (not needed)
                    OS_TASK_OPT_STK_CHK |           // Stack Checking enabled 
                    OS_TASK_OPT_STK_CLR);           // Stack Cleared
	   
    OSTaskCreateExt(task2,                          // Pointer to task code
                    NULL,                           // Pointer to argument passed to task
                    &task2_stk[TASK_STACKSIZE - 1], // Pointer to top of task stack
                    TASK2_PRIORITY,                 // Desired Task priority
                    TASK2_PRIORITY,                 // Task ID
                    &task2_stk[0],                  // Pointer to bottom of task stack
                    TASK_STACKSIZE,                 // Stacksize
                    NULL,                           // Pointer to user supplied memory (not needed)
                    OS_TASK_OPT_STK_CHK |           // Stack Checking enabled 
                    OS_TASK_OPT_STK_CLR);           // Stack Cleared

    if(DEBUG == 1){
        OSTaskCreateExt(statisticTask,                 // Pointer to task code
	                NULL,                          // Pointer to argument passed to task
	                &stat_stk[TASK_STACKSIZE - 1], // Pointer to top of task stack
	                TASK_STAT_PRIORITY,            // Desired Task priority
	                TASK_STAT_PRIORITY,            // Task ID
	                &stat_stk[0],                  // Pointer to bottom of task stack
	                TASK_STACKSIZE,                // Stacksize
	                NULL,                          // Pointer to user supplied memory (not needed)
	                OS_TASK_OPT_STK_CHK |          // Stack Checking enabled 
	                OS_TASK_OPT_STK_CLR);          // Stack Cleared	    
    }  

    OSStart();
    return 0;
}
