/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wwrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*
FreeRTOS is a market leading RTOS from Real Time Engineers Ltd. that supports
31 architectures and receives 77500 downloads a year. It is professionally
developed, strictly quality controlled, robust, supported, and free to use in
commercial products without any requirement to expose your proprietary source
code.

This simple FreeRTOS demo does not make use of any IO ports, so will execute on
any Cortex-M3 of Cortex-M4 hardware.  Look for TODO markers in the code for
locations that may require tailoring to, for example, include a manufacturer
specific header file.

This is a starter project, so only a subset of the RTOS features are
demonstrated.  Ample source comments are provided, along with web links to
relevant pages on the http://www.FreeRTOS.org site.

Here is a description of the project's functionality:

The main() Function:
main() creates the tasks and software timers described in this section, before
starting the scheduler.

The Queue Send Task:
The queue send task is implemented by the prvQueueSendTask() function.
The task uses the FreeRTOS vTaskDelayUntil() and xQueueSend() API functions to
periodically send the number 100 on a queue.  The period is set to 200ms.  See
the comments in the function for more details.
http://www.freertos.org/vtaskdelayuntil.html
http://www.freertos.org/a00117.html

The Queue Receive Task:
The queue receive task is implemented by the prvQueueReceiveTask() function.
The task uses the FreeRTOS xQueueReceive() API function to receive values from
a queue.  The values received are those sent by the queue send task.  The queue
receive task increments the ulCountOfItemsReceivedOnQueue variable each time it
receives the value 100.  Therefore, as values are sent to the queue every 200ms,
the value of ulCountOfItemsReceivedOnQueue will increase by 5 every second.
http://www.freertos.org/a00118.html

An example software timer:
A software timer is created with an auto reloading period of 1000ms.  The
timer's callback function increments the ulCountOfTimerCallbackExecutions
variable each time it is called.  Therefore the value of
ulCountOfTimerCallbackExecutions will count seconds.
http://www.freertos.org/RTOS-software-timer.html

The FreeRTOS RTOS tick hook (or callback) function:
The tick hook function executes in the context of the FreeRTOS tick interrupt.
The function 'gives' a semaphore every 500th time it executes.  The semaphore
is used to synchronise with the event semaphore task, which is described next.

The event semaphore task:
The event semaphore task uses the FreeRTOS xSemaphoreTake() API function to
wait for the semaphore that is given by the RTOS tick hook function.  The task
increments the ulCountOfReceivedSemaphores variable each time the semaphore is
received.  As the semaphore is given every 500ms (assuming a tick frequency of
1KHz), the value of ulCountOfReceivedSemaphores will increase by 2 each second.

The idle hook (or callback) function:
The idle hook function queries the amount of free FreeRTOS heap space available.
See vApplicationIdleHook().

The malloc failed and stack overflow hook (or callback) functions:
These two hook functions are provided as examples, but do not contain any
functionality.
*/

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4_discovery.h"
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"



/*-----------------------------------------------------------*/
#define mainQUEUE_LENGTH 	100
#define ARRAY_SIZE 			19

#define ADC_Port			GPIOC

#define Traffic_Red_Pin  	GPIO_Pin_0
#define Traffic_Yellow_Pin  GPIO_Pin_1
#define Traffic_Green_Pin  	GPIO_Pin_2

#define SHIFT1_REG_Port		GPIOC
#define SHIFT_DATA_PIN		GPIO_Pin_6
#define SHIFT_CLK_PIN		GPIO_Pin_7
#define SHIFT_RESET_PIN		GPIO_Pin_8

//#define SHIFT2_REG_Port		GPIOC
//#define SHIFT2_REG_PIN		GPIO_PIN_6
//#define SHIFT2_CLK_PIN		GPIO_PIN_7
//
//#define SHIFT3_REG_Port		GPIOC
//#define SHIFT3_REG_PIN		GPIO_PIN_6
//#define SHIFT3_CLK_PIN		GPIO_PIN_7
//
//#define SHIFT1_PIN_2	GPIO_PIN_6
//#define SHIFT1_PIN_3	GPIO_PIN_6
//#define SHIFT1_PIN_4	GPIO_PIN_6
//#define SHIFT1_PIN_5	GPIO_PIN_6
//#define SHIFT1_PIN_6	GPIO_PIN_6
//#define SHIFT1_PIN_7	GPIO_PIN_6
//#define SHIFT1_PIN_8	GPIO_PIN_6

#define Prio_Task_Traffic_Flow		( tskIDLE_PRIORITY + 1 )
#define Prio_Task_Traffic_Create	( tskIDLE_PRIORITY + 2 )
#define Prio_Task_Traffic_Light		( tskIDLE_PRIORITY + 2 )
#define Prio_Task_Traffic_Display	( tskIDLE_PRIORITY )


#define LED_CLK_C RCC_AHB1Periph_GPIOC
// Can't use global variables

// Initialize global variables
// Global light color 0: red, 1: green
uint16_t global_flowrate;
uint16_t global_light_color = 1;
uint16_t global_car_value;




//TimerHandle_t			xTimer[3];
//TimerHandle_t			xTimer[3];
TimerHandle_t			xLightTimer;
//TimerHandle_t			Red_Light_TIMER;
//TimerHandle_t			Yellow_Light_TIMER;
//TimerHandle_t			Green_Light_TIMER;


/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
void prvSetupHardware( void );
void ShiftRegisterValuePreLight( uint16_t value );
static void Traffic_Flow_Task( void *pvParameters);
static void Traffic_Generator_Task( void *pvParameters);
static void System_Display_Task( void *pvParameters);
void LIGHT_TIMER_Callback( xTimerHandle xTimer);
static void Manager_Task( void *pvParameters );
void xxx(void);

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */

xQueueHandle xQueue_handle = 0;


/*-----------------------------------------------------------*/

struct TRAFFIC_Struct {
	uint16_t flow;
	int	carArray[ARRAY_SIZE];
	uint16_t car;
	uint16_t light_state;
};

int main(void)
{

	struct TRAFFIC_Struct TRAFFIC_Init;

	/* Configure the system ready to run the demo.  The clock configuration
	can be done here if it was not done before main() was called. */
	prvSetupHardware();

	xQueue_handle = xQueueCreate( 1, sizeof( TRAFFIC_Init ));

	xTaskCreate( Manager_Task, "Manager", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
//	xTaskCreate( Traffic_Flow_Task, "Flow", configMINIMAL_STACK_SIZE, NULL, Prio_Task_Traffic_Flow, NULL);
	xTaskCreate( Traffic_Generator_Task, "Generator", configMINIMAL_STACK_SIZE, NULL, Prio_Task_Traffic_Create, NULL);
//	xTaskCreate( Traffic_Light_State_Task, "Light-State", configMINIMAL_STACK_SIZE, NULL, Prio_Task_Traffic_Light, NULL);
	xTaskCreate( System_Display_Task, "Display", configMINIMAL_STACK_SIZE, NULL, Prio_Task_Traffic_Display, NULL);

//	Create the timer object
	xLightTimer = xTimerCreate( "Traffic_Timer", 1000 / portTICK_PERIOD_MS, pdFALSE, (void *) 0, LIGHT_TIMER_Callback);

//	Start the timer
	xTimerStart( xLightTimer, 0);

	vTaskStartScheduler();


	return 0;
}

static void Manager_Task( void *pvParameters ) {

	int new[ARRAY_SIZE] = {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0};
	struct TRAFFIC_Struct Traffic_state;
	Traffic_state.car = 0;
	//Traffic_state.carArray = new;
	memcpy(Traffic_state.carArray, new, ARRAY_SIZE*sizeof(int));

	for(int i = 0; i < 19; i++) {
		printf("MANAGER: Array: %i, Value: %i \n", i, Traffic_state.carArray[i]);
	}

	//Traffic_state.carArray = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	Traffic_state.light_state = 1;
	if(xQueueSend(xQueue_handle, &Traffic_state, 1000)) {
		printf("MANAGER: Sending data to queue \n");
	}
	else {
		printf("MANAGER failed! \n");
	}

	while(1) {
			vTaskDelay(1000);
	}
}

// Traffic Flow Task
/*-----------------------------------------------------------*/
//void Traffic_Flow_Task(void *pvParameters){
//
//	uint16_t adc_val = 0;
//	uint16_t speed_val = 0;
//	uint16_t current_speed_val = 0;
//	uint16_t change_in_speed;
//	struct TRAFFIC_Struct Traffic;
//
//	while(1) {
//		if(xQueueReceive(xQueue_handle, &Traffic, 500)){
//
//			ADC_SoftwareStartConv(ADC1);
//
//			while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
//
//			adc_val = ADC_GetConversionValue(ADC1);
//			printf("TRAFFIC_FLOW_TASK: ADC value: %u \n", adc_val);
//
//
//
//	//		Retrieve flow value
//	//		xQueueReceive( xQueue_handle, &(TRAFFIC_r), 500);
//	//
//	//		flowrate = TRAFFIC_r->flow;
//	//		flowrate = 4;
//	//		printf("GENERATOR_Task: Retrieved flow rate: %u. \n", flowrate);
//
//
//	// 		Generate random number based on potentiometer
////			car_value = (rand() % 10 + 1);
//			printf("GENERATOR_Task: Updated car value: %u. \n", Traffic.car);
//
//	//		Set car value to queue and send it back to the queue
//			Traffic.flow = 4;
//			if( xQueueSend( xQueue_handle, (void *) &Traffic, 1000)) {
//				vTaskDelay(1000);
//			} else {
//				printf("GENERATOR_Task failed");
//			}
//
//		}
//	}
//
//}

/*-----------------------------------------------------------*/


// Traffic Generator Task
/*-----------------------------------------------------------*/
void Traffic_Generator_Task(void *pvParameters){
//	Generate Traffic
	printf("Generate Traffic \n");
	struct TRAFFIC_Struct Traffic;

	uint16_t flowrate;
	uint16_t car_value;

	while(1){
		if(xQueueReceive(xQueue_handle, &Traffic, 500)){

	//		Retrieve flow value
	//		xQueueReceive( xQueue_handle, &(TRAFFIC_r), 500);
	//
	//		flowrate = TRAFFIC_r->flow;
	//		flowrate = 4;
	//		printf("GENERATOR_Task: Retrieved flow rate: %u. \n", flowrate);


	// 		Generate random number based on potentiometer
//			car_value = (rand() % 10 + 1);
			printf("GENERATOR_Task: Updated car value: %u. \n", Traffic.car);

	//		Set car value to queue and send it back to the queue
			Traffic.car = 1;
			if( xQueueSend( xQueue_handle, (void *) &Traffic, 1000)) {
				vTaskDelay(1000);
			} else {
				printf("GENERATOR_Task failed");
			}

		}
	}
}


void LIGHT_TIMER_Callback(xTimerHandle xTimer){
	struct TRAFFIC_Struct Traffic;

	printf("TRAFFIC_LIGHT_Callback: Start traffic callback \n");
	if (xQueueReceive(xQueue_handle, &Traffic, 500)){
		if( global_light_color == 0) {
			GPIO_ResetBits(GPIOC, Traffic_Red_Pin);
			GPIO_SetBits(GPIOC, Traffic_Green_Pin);
			xTimerChangePeriod(xLightTimer, 3000, 100);
			xTimerStart(xLightTimer, 0);
			global_light_color = 1;
		}

		//				Green light -> Yellow light
		else if (global_light_color == 1) {
			GPIO_ResetBits(GPIOC, Traffic_Green_Pin);
			GPIO_SetBits(GPIOC, Traffic_Yellow_Pin);
			xTimerChangePeriod(xLightTimer, 1000, 100);
			xTimerStart(xLightTimer, 0);
			global_light_color = 2;
		}

		//				Yellow light -> Red light
		else if (global_light_color == 2) {
		GPIO_ResetBits(GPIOC, Traffic_Yellow_Pin);
		GPIO_SetBits(GPIOC, Traffic_Red_Pin);
		xTimerChangePeriod(xLightTimer, 1500, 100);
		xTimerStart(xLightTimer, 0);
		global_light_color = 0;

		}

		if( xQueueSend( xQueue_handle, (void *) &Traffic, 1000)) {
			vTaskDelay(1000);
		} else {
			printf("LIGHT_TIMER failed");
		}
	}




}

void System_Display_Task( void *pvParameters){

	struct TRAFFIC_Struct Traffic_state;

	while(1) {

		if(xQueueReceive(xQueue_handle, &Traffic_state, 500)) {

			printf("DISPLAY_TASK: Grabbing value from queue \n");
			Shift_Traffic(&Traffic_state);

			Display_Board(&Traffic_state);

			if(xQueueSend(xQueue_handle, &Traffic_state, 1000)) {
				vTaskDelay(500);
			} else {
				printf("Display fail \n");
			}
		}
	}
}

void Shift_Traffic( struct TRAFFIC_Struct *traffic) {
//	shift all lights
	for(int i = 18; i > 0; i--) {

//		Red light and check if light 8 is on
		if (i == 8 && global_light_color != 1) {
			// handle not green case
			traffic->carArray[i] = 0;
			continue;
		}

		// if we're behind traffic light
		if( i <= 7) {

			// if light is green just shift normally
			if(global_light_color == 1){
				traffic->carArray[i] = traffic->carArray[i - 1];
			}

			// NOT GREEN
			else {
				// position 7 stops
//				Shift if there is an empty spot
				if (traffic->carArray[i] == 0){
					traffic->carArray[i] = traffic->carArray[i-1];
				}

//				Light 0 stops blinking
				else if (traffic->carArray[0] == 1){
					traffic->carArray[0] = traffic->car;
					traffic->car = 1;
				}

			}
		} else {

			traffic->carArray[i] = traffic->carArray[i - 1];
		}


		printf("SHIFT: Array: %i, Value: %i \n", i, traffic->carArray[i]);
	}

	traffic->carArray[0] = traffic->car;
	traffic->car = 0;

}

void Display_Board( struct TRAFFIC_Struct *traffic){

	GPIO_ResetBits(GPIOC, SHIFT_RESET_PIN);
	for (int i = 0; i < 10; i++);
	GPIO_SetBits(GPIOC, SHIFT_RESET_PIN);

	for (int val = ARRAY_SIZE - 1; val >= 0; val--) {
//		Write low bits
		if (traffic->carArray[val] == 0) {
			GPIO_ResetBits(GPIOC, GPIO_Pin_6);
			GPIO_ResetBits(GPIOC, SHIFT_CLK_PIN);
			for (int i = 0; i < 5; i++);
			GPIO_SetBits(GPIOC, SHIFT_CLK_PIN);
		}
//		Write high bits
		if (traffic->carArray[val] == 1) {
			GPIO_SetBits(GPIOC, GPIO_Pin_6);
			GPIO_ResetBits(GPIOC, SHIFT_CLK_PIN);
			for (int i = 0; i < 5; i++);
			GPIO_SetBits(GPIOC, SHIFT_CLK_PIN);
			GPIO_ResetBits(GPIOC, SHIFT_DATA_PIN);
		}

	}

}

/*-----------------------------------------------------------*/


/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software 
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}
/*-----------------------------------------------------------*/

void prvSetupHardware(void)
{
	/* Ensure all priority bits are assigned as preemption priority bits.
	http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	NVIC_SetPriorityGrouping( 0 );

	GPIO_InitTypeDef  	GPIO_Init_Shift_1;
	GPIO_InitTypeDef	GPIO_Init_Traffic;

	/* Enable the GPIO_LED Clock */
	RCC_AHB1PeriphClockCmd(LED_CLK_C, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

//	Initialize Shift Registers GPIO
	GPIO_Init_Shift_1.GPIO_Pin = GPIO_Pin_6 | SHIFT_CLK_PIN | GPIO_Pin_8 ;
	GPIO_Init_Shift_1.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Init_Shift_1.GPIO_OType = GPIO_OType_PP;
	GPIO_Init_Shift_1.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(SHIFT1_REG_Port, &GPIO_Init_Shift_1);

//	Initialize Traffic lights GPIO
	GPIO_Init_Traffic.GPIO_Pin = Traffic_Green_Pin | Traffic_Red_Pin | Traffic_Yellow_Pin;
	GPIO_Init_Traffic.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Init_Traffic.GPIO_OType = GPIO_OType_PP;
	GPIO_Init_Traffic.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(SHIFT1_REG_Port, &GPIO_Init_Traffic);


//	Initialize ADC
//	Enable Clock

	RCC_AHB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

//	Initialize GPIO
	GPIO_InitTypeDef	GPIO_InitStruct;

	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

//	Initialize ADC1
	ADC_InitTypeDef ADC_InitStruct;

	ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStruct.ADC_ExternalTrigConv = DISABLE;
	ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStruct.ADC_NbrOfConversion = 1;
	ADC_InitStruct.ADC_ScanConvMode = DISABLE;
	ADC_Init(ADC1, &ADC_InitStruct);
	ADC_Cmd(ADC1, ENABLE);

//	Select Input Channel
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 1, ADC_SampleTime_84Cycles);


	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}
