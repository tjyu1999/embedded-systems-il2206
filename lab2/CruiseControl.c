/* Cruise control skeleton for the IL 2206 embedded lab
 *
 * Maintainers:  Rodolfo Jordao (jordao@kth.se), George Ungereanu (ugeorge@kth.se)
 *
 * Description:
 *
 *   In this file you will find the "model" for the vehicle that is being simulated on top
 *   of the RTOS and also the stub for the control task that should ideally control its
 *   velocity whenever a cruise mode is activated.
 *
 *   The missing functions and implementations in this file are left as such for
 *   the students of the IL2206 course. The goal is that they get familiriazed with
 *   the real time concepts necessary for all implemented herein and also with Sw/Hw
 *   interactions that includes HAL calls and IO interactions.
 *
 *   If the prints prove themselves too heavy for the final code, they can
 *   be exchanged for alt_printf where hexadecimals are supported and also
 *   quite readable. This modification is easily motivated and accepted by the course
 *   staff.
 */

# include <stdio.h>
# include "system.h"
# include "includes.h"
# include "altera_avalon_pio_regs.h"
# include "sys/alt_irq.h"
# include "sys/alt_alarm.h"

# define DEBUG 1

# define HW_TIMER_PERIOD 100 /* 100ms */

/* Button Patterns */
# define GAS_PEDAL_FLAG      0x08
# define BRAKE_PEDAL_FLAG    0x04
# define CRUISE_CONTROL_FLAG 0x02

/* Switch Patterns */
# define ENGINE_FLAG   0x00000001
# define TOP_GEAR_FLAG 0x00000002

# define SWITCH_4      0x00000010
# define SWITCH_5      0x00000020
# define SWITCH_6      0x00000040
# define SWITCH_7      0x00000080
# define SWITCH_8      0x00000100
# define SWITCH_9      0x00000200

/* LED Patterns */
# define LED_RED_0   0x00000001 // Engine
# define LED_RED_1   0x00000002 // Top Gear

# define LED_GREEN_0 0x0001 // Cruise Control activated
# define LED_GREEN_2 0x0002 // Cruise Control Button
# define LED_GREEN_4 0x0010 // Brake Pedal
# define LED_GREEN_6 0x0040 // Gas Pedal

# define LED_RED_12  0x00001000
# define LED_RED_13  0x00002000
# define LED_RED_14  0x00004000
# define LED_RED_15  0x00008000
# define LED_RED_16  0x00010000
# define LED_RED_17  0x00020000

/*
 * Definition of Tasks
 */
# define TASK_STACKSIZE 2048

OS_STK StartTask_Stack[TASK_STACKSIZE]; 
OS_STK ControlTask_Stack[TASK_STACKSIZE]; 
OS_STK VehicleTask_Stack[TASK_STACKSIZE];
OS_STK ButtonIOTask_Stack[TASK_STACKSIZE];
OS_STK SwitchIOTask_Stack[TASK_STACKSIZE];
OS_STK WatchdogTask_Stack[TASK_STACKSIZE];
OS_STK DetectionTask_Stack[TASK_STACKSIZE];
OS_STK ExtraloadTask_Stack[TASK_STACKSIZE];

// Task Priorities
# define STARTTASK_PRIO      5
# define VEHICLETASK_PRIO   10
# define CONTROLTASK_PRIO   12
# define BUTTONIOTASK_PRIO   7
# define SWITCHIOTASK_PRIO   8
# define WATCHDOGTASK_PRIO   6
# define DETECTIONTASK_PRIO 14
# define EXTRALOADTASK_PRIO 13

// Task Periods
# define CONTROL_PERIOD   300
# define VEHICLE_PERIOD   300
# define BUTTON_PERIOD    300
# define SWITCH_PERIOD    300
# define WATCHDOG_PERIOD  300
# define DETECTION_PERIOD 300
# define EXTRALOAD_PERIOD 300

/*
 * Definition of Kernel Objects 
 */

// Mailboxes
OS_EVENT* Mbox_Throttle;
OS_EVENT* Mbox_Velocity;
OS_EVENT* Mbox_Brake;
OS_EVENT* Mbox_Engine;

OS_EVENT* Mbox_Utilization_Signal;
OS_EVENT* Mbox_Timeout_Signal;

// Semaphores
OS_EVENT* vehicle_semaphore;
OS_EVENT* control_semaphore;
OS_EVENT* button_semaphore;
OS_EVENT* switch_semaphore;
OS_EVENT* watchdog_semaphore;
OS_EVENT* detection_semaphore;
OS_EVENT* extraload_semaphore;

// SW-Timer
OS_TMR* vehicle_timer;
OS_TMR* control_timer;
OS_TMR* button_timer;
OS_TMR* switch_timer;
OS_TMR* watchdog_timer;
OS_TMR* detection_timer;
OS_TMR* extraload_timer;

/*
 * Types
 */
enum active{on = 2, off = 1};

enum active CRUISE_CONTROL = off;
enum active BRAKE_PEDAL = off;
enum active GAS_PEDAL = off;
enum active ENGINE = off;
enum active TOP_GEAR = off;

enum active switch4 = off;
enum active switch5 = off;
enum active switch6 = off;   
enum active switch7 = off;
enum active switch8 = off;
enum active switch9 = off;

/*
 * Global variables
 */
int delay;            // Delay of HW-timer 
INT16U led_green = 0; // Green LEDs
INT32U led_red = 0;   // Red LEDs

int utilization[6];

/*
 * Helper functions
 */
int buttons_pressed(void) return ~IORD_ALTERA_AVALON_PIO_DATA(D2_PIO_KEYS4_BASE);

int switches_pressed(void) return IORD_ALTERA_AVALON_PIO_DATA(DE2_PIO_TOGGLES18_BASE);

/*
 * ISR for HW Timer
 */
alt_u32 alarm_handler(void* context){
    OSTmrSignal(); /* Signals a 'tick' to the SW timers */
    return delay;
}

static int b2sLUT[] = {0x40,  //0
                       0x79,  //1
                       0x24,  //2
                       0x30,  //3
                       0x19,  //4
                       0x12,  //5
                       0x02,  //6
                       0x78,  //7
                       0x00,  //8
                       0x18,  //9
                       0x3F}; //-

/*
 * convert int to seven segment display format
 */
int int2seven(int inval)return b2sLUT[inval];

/*
 * output current velocity on the seven segement display
 */
void show_velocity_on_sevenseg(INT8S velocity){
    int tmp = velocity;
    int out;
    INT8U out_high = 0;
    INT8U out_low = 0;
    INT8U out_sign = 0;

    if(velocity < 0){
        out_sign = int2seven(10);
        tmp *= -1;
    }
    else out_sign = int2seven(0);

    out_high = int2seven(tmp / 10);
    out_low = int2seven(tmp - (tmp / 10) * 10);
    out = int2seven(0) << 21 |
          out_sign << 14 |
          out_high << 7  |
          out_low;
    
    IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_LOW28_BASE, out);
}

/*
 * shows the target velocity on the seven segment display (HEX5, HEX4)
 * when the cruise control is activated (0 otherwise)
 */
void show_target_velocity(INT8U target_vel){
    int tmp = target_vel;
    int out;
    INT8U out_high = 0;
    INT8U out_low = 0;

    out_high = int2seven(tmp / 10);
    out_low = int2seven(tmp - (tmp / 10) * 10);
    out = int2seven(0) << 21 |
          int2seven(0) << 14 |
          out_high << 7  |
          out_low;
    
    IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_HEX_HIGH28_BASE, out);
}

/*
 * indicates the position of the vehicle on the track with the four leftmost red LEDs
 * LEDR17: [0m, 400m)
 * LEDR16: [400m, 800m)
 * LEDR15: [800m, 1200m)
 * LEDR14: [1200m, 1600m)
 * LEDR13: [1600m, 2000m)
 * LEDR12: [2000m, 2400m]
 */
void show_position(INT16U position){
    int rled = 0;

    if(position < 400) rled = LED_RED_17;
    else if(position < 800) rled = LED_RED_16;
    else if(position < 1200) rled = LED_RED_15;
    else if(position < 1600) rled = LED_RED_14;
    else if(position < 2000) rled = LED_RED_13;
    else if(position < 2400) rled = LED_RED_12;

    if(ENGINE == on){
        if(TOP_GEAR == on) IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE, LED_RED_0 | LED_RED_1 | rled);
        else IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_REDLED18_BASE, LED_RED_0 | rled);
    }
    else{
        if(TOP_GEAR == on) IOWR_ALTERA_AVALON_PIO_DATA (DE2_PIO_REDLED18_BASE, LED_RED_1 | rled);
        else IOWR_ALTERA_AVALON_PIO_DATA (DE2_PIO_REDLED18_BASE, rled);
    }
}

void timer_callback(void* ptmr, void* callback_arg) OSSemPost((OS_EVENT*) callback_arg);

/*
 * The task 'VehicleTask' is the model of the vehicle being simulated. It updates variables like
 * acceleration and velocity based on the input given to the model.
 * 
 * The car model is equivalent to moving mass with linear resistances acting upon it.
 * Therefore, if left one, it will stably stop as the velocity converges to zero on a flat surface.
 * You can prove that easily via basic LTI systems methods.
 */
void VehicleTask(void* pdata){
    // constants that should not be modified
    const unsigned int wind_factor = 1;
    const unsigned int brake_factor = 4;
    const unsigned int gravity_factor = 2;
    
    // variables relevant to the model and its simulation on top of the RTOS
    INT8U err;
    void* msg;
    INT8U* throttle;
    INT16S acceleration;
    INT16U position = 0;
    INT16S velocity = 0;
    
    enum active brake_pedal = off;
    enum active engine = off;

    while(1){
        OSSemPend(vehicle_semaphore, 0, &err);
        err = OSMboxPost(Mbox_Velocity, (void*) &velocity);

        /* Non-blocking read of mailbox: 
           - message in mailbox: update throttle
           - no message:         use old throttle
         */
        msg = OSMboxPend(Mbox_Throttle, 1, &err); 
        if(err == OS_NO_ERR) throttle = (INT8U*) msg;
        /* Same for the brake signal that bypass the control law */
        msg = OSMboxPend(Mbox_Brake, 1, &err); 
        if(err == OS_NO_ERR) brake_pedal = (enum active) msg;
        /* Same for the engine signal that bypass the control law */
        msg = OSMboxPend(Mbox_Engine, 1, &err); 
        if(err == OS_NO_ERR) engine = (enum active) msg;

        // vehichle cannot effort more than 80 units of throttle
        if(*throttle > 80) *throttle = 80;

        // brakes + wind
        if(brake_pedal == off){
            // wind resistance
            acceleration = -wind_factor * velocity;
            // actuate with engines
            if(engine == on) acceleration += (*throttle);

            // gravity effects
            if(400 <= position && position < 800) acceleration -= gravity_factor;            // traveling uphill
            else if(800 <= position && position < 1200) acceleration -= 2 * gravity_factor;  // traveling steep uphill
            else if(1600 <= position && position < 2000) acceleration += 2 * gravity_factor; // traveling downhill
            else if(2000 <= position) acceleration += gravity_factor;                        // traveling steep downhill
        }
        // if the engine and the brakes are activated at the same time,
        // we assume that the brake dynamics dominates, so both cases fall here.
        else acceleration = -brake_factor * velocity;

        printf("Position: %d m\n", position);
        printf("Velocity: %d m/s\n", velocity);
        printf("Accell  : %d m/s2\n", acceleration);
        printf("Throttle: %d V\n\n", *throttle);

        position = position + velocity * VEHICLE_PERIOD / 1000;
        velocity = velocity  + acceleration * VEHICLE_PERIOD / 1000.0;
        // reset the position to the beginning of the track
        if(position > 2400) position = 0;

        show_position(position);
        show_velocity_on_sevenseg((INT8S) velocity);
    }
} 

/*
 * The task 'ControlTask' is the main task of the application. It reacts
 * on sensors and generates responses.
 */

void ControlTask(void* pdata){
    INT8U err;
    INT8U throttle = 20; /* Value between 0 and 80, which is interpreted as between 0.0V and 8.0V */
    void* msg;
    INT16S* current_velocity;
    INT16S prev_velocity = 0;
    INT16S target_velocity = 0;
    INT16S deviation_velocity;
      
    enum active engine = off;
    enum active cruise_control = off;
    enum active prev_cruise_ontrol = off;

    // printf("Control Task created!\n");

    while(1){
        OSSemPend(control_semaphore, 0, &err);
        msg = OSMboxPend(Mbox_Velocity, 0, &err);
        current_velocity = (INT16S *) msg;

        // Here you can use whatever technique or algorithm that you prefer to control
        // the velocity via the throttle. There are no right and wrong answer to this controller, so
        // be free to use anything that is able to maintain the cruise working properly. You are also
        // allowed to store more than one sample of the velocity. For instance, you could define
        //
        // INT16S previous_vel;
        // INT16S pre_previous_vel;
        // ...
        //
        // If your control algorithm/technique needs them in order to function.
        
        if(engine != ENGINE){
            engine = ENGINE;
            err = OSMboxPost(Mbox_Engine, (void*) engine);
        }
        
        throttle = 0;
        
        if(CRUISE_CONTROL != prev_cruise_control && CRUISE_CONTROL == on){
            if(cruise_control == on) cruise_control = off;
            else cruise_control = on;
        }
        
        if(cruise_control == on){
            if((gas_pedal == off) && (brake_pedal == off) && (top_gear == on) && (*current_velocity >= 20)){   
                if(*current_velocity >= 25){
	            deviation_velocity = *current_velocity - prev_velocity;
	            if(deviation_velocity < -4) throttle = 80;
	            else if(deviation_velocity > 4) throttle = 60;
	        }
	        
                if(CRUISE_CONTROL != prev_cruise_control) target_velocity = *current_velocity;
                show_target_velocity(*current_velocity);        
                prev_cruise_control = CRUISE_CONTROL;
            }
            else{
                cruise_control = off;
                prev_cruise_control = off;

                if(gas_pedal == on) throttle = 80;
                else if(brake_pedal == on) throttle = 0;
                show_target_velocity((INT8U) 0);
            }
        }
        else{
            prev_cruise_control = CRUISE_CONTROL;
        
            if(gas_pedal == on) throttle = 80;
            else if(brake_pedal == on) throttle = 0;
            show_target_velocity((INT8U) 0);
        }
        
        led_green = (BRAKE_PEDAL == on ? LED_GREEN_4 : 0) | ((GAS_PEDAL == on ? LED_GREEN_6 : 0 ) | (CRUISE_CONTROL == on ? LED_GREEN_2 : 0));
        led_green = (led_green & (~LED_GREEN_0)) | (cruise_control == on ? LED_GREEN_0 : 0);
        led_red = (TOP_GEAR == on ? LED_RED_1 : 0) | (ENGINE == on ? LED_RED_0 : 0);
        IOWR_ALTERA_AVALON_PIO_DATA(DE2_PIO_GREENLED9_BASE, led_green);

        err = OSMboxPost(Mbox_Throttle, (void*) &throttle);
    }
}

void ButtonIOTask(void* pdata){
    INT8U err;
    int buttons;
    
    enum active prev_brake_pedal = off;

    while(1){
        OSSemPend(button_semaphore, 0, &err);
        buttons = buttons_pressed();
        prev_brake_pedal = BRAKE_PEDAL;
        
        // cruise control
        if(buttons & CRUISE_CONTROL_FLAG) CRUISE_CONTROL = on;
        else CRUISE_CONTROL = off;
        
        // brake pedal
        if(buttons & BRAKE_PEDAL_FLAG) BRAKE_PEDAL = on;
        else BRAKE_PEDAL = off;
        
        if(BRAKE_PEDAL != prev_brake_pedal) err = OSMboxPost(Mbox_Brake, (void*) BRAKE_PEDAL);

        // gas pedal        
        if(buttons & GAS_PEDAL_FLAG) GAS_PEDAL = on;
        else GAS_PEDAL = off;
    }
}

void SwitchIOTask(void* pdata){
    INT8U err;
    int switches;

    while(1){
        OSSemPend(switch_semaphore, 0, &err);
        switches = switches_pressed();

        // engine
        if(switches & ENGINE_FLAG) ENGINE = on;
        else ENGINE = off;

        // top gear
        if(switches & TOP_GEAR_FLAG) TOP_GEAR = on;
        else TOP_GEAR = on;
        
        // sw4
        if(switches & SWITCH_4){
            switch4 = on;
            utilization[5] = 2;
        }
        else{
            switch4 = off;
            utilization[5] = 0;
        }
        
        // sw5
        if(switches & SWITCH_5){
            switch5 = on;
            utilization[4] = 4;
        }
        else{
            switch5 = off;
            utilization[4] = 0;
        }
        
        // sw6
        if(switches & SWITCH_6){
            switch6 = on;
            utilization[3] = 8;
        }
        else{
            switch6 = off;
            utilization[3] = 0;
        }

        // sw7
        if(switches & SWITCH_7){
            switch7 = on;
            utilization[2] = 16;
        }
        else{
            switch7 = off;
            utilization[2] = 0;
        }
        
        // sw8
        if(switches & SWITCH_8){
            switch8 = on;
            utilization[1] = 32;
        }
        else{
            switch8 = off;
            utilization[1] = 0;
        }
        
        // sw9
        if(switches & SWITCH_9){
            switch9 = on;
            utilization[0] = 64;
        }
        else{
            switch9 = off;
            utilization[0] = 0;
        }
    }
}

void WatchdogTask(void* pdata){
    INT8U err;
    void* msg;
    
    while(1){
        OSSemPend(watchdog_semaphore, 0, &err);
        
        msg = OSMboxPend(Mbox_Utilization_Signal, 0, &err);
        if((int) (int*) msg) printf("WARNING: Overload!!! (utilization)\n\n");
        
        msg = OSMboxPend(Mbox_Timeout_Signal, 5000, &err);
        if(err == OS_ERR_TIMEOUT) printf("WARNING: Overload!!! (timeout)\n\n");
    }
}

void DetectionTask(void* pdata){
    INT8U err;
    int signal = 1;
    
    while(1){
        OSSemPend(detection_semaphore, 0, &err);
        err = OSMboxPost(Mbox_Timeout_Signal, (void*) signal);
    }
}

void ExtraloadTask(void* pdata){
    INT8U err;
    
    while(1){
        OSSemPend(extraload_semaphore, 0, &err);   
        int total = 0;
        int signal = 0;
        
        for(int i = 0; i < 6; i++) total += utilization[i];
        printf("### Utilization: %d%% ###\n\n", total);
        if(total > 100) signal = 1;
        err = OSMboxPost(Mbox_Utilization_Signal, (void \*) signal);
    }
}

/* 
 * The task 'StartTask' creates all other tasks kernel objects and
 * deletes itself afterwards.
 */ 
void StartTask(void* pdata){
    INT8U err;
    void* context;
    static alt_alarm alarm; /* Is needed for timer ISR function */

    /* Base resolution for SW timer : HW_TIMER_PERIOD ms */
    delay = alt_ticks_per_second() * HW_TIMER_PERIOD / 1000; 
    printf("delay in ticks %d\n", delay);

    /* 
     * Create Hardware Timer with a period of 'delay' 
     */
    if(alt_alarm_start(&alarm, delay, alarm_handler, context) < 0) printf("No system clock available!\n");

    /* 
     * Create and start Software Timer 
     */
    
    vehicle_timer = OSTmrCreate(0,
                                VEHICLE_PERIOD / HW_TIMER_PERIOD,
                                OS_TMR_OPT_PERIODIC,
                                timer_callback,
                                (void*) vehicle_semaphore,
                                NULL,
                                &err);
    
    control_timer = OSTmrCreate(0,
                                CONTROL_PERIOD / HW_TIMER_PERIOD,
                                OS_TMR_OPT_PERIODIC,
                                timer_callback,
                                (void*) control_semaphore,
                                NULL,
                                &err);
    
    button_timer = OSTmrCreate(0,
                               BUTTON_PERIOD / HW_TIMER_PERIOD,
                               OS_TMR_OPT_PERIODIC,
                               timer_callback,
                               (void*) button_semaphore,
                               NULL,
                               &err);
    
    switch_timer = OSTmrCreate(0,
                               SWITCH_PERIOD / HW_TIMER_PERIOD,
                               OS_TMR_OPT_PERIODIC,
                               timer_callback,
                               (void*) switch_semaphore,
                               NULL,
                               &err);
    
    watchdog_timer = OSTmrCreate(0,
                                 WATCHDOG_PERIOD / HW_TIMER_PERIOD,
                                 OS_TMR_OPT_PERIODIC,
                                 timer_callback,
                                 (void*) watchdog_semaphore,
                                 NULL,
                                 &err);
    
    detection_timer = OSTmrCreate(0,
                                  DETECTION_PERIOD / HW_TIMER_PERIOD,
                                  OS_TMR_OPT_PERIODIC,
                                  timer_callback,
                                  (void*) detection_semaphore,
                                  NULL,
                                  &err);
    
    extraload_timer = OSTmrCreate(0,
                                  EXTRALOAD_PERIOD / HW_TIMER_PERIODs,
                                  OS_TMR_OPT_PERIODIC,
                                  timer_callback,
                                  (void*) extraload_semaphore,
                                  NULL,
                                  &err);
                                  
    OSTmrStart(vehicle_timer, &err);
    OSTmrStart(control_timer, &err);
    OSTmrStart(button_timer, &err);
    OSTmrStart(switch_timer, &err);
    OSTmrStart(watchdog_timer, &err);
    OSTmrStart(detection_timer, &err);
    OSTmrStart(extraload_timer, &err);
     
    /*
     * Creation of Kernel Objects
     */

    // Mailboxes
    Mbox_Throttle = OSMboxCreate((void*) 0); /* Empty Mailbox - Throttle */
    Mbox_Velocity = OSMboxCreate((void*) 0); /* Empty Mailbox - Velocity */
    Mbox_Brake = OSMboxCreate((void*) 1);    /* Empty Mailbox - Brake */
    Mbox_Engine = OSMboxCreate((void*) 1);   /* Empty Mailbox - Engine */

    Mbox_Utilization_Signal = OSMboxCreate((void*) 0);
    Mbox_Timeout_Signal = OSMboxCreate((void*) 0);
    
    vehicle_semaphore = OSSemCreate(1);
    control_semaphore = OSSemCreate(1);
    button_semaphore = OSSemCreate(1);
    switch_semaphore = OSSemCreate(1);
    watchdog_semaphore = OSSemCreate(1);
    detection_semaphore = OSSemCreate(1);
    extraload_semaphore = OSSemCreate(1);

    /*
     * Create statistics task
     */
    OSStatInit();

    /* 
     * Creating Tasks in the system 
     */

    err = OSTaskCreateExt(VehicleTask,                            // Pointer to task code
                          NULL,                                   // Pointer to argument that is passed to task
                          &VehicleTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack
                          VEHICLETASK_PRIO,
                          VEHICLETASK_PRIO,
                          (void*) &VehicleTask_Stack[0],
                          TASK_STACKSIZE,
                          (void*) 0,
                          OS_TASK_OPT_STK_CHK);
    
    err = OSTaskCreateExt(ControlTask,                            // Pointer to task code
                          NULL,                                   // Pointer to argument that is passed to task
                          &ControlTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack
                          CONTROLTASK_PRIO,
                          CONTROLTASK_PRIO,
                          (void*) &ControlTask_Stack[0],
                          TASK_STACKSIZE,
                          (void*) 0,
                          OS_TASK_OPT_STK_CHK);
        
    err = OSTaskCreateExt(ButtonIOTask,                            // Pointer to task code
                          NULL,                                    // Pointer to argument that is passed to task
                          &ButtonIOTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack
                          BUTTONIOTASK_PRIO,
                          BUTTONIOTASK_PRIO,
                          (void*) &ButtonIOTask_Stack[0],
                          TASK_STACKSIZE,
                          (void*) 0,
                          OS_TASK_OPT_STK_CHK);
        
    err = OSTaskCreateExt(SwitchIOTask,                            // Pointer to task code
                          NULL,                                    // Pointer to argument that is passed to task
                          &SwitchIOTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack
                          SWITCHIOTASK_PRIO,
                          SWITCHIOTASK_PRIO,
                          (void*) &SwitchIOTask_Stack[0],
                          TASK_STACKSIZE,
                          (void*) 0,
                          OS_TASK_OPT_STK_CHK);
        
    err = OSTaskCreateExt(WatchdogTask,                            // Pointer to task code
                          NULL,                                    // Pointer to argument that is passed to task
                          &WatchdogTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack
                          WATCHDOGTASK_PRIO,
                          WATCHDOGTASK_PRIO,
                          (void*) &WatchdogTask_Stack[0],
                          TASK_STACKSIZE,
                          (void*) 0,
                          OS_TASK_OPT_STK_CHK);
        
    err = OSTaskCreateExt(DetectionTask,                            // Pointer to task code
                          NULL,                                     // Pointer to argument that is passed to task
                          &DetectionTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack
                          DETECTIONTASK_PRIO,
                          DETECTIONTASK_PRIO,
                          (void*) &DetectionTask_Stack[0],
                          TASK_STACKSIZE,
                          (void*) 0,
                          OS_TASK_OPT_STK_CHK);
        
    err = OSTaskCreateExt(ExtraloadTask,                            // Pointer to task code
                          NULL,                                     // Pointer to argument that is passed to task
                          &ExtraloadTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack
                          EXTRALOADTASK_PRIO,
                          EXTRALOADTASK_PRIO,
                          (void*) &ExtraloadTask_Stack[0],
                          TASK_STACKSIZE,
                          (void*) 0,
                          OS_TASK_OPT_STK_CHK);

    printf("All Tasks and Kernel Objects generated!\n\n");

    /* Task deletes itself */
    OSTaskDel(OS_PRIO_SELF);
}

/*
 *
 * The function 'main' creates only a single task 'StartTask' and starts
 * the OS. All other tasks are started from the task 'StartTask'.
 *
 */
int main(void){
    printf("Lab: Cruise Control\n");

    OSTaskCreateExt(StartTask,                                    // Pointer to task code
                    NULL,                                         // Pointer to argument that is passed to task
                    (void*) &StartTask_Stack[TASK_STACKSIZE - 1], // Pointer to top of task stack 
                    STARTTASK_PRIO,
                    STARTTASK_PRIO,
                    (void*) &StartTask_Stack[0],
                    TASK_STACKSIZE,
                    (void*) 0,  
                    OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

    OSStart();
    return 0;
}
