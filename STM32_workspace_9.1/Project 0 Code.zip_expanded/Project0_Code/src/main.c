
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

struct dd_task {
	TaskHandle_t t_handle;
	uint32_t type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t execution_time;
	uint32_t completion_time;
	uint32_t message;
};

struct dd_task_list {
	struct dd_task task;
	struct dd_task_list* next_task;
};

struct dd_task dd_delete(struct dd_task_list* head, uint32_t task_id){
	struct dd_task_list *current = head;
	struct dd_task_list *previous = head;

	while(current->task.task_id != task_id) {
		previous = current;
		current = current->next_task;
	}
	if(current->next_task == NULL) {
		previous->next_task = NULL;
	}
	else {
		previous->next_task = current->next_task;
	}
	vTaskDelete(current->task.t_handle);
	return current->task;
}
/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
static void prvSetupHardware( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */

static void DD_Task_Generator( void *pvParameters);
static void Create_UD_Task(void *pvParameters);
static void Deadline_Driven_Scheduler(void *pvParameters);
static void DD_Task_Monitor( void *pvParameters);



typedef enum {PERIODIC, APERIODIC} task_type;
//typedef enum {PERIODIC} task_type;



void dd_start(struct dd_task_list*);
int get_active_dd_task_list(struct dd_task_list*);
void create_dd_task(uint32_t , uint32_t , uint32_t, uint32_t );


xQueueHandle xQueue_create_task = 0;
xQueueHandle xQueue_create_task_message = 0;
xQueueHandle xQueue_delete_task = 0;
xQueueHandle xQueue_delete_task_message = 0;

xQueueHandle active_task_queue = 0;
xQueueHandle complete_task_queue = 0;
xQueueHandle overdue_task_queue = 0;

xQueueHandle Complete_Queue = 0;
xQueueHandle Create_Queue = 0;



/*-----------------------------------------------------------*/

int main(void)
{
	//SystemInit();

	prvSetupHardware();


	Complete_Queue = xQueueCreate( 20, sizeof(uint32_t));
	Create_Queue = xQueueCreate( 20, sizeof(uint32_t));

	active_task_queue = xQueueCreate( 1, sizeof(struct dd_task_list));
	complete_task_queue = xQueueCreate( 1, sizeof(struct dd_task_list));
	overdue_task_queue = xQueueCreate(1, sizeof(struct dd_task_list));

	vQueueAddToRegistry( Complete_Queue, "Completed Task Queue");
	vQueueAddToRegistry( Create_Queue, "Created Task Queue");

	vQueueAddToRegistry( active_task_queue, "Active task list queue");
	vQueueAddToRegistry( complete_task_queue, "Complete task list queue");
	vQueueAddToRegistry( overdue_task_queue, "Overdue task list queue");

	xTaskCreate( DD_Task_Generator, "Generate", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	xTaskCreate (Deadline_Driven_Scheduler, "DDS", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
	xTaskCreate (DD_Task_Monitor, "Monitor", configMINIMAL_STACK_SIZE, NULL, 5, NULL);

	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/

void Deadline_Driven_Scheduler(void *pvParameters){
	printf("DDS started\n");
	struct dd_task_list *curr_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list *fin_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list *missed_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));

	curr_tasks->next_task = NULL;
	fin_tasks->next_task = NULL;
	missed_tasks->next_task = NULL;

	struct dd_task new_task;

	uint32_t completed_task_id;
	int CPUAvailable = 1;
	while(1){

		if(xQueueReceive(Create_Queue, &new_task, 0)){
			printf("Received task: %d at: %d\n", (int)new_task.task_id, (int)xTaskGetTickCount());
			addToList(curr_tasks, new_task, 1);
		}

		if(xQueueReceive(Complete_Queue, &completed_task_id, 0)) {
			CPUAvailable = 1;
			struct dd_task removed_task = dd_delete(curr_tasks, completed_task_id);
			printf("Completed task: %d at: %d\n", (int)removed_task.task_id, (int)removed_task.completion_time);

			if(removed_task.completion_time < removed_task.absolute_deadline) {
				addToList(fin_tasks, removed_task, 0);
			}
			else{
				printf("Overdue Task: %d at: %d\n", (int)removed_task.task_id, (int)xTaskGetTickCount());
				addToList(missed_tasks, removed_task, 0);
			}

		}

		dd_remove_overdue(curr_tasks, missed_tasks);

		if(curr_tasks->next_task != NULL && CPUAvailable && (xQueuePeek(Create_Queue, &new_task, 0) == pdFALSE)) {

			CPUAvailable = 0;
			dd_start(curr_tasks);
		}

		if(curr_tasks->next_task != NULL) {
			if(!xQueueOverwrite(active_task_queue, &curr_tasks)) {
				printf("Failed to send task to active queue\n");
			}
		}
		if(missed_tasks->next_task != NULL) {
			if(!xQueueOverwrite(overdue_task_queue, &missed_tasks)){
				printf("Failed to send task to missed queue\n");
			}
		}
		if(fin_tasks->next_task != NULL) {
			if(!xQueueOverwrite(complete_task_queue, &fin_tasks)) {
				printf("Failed to send task to complete queue\n");
			}
		}
		vTaskDelay(pdMS_TO_TICKS(1));

//		if(xQueue_create_task_message != 0 && xQueue_create_task != 0 && xQueueReceive(xQueue_create_task, &next_task, 30)){
//			next_task.release_time = (uint32_t)xTaskGetTickCount();
//			addPriorityToList(active_list_head, next_task);
//			uint16_t reply = 1;
//			if(!xQueueSend(xQueue_create_task_message, &reply,30)) {
//                printf("Failed to send success message to release\n");
//            }
//			if (currently_active != NULL && currently_active != active_list_head->next_task->task.t_handle){
//				vTaskPrioritySet(currently_active, 1);
//			}
//			currently_active = active_list_head->next_task->task.t_handle;
//			vTaskPrioritySet(currently_active, 2);
//		}
//
//		//For Finished tasks
//		if(xQueue_complete_list_message != 0 && xQueue_complete_task_list != 0 && xQueueReceive(xQueue_complete_task_list, &finished_task, 30)){
//
//			struct dd_task_list *curr = active_list_head;
//			uint16_t found = 1;
//			while(curr->task.t_handle != finished_task){
//				if (curr->next_task == NULL){
//					found = 0;
//					printf("Task to complete not found\n");
//				} else {
//					curr = curr->next_task;
//				}
//
//			}
//			if (found){
//				//printf("Completing Task %d\n", (int)curr->task.task_id);
//				curr->task.completion_time = (uint32_t) xTaskGetTickCount();
//				if (curr->task.completion_time > curr->task.absolute_deadline){
//					addToList(overdue_list_head, curr->task);
//				} else {
//					addToList(complete_list_head, curr->task);
//				}
//				removeFromList(active_list_head, curr->task.task_id);
//				free(curr);
//			}
//			uint32_t reply = 1;
//			if(!xQueueOverwrite(xQueue_complete_task_list, &reply)){
//                printf("Failed to send success message to complete\n");
//            }
//			//vTaskDelay(pdMS_TO_TICKS(1));
//			if(active_list_head->next_task != NULL){
//				if (currently_active != NULL && currently_active != active_list_head->next_task->task.t_handle){
//					vTaskPrioritySet(currently_active, 1);
//				}
//				currently_active = active_list_head->next_task->task.t_handle;
//				vTaskPrioritySet(currently_active, 2);
//			}
//		}
//
//		//Active task list request
//		message = 0;
//		if(xQueue_active_list_message != 0 && xQueueReceive(xQueue_active_list_message, &message, 30)){
//			if(message){
//				int reply = 2;
//				if(!xQueueOverwrite(xQueue_active_task_list, &active_list_head)){
//                    printf("Failed to send active task list\n");
//                }
//				if(!xQueueOverwrite(xQueue_active_list_message, &reply)){
//                    printf("Failed to send active_list_message\n");
//                }
//			}
//		}
//
//		//Complete task list request
//		message = 0;
//		if (xQueue_complete_list_message != 0 && xQueueReceive(xQueue_complete_list_message, &message, 30)){
//			if(message){
//				int reply = 2;
//				if(!xQueueOverwrite(xQueue_complete_task_list, &complete_list_head)){
//                    printf("Failed to send active task list\n");
//                }
//				if(!xQueueOverwrite(xQueue_complete_list_message, &reply)){
//                    printf("Failed to send complete message\n");
//                }
//			}
//		}
//
//		//Overdue task list request
//		message = 0;
//		if (xQueue_overdue_list_message != 0 && xQueueReceive(xQueue_overdue_list_message, &message, 30)){
//			if(message){
//				int reply = 2;
//				if(!xQueueOverwrite(xQueue_overdue_task_list, &overdue_list_head)){
//                    printf("Failed to send overdue task list\n");
//                }
//				if(!xQueueOverwrite(xQueue_overdue_list_message, &reply)){
//                    printf("Failed to send overdue message\n");
//                }
//			}
//		}
//
//		vTaskDelay(pdMS_TO_TICKS(1));
//
	}

}

void DD_Task_Monitor(void *pvParameters){
	printf("Monitor starts \n");
	struct dd_task_list *curr_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
 	struct dd_task_list *fin_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
 	struct dd_task_list *missed_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));

 	int count = 0;
	while(1){

		vTaskDelay(pdMS_TO_TICKS(1500));
		printf("%d---------- ----------\n", (int)xTaskGetTickCount());

		if(xQueueReceive(active_task_queue, &curr_tasks, 0)){
			count = get_active_dd_task_list(curr_tasks);
			printf("Active Tasks: %d\n", count);
		}
		else {
			printf("Active Tasks: 0\n");
		}

		if(xQueueReceive(complete_task_queue, &fin_tasks, 0)){
			count = get_complete_dd_task_list(fin_tasks);
			printf("Complete Tasks: %d\n", count);
		}
		else {
			printf("Complete Tasks: 0\n");
		}

		if(xQueueReceive(overdue_task_queue, &missed_tasks, 0)){
			count = get_overdue_dd_task_list(missed_tasks);
			printf("Overdue Tasks: %d\n", count);
		}
		else {
			printf("Overdue Tasks: 0\n");
		}

		printf("%d---------- ----------\n", (int)xTaskGetTickCount());
//			//If list not empty
//			if (curr_tasks->next_task != NULL ){
//				curr_tasks = curr_tasks->next_task;
//				while(curr_tasks->next_task != NULL){
//					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d\n", (int)curr_tasks->task.type,(int)curr_tasks->task.task_id, (int)curr_tasks->task.absolute_deadline, (int)curr_tasks->task.release_time);
//					curr_tasks = curr_tasks->next_task;
//				}
//				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d\n", (int)curr_tasks->task.type,(int)curr_tasks->task.task_id, (int)curr_tasks->task.absolute_deadline, (int)curr_tasks->task.release_time);
//			}
//			free(curr_tasks);
//			vQueueDelete(xQueue_active_task_list);
//			xQueue_active_task_list = 0;
//		}
//		printf("\n---------- Finished Tasks ----------\n\n");
//		get_complete_dd_task_list();
//		if(xQueueReceive(xQueue_complete_task_list, &fin_tasks, 100)){
//			//If list not empty
//			if (fin_tasks->next_task != NULL ){
//				fin_tasks= fin_tasks->next_task;
//				while(fin_tasks->next_task != NULL){
//					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)fin_tasks->task.type,(int)fin_tasks->task.task_id, (int)fin_tasks->task.absolute_deadline, (int)fin_tasks->task.release_time, (int)fin_tasks->task.completion_time);
//					fin_tasks = fin_tasks->next_task;
//				}
//				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)fin_tasks->task.type,(int)fin_tasks->task.task_id, (int)fin_tasks->task.absolute_deadline, (int)fin_tasks->task.release_time, (int)fin_tasks->task.completion_time);
//			}
//			free(fin_tasks);
//			vQueueDelete(xQueue_complete_task_list);
//			xQueue_complete_task_list = 0;
//		}
//
//		printf("\n---------- Missed Tasks ----------\n\n");
//		get_overdue_dd_task_list();
//		if(xQueueReceive(xQueue_overdue_task_list, &missed_tasks, 100)){
//			//If list not empty
//			if (missed_tasks->next_task != NULL ){
//				missed_tasks= missed_tasks->next_task;
//				while(missed_tasks->next_task != NULL){
//					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)missed_tasks->task.type,(int)missed_tasks->task.task_id, (int)missed_tasks->task.absolute_deadline, (int)missed_tasks->task.release_time, (int)missed_tasks->task.completion_time);
//					missed_tasks = missed_tasks->next_task;
//				}
//				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)missed_tasks->task.type,(int)missed_tasks->task.task_id, (int)missed_tasks->task.absolute_deadline, (int)missed_tasks->task.release_time, (int)missed_tasks->task.completion_time);
//			}
//			free(missed_tasks);
//
//			vQueueDelete(xQueue_overdue_task_list);
//			xQueue_overdue_task_list = 0;

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

		if (test_bench == 2){
			test_period = 1500;
			create_dd_task(PERIODIC, 1, 95, 250 + i * test_period);
			create_dd_task(PERIODIC, 2, 150, 500 + i * test_period);
			create_dd_task(PERIODIC, 3, 250, 750 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(500));

			create_dd_task(PERIODIC, 1, 95, 500 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			create_dd_task(PERIODIC, 1, 95, 750 + i * test_period);
			create_dd_task(PERIODIC, 2, 150, 1000 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			create_dd_task(PERIODIC, 1, 95, 1000 + i * test_period);
			create_dd_task(PERIODIC, 3, 250, 1500 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			create_dd_task(PERIODIC, 1, 95, 1250 + i * test_period);
			create_dd_task(PERIODIC, 2, 150, 1500 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			create_dd_task(PERIODIC, 1, 95, 1500 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(250));

		}
	}
}

void dd_start(struct dd_task_list* head){
	head->next_task->task.release_time = (uint32_t)xTaskGetTickCount();
	printf("Released task: %d at: %d\n", (int)head->next_task->task.task_id, (int)head->next_task->task.release_time);
	xTaskCreate(Create_UD_Task, "UD Task", configMINIMAL_STACK_SIZE, &head->next_task->task, 3, &(head->next_task->task.t_handle));
}

void Create_UD_Task(void *pvParameters){
	struct dd_task* curr_task = (struct dd_task*) pvParameters;
	int start = (int)xTaskGetTickCount();
	int end = start + (int) curr_task->execution_time;

	while ((int)xTaskGetTickCount() < end);
	uint32_t task_id = curr_task->task_id;
	curr_task->completion_time = (uint32_t) xTaskGetTickCount();
	delete_dd_task(task_id);
	vTaskSuspend(NULL);
}


void complete_dd_task(uint32_t task_id){
	if(!QueueSend(Complete_Queue, &task_id, 0)) {
		printf("Failed to send completed task to queue \n");
	}
}

void create_dd_task(uint32_t type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline) {

	struct dd_task new_dd_task;
	printf("CREATE_DD_TASK: Creating task %u \n", task_id);

	new_dd_task.type = type;
	new_dd_task.task_id = task_id;
	new_dd_task.execution_time = execution_time;
	new_dd_task.absolute_deadline = absolute_deadline;
	new_dd_task.release_time = -1;
	new_dd_task.completion_time = -1; //use l8er

	if( !xQueueSend( xQueue_create_task, &new_dd_task, 100)) {
		printf("Failed to create a task\n");
	}


//	xTaskCreate(Create_UD_Task, "UD Task", configMINIMAL_STACK_SIZE, (void *) new_dd_task.execution_time, 1, &(new_dd_task.t_handle));
//
//	xQueue_create_task = xQueueCreate( 1, sizeof(struct dd_task));
//	xQueue_create_task_message = xQueueCreate( 1, sizeof( uint32_t));
//
//	vQueueAddToRegistry( xQueue_create_task, "Create Task");
//	vQueueAddToRegistry( xQueue_create_task_message, "Create Task Message");
//
//
//	if( xQueueSend( xQueue_create_task, &new_dd_task, 30)) {
//		vTaskDelay(0);
//	} else {
//		printf("CREATE_DD_TASK: Failed!\n");
//	}
//
//	int reply_message;
//
//	while(1) {
//		if(xQueueReceive(xQueue_create_task_message, &reply_message, 30)){
//			break;
//		}
//	}
//	vQueueDelete(xQueue_create_task);
//	vQueueDelete(xQueue_create_task_message);
//	xQueue_create_task = 0;
//	xQueue_create_task_message = 0;
}

void delete_dd_task(TaskHandle_t t_handle) {
	xQueue_delete_task = xQueueCreate( 1, sizeof( TaskHandle_t));
	xQueue_delete_task_message = xQueueCreate( 1, sizeof(uint32_t));

	vQueueAddToRegistry(xQueue_delete_task, "Delete");
	vQueueAddToRegistry(xQueue_delete_task_message, "Delete Message");

	if(!xQueueOverwrite(xQueue_delete_task, &t_handle)){
		printf('Delete task failed\n');
	}

	int reply;

	while(1){
		if(xQueueReceive(xQueue_delete_task_message, &reply, 30)) {
			break;
		}
	}
	vTaskDelete(t_handle);
	vQueueDelete(xQueue_delete_task);
	vQueueDelete(xQueue_delete_task_message);
	xQueue_delete_task = 0;
	xQueue_delete_task_message = 0;
}

// TASK LISTS
/*-----------------------------------------------------------*/
int get_active_dd_task_list(struct dd_task_list* head) {

	struct dd_task_list *current = head;
	int i = 0;

	while(current->next_task != NULL) {
		current = current->next_task;

		i++;
	}
//    xQueue_active_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
//    xQueue_active_list_message = xQueueCreate(1, sizeof(uint32_t));
//
//    vQueueAddToRegistry(xQueue_active_task_list, "Active list");
//    vQueueAddToRegistry(xQueue_active_list_message, "Active message");
//
//    int message = 1;
//    int reply;
//
//    if(!xQueueOverwrite(xQueue_active_list_message, &message)){
//        printf("Failed to send active task message\n");
//    }
//
//    while(1){
//        if(xQueuePeek(xQueue_active_list_message, &reply, 30)) {
//        	if (reply == 2) {
//        		break;
//        	}
//        }
//    }
//    vQueueDelete(xQueue_active_list_message);
//    xQueue_active_list_message = 0;

    return i;
}

int get_complete_dd_task_list(struct dd_task_list* head) {
	struct dd_task_list *current = head;
	int i = 0;

	while(current->next_task != NULL) {
		current = current->next_task;

		i++;
	}
	return i;
//    xQueue_complete_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
//    xQueue_complete_list_message = xQueueCreate(1, sizeof(uint32_t));
//
//    vQueueAddToRegistry(xQueue_complete_task_list, "Complete list");
//    vQueueAddToRegistry(xQueue_complete_list_message, "Complete message");
//
//    int message = 1;
//    int reply;
//
//    if(!xQueueOverwrite(xQueue_complete_list_message, &message)){
//        printf("Failed to send complete task message\n");
//    }
//
//    while(1){
//        if(xQueuePeek(xQueue_complete_list_message, &reply, 30)) {
//        	if (reply == 2) {
//        		break;
//        	}
//        }
//    }
//    vQueueDelete(xQueue_complete_list_message);
//    xQueue_complete_list_message = 0;

}

int get_overdue_dd_task_list(struct dd_task_list* head) {
	struct dd_task_list *current = head;
	int i = 0;

	while(current->next_task != NULL) {
		current = current->next_task;

		i++;
	}
	return i;
//    xQueue_overdue_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
//    xQueue_overdue_list_message = xQueueCreate(1, sizeof(uint32_t));
//
//    vQueueAddToRegistry(xQueue_overdue_task_list, "Overdue list");
//    vQueueAddToRegistry(xQueue_overdue_list_message, "Overdue message");
//
//    int message = 1;
//    int reply;
//
//    if(!xQueueOverwrite(xQueue_overdue_list_message, &message)){
//        printf("Failed to send overdue task message\n");
//    }
//
//    while(1){
//        if(xQueuePeek(xQueue_overdue_list_message, &reply, 30)) {
//        	if (reply == 2) {
//        		break;
//        	}
//        }
//    }
//    vQueueDelete(xQueue_overdue_list_message);
//    xQueue_overdue_list_message = 0;

}

void dd_remove_overdue(struct dd_task_list* activeHead, struct dd_task_list* overdueHead){
	struct dd_task_list *current = activeHead;
	struct dd_task_list *previous = activeHead;

	while (current != NULL) {
		while (current != NULL && current->task.absolute_deadline > (uint32_t)xTaskGetTickCount()){
			previous = current;
			current = current->next_task;
		}
		if(current->task.release_time != -1 && current->task.completion_time == -1){
			previous = current;
			current = current->next_task;
			continue;
		}
		if (current == NULL) {
			return;
		}

		previous->next_task = current->next_task;
		addToList(overdueHead, current->task, 0);
		printf("Overdue Task: %d at: %d\n", (int)current->task.task_id, (int) xTaskGetTickCount());
		current = previous->next_task;
	}
}

void addToList(struct dd_task_list *head, struct dd_task next_task, int f_sort) {
	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list *current = head;

	new_node->task = next_task;
	new_node->next_task = NULL;

	if(f_sort == 1) {
		while(current->next_task != NULL) {
			if (new_node->task.absolute_deadline < current->next_task->task.absolute_deadline) {
				new_node->next_task = current->next_task;
				current->next_task = new_node;
				return;
			} else {
				current = current->next_task;
			}
		}
	}
	else {
		while(current->next_task != NULL) {
			current = current->next_task;
		}
	}
	current->next_task = new_node;

}


//void addPriorityToList(struct dd_task_list *head, struct dd_task next_task) {
//	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
//	struct dd_task_list *current = head;
//
//	new_node->task = next_task;
//
//	while (current->next_task != NULL) {
////		Check current and new node absolute deadline
//		if (current->next_task->task.absolute_deadline > new_node->task.absolute_deadline) {
//			new_node->next_task = current->next_task;
//			current->next_task = new_node;
//			return;
//		}
//		current = current->next_task;
//	}
//	new_node->next_task = current->next_task;
//	current->next_task = new_node;
//}
//
//void removeFromList(struct dd_task_list *head, uint32_t task_id) {
//	struct dd_task_list *current = head;
//	struct dd_task_list *previous = head;
//	while (current->task.task_id != task_id){
//		if(current->next_task == NULL){
//			printf("ID not found\n");
//			return;
//		}
//		previous = current;
//		current = current->next_task;
//	}
//	if (current->next_task == NULL){
//		previous->next_task = NULL;
//	} else {
//		previous->next_task = current->next_task;
//	}
//	free(current);
//}

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
