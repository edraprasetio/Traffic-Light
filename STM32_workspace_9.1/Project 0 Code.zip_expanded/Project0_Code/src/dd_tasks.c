#include "dd_tasks.h"


void addToList(struct dd_task_list*, struct dd_task, int);
void dd_remove_overdue(struct dd_task_list*, struct dd_task_list*);
struct dd_task dd_delete(struct dd_task_list*, uint32_t);
int get_active_dd_task_list(struct dd_task_list*);
int get_complete_dd_task_list(struct dd_task_list*);
int get_overdue_dd_task_list(struct dd_task_list*);
void create_dd_task(uint32_t , uint32_t , uint32_t, uint32_t );

void Create_UD_Task(void *pvParameters){
//	printf("Creating UD task\n");
	struct dd_task* curr_task = (struct dd_task*) pvParameters;
//	Get tick count
	int start = (int)xTaskGetTickCount();
	int end = start + (int) curr_task->execution_time;

//	Keep running before task ends
	while ((int)xTaskGetTickCount() < end);
	uint32_t task_id = curr_task->task_id;
	curr_task->completion_time = (uint32_t) xTaskGetTickCount();

//	Complete task and send to queue
	if(!xQueueSend(Complete_Queue, &task_id, 0)) {
			printf("Failed to send completed task to queue \n");
	}
	vTaskSuspend(NULL);
}



void create_dd_task(uint32_t type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline) {

	struct dd_task new_dd_task;
	new_dd_task.type = type;
	new_dd_task.task_id = task_id;
	new_dd_task.execution_time = execution_time;
	new_dd_task.absolute_deadline = absolute_deadline;
	new_dd_task.release_time = -1;
	new_dd_task.completion_time = -1; //use l8er

	if( !xQueueSend( Create_Queue, &new_dd_task, 0)) {
		printf("Failed to create a task\n");
	}

}


struct dd_task dd_delete(struct dd_task_list* head, uint32_t task_id){
//	printf("Deleting a deadly driven task\n");
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

// TASK LISTS
/*-----------------------------------------------------------*/
int get_active_dd_task_list(struct dd_task_list* head) {

	struct dd_task_list *current = head;
	int i = 0;

	while(current->next_task != NULL) {
		current = current->next_task;

		i++;
	}

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


}

int get_overdue_dd_task_list(struct dd_task_list* head) {
	struct dd_task_list *current = head;
	int i = 0;

	while(current->next_task != NULL) {
		current = current->next_task;

		i++;
	}
	return i;

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