// File: TwoTasks.c 

#include <stdio.h>
#include "includes.h"
#include <string.h>

#define DEBUG 1

/* Definition of Task Stacks */
/* Stack grows from HIGH to LOW memory */
#define   TASK_STACKSIZE       2048
OS_STK    task1_stk[TASK_STACKSIZE];
OS_STK    task2_stk[TASK_STACKSIZE];
OS_STK    stat_stk[TASK_STACKSIZE];

// declare semaphore
OS_EVENT *semaphore1;
OS_EVENT *semaphore2; 
OS_EVENT *semaphore3;

/* Definition of Task Priorities */
#define TASK1_PRIORITY      6  // highest priority
#define TASK2_PRIORITY      7
#define TASK_STAT_PRIORITY 12  // lowest priority

int task0_counter = 0;
int task1_counter = 0;

void printStackSize(char* name, INT8U prio){
    INT8U err;
    OS_STK_DATA stk_data;
    
    err = OSTaskStkChk(prio, &stk_data);
    if(err == OS_NO_ERR){
        if(DEBUG == 1)
            printf("%s (priority %d) - Used: %d; Free: %d\n", name, prio, stk_data.OSUsed, stk_data.OSFree);
    }
    else{
        if(DEBUG == 1)
	    printf("Stack Check Error!\n");    
    }
}

/* Prints a message and sleeps for given time interval */
void task1(void* pdata){
    INT8U err;

    while(1){ 
        char text1[] = "Hello from Task1\n";
        int i;
        
        OSSemPend(semaphore1, 0, &err); /* void OSSemPend(OS_EVENT *pevent, INT16U timeout, INT8U *perr);
                                           used when a task wants exclusive access to a resource, and needs to synchronize its activities
        
                                           arg: pevent- a pointer to the semaphore
                                           timeout- 
                                           perr- a pointer to a variable used to hold an error code
                                           return: none */
                                           
        printf("Task 0 - State %d\n", task0_counter % 2);
        task0_counter++;
        
        OSSemPost(semaphore2); /* INT8U OSSemPost(OS_EVENT *pevent);
                          	  a semaphore is signaled by calling the function
                          	  
                                  arg: pevent- a pointer to the semaphore
                                  return: an error code */
        
        // OSTimeDlyHMSM(0, 0, 0, 11);
        
        /* Context Switch to next task
         * Task will go to the ready state
         * after the specified delay
         */
    }
}

/* Prints a message and sleeps for given time interval */
void task2(void* pdata){
    INT8U err;

    while(1){ 
        char text2[] = "Hello from Task2\n";
        int i;

        OSSemPend(semaphore2, 0, &err);
        
        printf("Task 1 - State %d\n", task1_counter % 2);
        task1_counter++;
            
        OSSemPost(semaphore3);
        
        // OSTimeDlyHMSM(0, 0, 0, 4);
    }
}

/* Printing Statistics */
void statisticTask(void* pdata){
    INT8U err;

    while(1){
        OSSemPend(semaphore3, 0, &err);
    
        printStackSize("Task1", TASK1_PRIORITY);
        printStackSize("Task2", TASK2_PRIORITY);
        printStackSize("StatisticTask", TASK_STAT_PRIORITY);
        
        OSSemPost(semaphore1);
    }
}

/* The main function creates two task and starts multi-tasking */
int main(void){
    printf("Lab 3 - Two Tasks\n");
    
    /* OS_EVENT *OSSemCreate(INT16U value);
       create and initializes a semaphore
       
       arg: an initial value of the semaphore
       return: a pointer to the event control block allocated to the semaphore */
    semaphore1 = OSSemCreate(1); // binary semaphore
    semaphore2 = OSSemCreate(0);
    semaphore3 = OSSemCreate(0);

    OSTaskCreateExt
      ( task1,                        // Pointer to task code
        NULL,                         // Pointer to argument passed to task
        &task1_stk[TASK_STACKSIZE-1], // Pointer to top of task stack
        TASK1_PRIORITY,               // Desired Task priority
        TASK1_PRIORITY,               // Task ID
        &task1_stk[0],                // Pointer to bottom of task stack
        TASK_STACKSIZE,               // Stacksize
        NULL,                         // Pointer to user supplied memory (not needed)
        OS_TASK_OPT_STK_CHK |         // Stack Checking enabled 
        OS_TASK_OPT_STK_CLR           // Stack Cleared                                 
        );
	   
    OSTaskCreateExt
      ( task2,                        // Pointer to task code
        NULL,                         // Pointer to argument passed to task
        &task2_stk[TASK_STACKSIZE-1], // Pointer to top of task stack
        TASK2_PRIORITY,               // Desired Task priority
        TASK2_PRIORITY,               // Task ID
        &task2_stk[0],                // Pointer to bottom of task stack
        TASK_STACKSIZE,               // Stacksize
        NULL,                         // Pointer to user supplied memory (not needed)
        OS_TASK_OPT_STK_CHK |         // Stack Checking enabled 
        OS_TASK_OPT_STK_CLR           // Stack Cleared                       
        );  

    if(DEBUG == 1){
        OSTaskCreateExt
	  ( statisticTask,                // Pointer to task code
	    NULL,                         // Pointer to argument passed to task
	    &stat_stk[TASK_STACKSIZE-1],  // Pointer to top of task stack
	    TASK_STAT_PRIORITY,           // Desired Task priority
	    TASK_STAT_PRIORITY,           // Task ID
	    &stat_stk[0],                 // Pointer to bottom of task stack
	    TASK_STACKSIZE,               // Stacksize
	    NULL,                         // Pointer to user supplied memory (not needed)
	    OS_TASK_OPT_STK_CHK |         // Stack Checking enabled 
	    OS_TASK_OPT_STK_CLR           // Stack Cleared                              
	    );
    }  

    OSStart();
    return 0;
}
