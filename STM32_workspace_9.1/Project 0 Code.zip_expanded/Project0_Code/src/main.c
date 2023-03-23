
/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include "stm32f4_discovery.h"
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"



/*-----------------------------------------------------------*/
#define mainQUEUE_LENGTH 100

#define amber  	0
#define green  	1
#define red  	2
#define blue  	3

#define amber_led	LED3
#define green_led	LED4
#define red_led		LED5
#define blue_led	LED6

#define Task1_Period 500
#define Task2_Period 500
#define Task3_Period 500


/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
static void prvSetupHardware( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */

static void Task1_Callback(xTimerHandle pxTimer);
static void Task2_Callback(xTimerHandle pxTimer);
static void Task3_Callback(xTimerHandle pxTimer);

static void DD_Task_Generator( void *pvParameters);

typedef enum {PERIODIC, APERIODIC} task_type;

typedef struct dd_task {
	TaskHandle_t t_handle;
	task_type type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t execution_time;
	uint32_t completion_time;
	uint32_t message;

};

typedef struct dd_task_list {
	struct dd_task task;
	struct dd_task_list *next_task;
};


void delete_dd_task(uint32_t task_id);

xQueueHandle xQueue_Generator = 0;
xQueueHandle xQueue_DDS = 0;
xQueueHandle xQueue_Monitor = 0;

xQueueHandle xQueue_create_task = 0;
xQueueHandle xQueue_create_task_message = 0;

xTimerHandle Task1_Release_Timer = 0;
xTimerHandle Task2_Release_Timer = 0;
xTimerHandle Task3_Release_Timer = 0;

/*-----------------------------------------------------------*/

int main(void)
{

	/* Initialize LEDs */
	STM_EVAL_LEDInit(amber_led);
	STM_EVAL_LEDInit(green_led);
	STM_EVAL_LEDInit(red_led);
	STM_EVAL_LEDInit(blue_led);

	/* Configure the system ready to run the demo.  The clock configuration
	can be done here if it was not done before main() was called. */
	prvSetupHardware();


	/* Create the queue used by the queue send and queue receive tasks.
	http://www.freertos.org/a00116.html */
	xQueue_create_task = xQueueCreate( 1, sizeof(struct dd_task));
	xQueue_create_task_message = xQueueCreate( 1, sizeof( uint32_t));

	/* Add to the registry, for the benefit of kernel aware debugging. */

	vQueueAddToRegistry( xQueue_create_task, "Create Task");
	vQueueAddToRegistry( xQueue_create_task_message, "Create Task Message")

	xTaskCreate( DD_Task_Generator, "Generate", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/

void xTask_DDS(void *pvParameters){
	printf("DDS started\n");
	//Heads of the lists
	struct dd_task_list *active_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	active_list_head->next_task = NULL;

	struct dd_task_list *complete_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	complete_list_head->next_task = NULL;

	struct dd_task_list *overdue_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	overdue_list_head->next_task = NULL;


	TaskHandle_t currently_active = NULL;
	struct dd_task next_task;
	TaskHandle_t finished_task;
	uint16_t message;
	while(1){

		if(release_task_message != 0 && release_queue != 0 && xQueueReceive(release_queue, &next_task, 30)){
			next_task.release_time = (uint32_t)xTaskGetTickCount();
			listPriorityAdd(active_list_head, next_task);
			uint16_t reply = 1;
			if(!xQueueSend(release_task_message, &reply,30)) {
                printf("Failed to send success message to release\n");
            }
			if (currently_active != NULL && currently_active != active_list_head->next_task->task.t_handle){
				vTaskPrioritySet(currently_active, 1);
			}
			currently_active = active_list_head->next_task->task.t_handle;
			vTaskPrioritySet(currently_active, 2);
		}

		//For Finished tasks
		if(complete_task_message != 0 && complete_queue != 0 && xQueueReceive(complete_queue, &finished_task, 30)){

			struct dd_task_list *curr = active_list_head;
			uint16_t found = 1;
			while(curr->task.t_handle != finished_task){
				if (curr->next_task == NULL){
					found = 0;
					printf("Task to complete not found\n");
				} else {
					curr = curr->next_task;
				}

			}
			if (found){
				//printf("Completing Task %d\n", (int)curr->task.task_id);
				curr->task.completion_time = (uint32_t) xTaskGetTickCount();
				if (curr->task.completion_time > curr->task.absolute_deadline){
					listAdd(overdue_list_head, curr->task);
				} else {
					listAdd(complete_list_head, curr->task);
				}
				listRemove(active_list_head, curr->task.task_id);
				free(curr);
			}
			uint32_t reply = 1;
			if(!xQueueOverwrite(complete_task_message, &reply)){
                printf("Failed to send success message to complete\n");
            }
			//vTaskDelay(pdMS_TO_TICKS(1));
			if(active_list_head->next_task != NULL){
				if (currently_active != NULL && currently_active != active_list_head->next_task->task.t_handle){
					vTaskPrioritySet(currently_active, 1);
				}
				currently_active = active_list_head->next_task->task.t_handle;
				vTaskPrioritySet(currently_active, 2);
			}
		}

		//Active task list request
		message = 0;
		if(active_list_message != 0 && xQueueReceive(active_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(active_task_list, &active_list_head)){
                    printf("Failed to send active task list\n");
                }
				if(!xQueueOverwrite(active_list_message, &reply)){
                    printf("Failed to send active_list_message\n");
                }
			}
		}

		//Complete task list request
		message = 0;
		if (complete_list_message != 0 && xQueueReceive(complete_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(complete_task_list, &complete_list_head)){
                    printf("Failed to send active task list\n");
                }
				if(!xQueueOverwrite(complete_list_message, &reply)){
                    printf("Failed to send complete message\n");
                }
			}
		}

		//Overdue task list request
		message = 0;
		if (overdue_list_message != 0 && xQueueReceive(overdue_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(overdue_task_list, &overdue_list_head)){
                    printf("Failed to send overdue task list\n");
                }
				if(!xQueueOverwrite(overdue_list_message, &reply)){
                    printf("Failed to send overdue message\n");
                }
			}
		}

		vTaskDelay(pdMS_TO_TICKS(1));

	}

}

void DD_Task_Generator( void *pvParameters ){
	uint32_t i = 0;
	uint32_t test_bench = 1;
	uint32_t test_period;

	while(1){
		if (test_bench == 1){
			test_period = 1500;
			create_dd_task(PERIODIC, 1, 95, 500 + i * test_period);
			create_dd_task(PERIODIC, 2, 150, 500 + i * test_period);
			create_dd_task(PERIODIC, 3, 250, 750 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(500));

			create_dd_task(PERIODIC, 1, 95, 1000 + i * test_period);
			create_dd_task(PERIODIC, 2, 150, 1000 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			create_dd_task(PERIODIC, 3, 250, 1500 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			create_dd_task(PERIODIC, 1, 95, 1500 + i * test_period);
			create_dd_task(PERIODIC, 2, 150, 1500 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(500));

		}
	}
}

static void Task1_Callback(xTimerHandle pxTimer){

}

static void Task2_Callback(xTimerHandle pxTimer){

}

static void Task3_Callback(xTimerHandle pxTimer){

}

void create_dd_task(uint32_t type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline) {

	struct dd_task new_dd_task;
	printf("CREATE_DD_TASK: Creating task %u\n", task_id);

	new_dd_task.type = type;
	new_dd_task.task_id = task_id;
	new_dd_task.execution_time = execution_time;
	new_dd_task.absolute_deadline = absolute_deadline;
	new_dd_task.message = 0;


	if( xQueueSend( xQueue_DDS, (void *) &new_dd_task, 1000)) {
		vTaskDelay(0);
	} else {
		printf("CREATE_DD_TASK: Failed!\n");
	}


}

void complete_dd_task(TaskHandle_t t_handle) {

    xQueue_complete_task = xQueueCreate( 1, sizeof( Taskhandle_t));
    xQueue_complete_task_message = xQueueCreate( 1, sizeof(uint32_t));

    vQueueAddToRegistry(xQueue_complete_task, "Delete");
    vQueueAddToRegistry(xQueue_complete_task_message, "Delete Message")

    if(!xQueueOverwrite(xQueue_complete_task, &t_handle)){
        printf('Complete task failedn');
    }

    int reply;

    while(1){
        if(xQueueReceive(xQueue_complete_task_message, &reply, 30)) {
            break;
        }
    }
    vTaskDelete(t_handle);
    vQueueDelete(xQueue_complete_task);
    vQueueDelete(xQueue_complete_task_message);
    xQueue_complete_task = 0;
    xQueue_complete_task_message = 0;
}

void get_active_dd_task_list(void) {
    xQueue_active_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
    xQueue_active_list_message = xQueueCrate(1, sizeof(uint32_t));

    vQueueAddToRegistry(xQueue_active_task_list, "Active list");
    vQueueAddToRegistry(xQueue_active_list_message, "Active message");

    int message = 1;
    int reply;

    if(!xQueueOverwrite(xQueue_active_list_message, &message)){
        printf("Failed to send active task message\n");
    }

    while(1){
        if(xQueuePeek(xQueue_active_list_message, &reply, 30)) {

        }
    }

}

void 


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

static void prvSetupHardware( void )
{
	/* Ensure all priority bits are assigned as preemption priority bits.
	http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	NVIC_SetPriorityGrouping( 0 );

	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}
