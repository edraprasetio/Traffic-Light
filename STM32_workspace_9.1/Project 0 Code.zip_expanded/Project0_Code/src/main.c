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
void DD_Task_Scheduler(void *pvParameters);
void DD_Task_User(void *pvParameters);
void DD_Task_Monitor(void *pvParameters);

/* DD tasks and functions */

void create_dd_task(uint32_t task_type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline);
void end_dd_task(TaskHandle_t t_handle); //complete or end func
void get_curr_dd_task_list(void);
void get_fin_dd_task_list(void);
void get_missed_dd_task_list(void);

//void delete_dd_task(uint32_t task_id);

/*Helper functions*/
void listAdd(struct dd_task_list *head, struct dd_task next_task);
void listPriorityAdd(struct dd_task_list *head, struct dd_task next_task);
void listRemove(struct dd_task_list *head, uint32_t task_id);

xQueueHandle xQueue_Generator = 0;
xQueueHandle xQueue_DDS = 0;
xQueueHandle xQueue_Monitor = 0;

xQueueHandle xQueue_create_task = 0;
xQueueHandle xQueue_create_task_message = 0;

xTimerHandle Task1_Release_Timer = 0;
xTimerHandle Task2_Release_Timer = 0;
xTimerHandle Task3_Release_Timer = 0;



xQueueHandle release_queue = 0;
xQueueHandle release_task_message = 0;
xQueueHandle end_queue = 0;
xQueueHandle end_task_message = 0;
xQueueHandle curr_task_list = 0;
xQueueHandle curr_list_message = 0;
xQueueHandle fin_task_list = 0;
xQueueHandle fin_list_message = 0;
xQueueHandle missed_task_list = 0;
xQueueHandle missed_list_message = 0;

typedef enum {PERIODIC, APERIODIC} task_type; //only periodic rly

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
	xQueue_Generator = xQueueCreate( 100, sizeof( uint32_t));
	xQueue_DDS = xQueueCreate( 100, sizeof( uint32_t));
	xQueue_Monitor = xQueueCreate( 100, sizeof( uint32_t));

	xQueue_create_task = xQueueCreate( 1, sizeof(struct dd_task));
	xQueue_create_task_message = xQueueCreate( 1, sizeof( uint32_t));

	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry( xQueue_Generator, "Generator Queue");
	vQueueAddToRegistry( xQueue_DDS, "DDS Queue");
	vQueueAddToRegistry( xQueue_Monitor, "Monitor Queue");

	vQueueAddToRegistry( xQueue_create_task, "Create Task");
	vQueueAddToRegistry( xQueue_create_task_message, "Create Task Message")

	xTaskCreate( DD_Task_Generator, "Generate", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

	Task1_Release_Timer = xTimerCreate( "Task 1 timer", pdMS_TO_TICKS(Task1_Period), pdTRUE, (void *) 0, Task1_Callback);
	Task2_Release_Timer = xTimerCreate( "Task 2 timer", pdMS_TO_TICKS(Task2_Period), pdTRUE, (void *) 0, Task2_Callback);
	Task3_Release_Timer = xTimerCreate( "Task 3 timer", pdMS_TO_TICKS(Task3_Period), pdTRUE, (void *) 0, Task3_Callback);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/



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


void DD_Task_Scheduler(void *pvParameters){
	printf("Scheduler started\n");
	//Heads of the lists
	struct dd_task_list *curr_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	curr_list_head->next_task = NULL;

	struct dd_task_list *fin_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	fin_list_head->next_task = NULL;

	struct dd_task_list *missed_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	missed_list_head->next_task = NULL;


	TaskHandle_t currently_active = NULL;
	struct dd_task next_task;
	TaskHandle_t finished_task;
	uint16_t message;
	while(1){
		//For New Tasks

		if(release_task_message != 0 && release_queue != 0 && xQueueReceive(release_queue, &next_task, 30)){
			//printf("Releasing Task %d\n", (int)next_task.task_id);
			next_task.release_time = (uint32_t)xTaskGetTickCount();
			listPriorityAdd(curr_list_head, next_task);
			uint16_t reply = 1;
			if(!xQueueSend(release_task_message, &reply,30))printf("Failed to send success message to release\n");
			//vTaskDelay(1);
			// Set priorities
			if (currently_active != NULL && currently_active != curr_list_head->next_task->task.t_handle){
				vTaskPrioritySet(currently_active, 1);
			}
			currently_active = curr_list_head->next_task->task.t_handle;
			vTaskPrioritySet(currently_active, 2);
		}

		//For Finished tasks
		if(end_task_message != 0 && end_queue != 0 && xQueueReceive(end_queue, &finished_task, 30)){

			struct dd_task_list *curr = curr_list_head;
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
					listAdd(missed_list_head, curr->task);
				} else {
					listAdd(fin_list_head, curr->task);
				}
				listRemove(curr_list_head, curr->task.task_id);
				free(curr);
			}
			uint32_t reply = 1;
			if(!xQueueOverwrite(end_task_message, &reply))printf("Failed to send success message to complete\n");
			//vTaskDelay(pdMS_TO_TICKS(1));
			if(curr_list_head->next_task != NULL){
				if (currently_active != NULL && currently_active != curr_list_head->next_task->task.t_handle){
					vTaskPrioritySet(currently_active, 1);
				}
				currently_active = curr_list_head->next_task->task.t_handle;
				vTaskPrioritySet(currently_active, 2);
			}
		}

		//Curr task list request
		message = 0;
		if(curr_list_message != 0 && xQueueReceive(curr_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(curr_task_list, &curr_list_head))printf("Failed to send active task list\n");
				if(!xQueueOverwrite(curr_list_message, &reply))printf("Failed to send active_list_message\n");
			}
		}

		//finished task list request
		message = 0;
		if (fin_list_message != 0 && xQueueReceive(fin_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(fin_task_list, &fin_list_head))printf("Failed to send active task list\n");
				if(!xQueueOverwrite(fin_list_message, &reply))printf("Failed to send complete message\n");
			}
		}

		//missed task list request
		message = 0;
		if (missed_list_message != 0 && xQueueReceive(missed_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(missed_task_list, &missed_list_head))printf("Failed to send overdue task list\n");
				if(!xQueueOverwrite(missed_list_message, &reply))printf("Failed to send overdue message\n");
			}
		}

		vTaskDelay(pdMS_TO_TICKS(1));

	}

}


void DD_Task_Monitor(void *pvParameters){
	struct dd_task_list *curr_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
 	struct dd_task_list *fin_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
 	struct dd_task_list *missed_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	while(1){
		vTaskDelay(pdMS_TO_TICKS(1600));
		printf("\n---------- Current Tasks ----------\n\n");
		get_curr_dd_task_list();
		if(xQueueReceive(curr_task_list, &curr_tasks, 100)){
			//If list not empty
			if (curr_tasks->next_task != NULL ){
				curr_tasks = curr_tasks->next_task;
				while(curr_tasks->next_task != NULL){
					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d\n", (int)curr_tasks->task.task_type,(int)curr_tasks->task.task_id, (int)curr_tasks->task.absolute_deadline, (int)curr_tasks->task.release_time);
					curr_tasks = curr_tasks->next_task;
				}
				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d\n", (int)curr_tasks->task.task_type,(int)curr_tasks->task.task_id, (int)curr_tasks->task.absolute_deadline, (int)curr_tasks->task.release_time);
			}
			free(curr_tasks);
			vQueueDelete(curr_task_list);
			curr_task_list = 0;
		}
		printf("\n---------- Finished Tasks ----------\n\n");
		get_fin_dd_task_list();
		if(xQueueReceive(fin_task_list, &fin_tasks, 100)){
			//If list not empty
			if (fin_tasks->next_task != NULL ){
				fin_tasks= fin_tasks->next_task;
				while(fin_tasks->next_task != NULL){
					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)fin_tasks->task.task_type,(int)fin_tasks->task.task_id, (int)fin_tasks->task.absolute_deadline, (int)fin_tasks->task.release_time, (int)fin_tasks->task.completion_time);
					fin_tasks = fin_tasks->next_task;
				}
				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)fin_tasks->task.task_type,(int)fin_tasks->task.task_id, (int)fin_tasks->task.absolute_deadline, (int)fin_tasks->task.release_time, (int)fin_tasks->task.completion_time);
			}
			free(fin_tasks);
			vQueueDelete(fin_task_list);
			fin_task_list = 0;
		}

		printf("\n---------- Missed Tasks ----------\n\n");
		get_missed_dd_task_list();
		if(xQueueReceive(missed_task_list, &missed_tasks, 100)){
			//If list not empty
			if (missed_tasks->next_task != NULL ){
				missed_tasks= missed_tasks->next_task;
				while(missed_tasks->next_task != NULL){
					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)missed_tasks->task.task_type,(int)missed_tasks->task.task_id, (int)missed_tasks->task.absolute_deadline, (int)missed_tasks->task.release_time, (int)missed_tasks->task.completion_time);
					missed_tasks = missed_tasks->next_task;
				}
				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)missed_tasks->task.task_type,(int)missed_tasks->task.task_id, (int)missed_tasks->task.absolute_deadline, (int)missed_tasks->task.release_time, (int)missed_tasks->task.completion_time);
			}
			free(missed_tasks);

			vQueueDelete(missed_task_list);
			missed_task_list = 0;
		}
	}
}

void create_dd_task(uint32_t type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline) {

	struct dd_task new_dd_task;
	printf("CREATE_DD_TASK: Creating task %u\n", task_id);

	new_dd_task.type = type;
	new_dd_task.task_id = task_id;
	new_dd_task.execution_time = execution_time;
	new_dd_task.absolute_deadline = absolute_deadline;
	new_dd_task.message = 0;


	// if( xQueueSend( xQueue_DDS, (void *) &new_dd_task, 1000)) {
	// 	vTaskDelay(0);
	// } else {
	// 	printf("CREATE_DD_TASK: Failed!\n");
	// }
	xTaskCreate(DD_Task_User,"UDT",configMINIMAL_STACK_SIZE,(void *) new_dd_task.execution_time,1,&(new_dd_task.t_handle));

	// Create queue
	release_queue = xQueueCreate(1,sizeof(struct dd_task));
	vQueueAddToRegistry(release_queue,"Release");
	release_task_message = xQueueCreate(1,sizeof(int));
	vQueueAddToRegistry(release_task_message,"Release message");

	// Send message
	if(!xQueueSend(release_queue, &new_dd_task, 30))printf("Failed to release new task\n");

	// Delete queue
	int reply;
	while(1){
		if(xQueueReceive(release_task_message, &reply, 30)){
			break;
		}
	}
	vQueueDelete(release_task_message);
	vQueueDelete(release_queue);
	release_queue = 0;
	release_task_message = 0;

}


void end_dd_task(TaskHandle_t t_handle){
		// Create queue
		end_queue = xQueueCreate(1,sizeof(TaskHandle_t));
		vQueueAddToRegistry(end_queue,"Delete");
		end_task_message = xQueueCreate(1,sizeof(int));
		vQueueAddToRegistry(end_task_message,"Delete message");

		// Send message
		if(!xQueueOverwrite(end_queue, &t_handle))printf("Failed to send complete message\n");

		// Delete queue
		int reply;
		while(1){
			if(xQueueReceive(end_task_message, &reply, 30)){
				break;
			}
		}
		vTaskDelete(t_handle);
		vQueueDelete(end_task_message);
		vQueueDelete(end_queue);
		end_task_message = 0;
		end_queue = 0;
}

void get_curr_dd_task_list(void){
	// Create Queues
	curr_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
	vQueueAddToRegistry(curr_task_list,"Active");
	curr_list_message = xQueueCreate(1, sizeof(int));
	vQueueAddToRegistry(curr_list_message,"Active message");
	int message = 1;
	int reply;

	//Send Message
	if(!xQueueOverwrite(curr_list_message, &message))printf("Failed to send curr task message\n");

	//Wait For Reply
	while(1){
		if(xQueuePeek(curr_list_message, &reply, 30)){
			if (reply == 2) break;
		}
	}
	vQueueDelete(curr_list_message);
	curr_list_message = 0;
}

void get_fin_dd_task_list(void){
	// Create Queues
	fin_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
	vQueueAddToRegistry(fin_task_list,"Complete");
	fin_list_message = xQueueCreate(1, sizeof(int));
	vQueueAddToRegistry(end_task_message,"Complete message");

	int message = 1;
	int reply;
	//Send Message
	if(!xQueueOverwrite(fin_list_message, &message))printf("Failed to send complete task message\n");
	//Wait For Reply
	while(1){
		if(xQueuePeek(fin_list_message, &reply, 30)){
			if (reply == 2) break;
		}
	}
	vQueueDelete(fin_list_message);
	fin_list_message = 0;
}

void get_missed_dd_task_list(void){
	// Create Queues
	missed_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
	vQueueAddToRegistry(missed_task_list,"Overdue");
	missed_list_message = xQueueCreate(1, sizeof(int));
	vQueueAddToRegistry(missed_list_message,"Overdue message");

	int message = 1;
	int reply;
	//Send Message
	if(!xQueueOverwrite(missed_list_message, &message))printf("Failed to send missed task message\n");
	//Wait For Reply
	while(1){
		if(xQueuePeek(missed_list_message, &reply, 30)){
			if (reply == 2) break;
		}
	}
	vQueueDelete(missed_list_message);
	missed_list_message = 0;
}


void DD_Task_User(void *pvParameters){
	while (uxTaskPriorityGet(NULL) < 2);
	int start = (int)xTaskGetTickCount();
	int end = start + (int) pvParameters;
	while ((int)xTaskGetTickCount() < end);

	vTaskPrioritySet(NULL, 3);
	end_dd_task(xTaskGetCurrentTaskHandle());
	vTaskSuspend(NULL);
}





/*----------------------- Helper Functions -------------------------*/

void listAdd(struct dd_task_list *head, struct dd_task next_task){
	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	new_node->task = next_task;
	new_node->next_task = NULL;
	struct dd_task_list *curr = head;
	while (curr->next_task != NULL){
		curr = curr->next_task;
	}
	curr->next_task = new_node;
}

void listPriorityAdd(struct dd_task_list *head, struct dd_task next_task){
	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	new_node->task = next_task;
	struct dd_task_list *curr = head;
	while (curr->next_task != NULL){
		if(curr->next_task->task.absolute_deadline > new_node->task.absolute_deadline){
			new_node->next_task = curr->next_task;
			curr->next_task = new_node;
			return;
		}
		curr = curr->next_task;
	}
	new_node->next_task = curr->next_task;
	curr->next_task = new_node;
}

void listRemove(struct dd_task_list *head, uint32_t task_id){
	struct dd_task_list *curr = head;
	struct dd_task_list *prev = head;
	while (curr->task.task_id != task_id){
		if(curr->next_task == NULL){
			printf("ID not found\n");
			return;
		}
		prev = curr;
		curr = curr->next_task;
	}
	if (curr->next_task == NULL){
		prev->next_task = NULL;
	} else {
		prev->next_task = curr->next_task;
	}
	free(curr);
}


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
