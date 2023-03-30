#include "dd_tasks.h"
#include "define_configs.h"

/*-----------------------------------------------------------*/

/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
static void prvSetupHardware( void );
static void DD_Task_Generator( void *pvParameters);
static void Create_UD_Task(void *pvParameters);
static void Deadline_Driven_Scheduler(void *pvParameters);
static void DD_Task_Monitor( void *pvParameters);



/*-----------------------------------------------------------*/

int main(void)
{

	prvSetupHardware();


	Complete_Queue = xQueueCreate( 20, sizeof(uint32_t));
	Create_Queue = xQueueCreate( 20, sizeof(struct dd_task));

	active_task_queue = xQueueCreate( 1, sizeof(struct dd_task_list));
	complete_task_queue = xQueueCreate( 1, sizeof(struct dd_task_list));
	overdue_task_queue = xQueueCreate(1, sizeof(struct dd_task_list));

	vQueueAddToRegistry( Complete_Queue, "Completed Task Queue");
	vQueueAddToRegistry( Create_Queue, "Created Task Queue");
	vQueueAddToRegistry( active_task_queue, "Active task list queue");
	vQueueAddToRegistry( complete_task_queue, "Complete task list queue");
	vQueueAddToRegistry( overdue_task_queue, "Overdue task list queue");

	xTaskCreate (DD_Task_Monitor, "Monitor", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
	xTaskCreate( DD_Task_Generator, "Generate", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	xTaskCreate (Deadline_Driven_Scheduler, "DDS", configMINIMAL_STACK_SIZE, NULL, 3, NULL);

	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/

void Deadline_Driven_Scheduler(void *pvParameters){
//	printf("DDS started\n");
	struct dd_task_list* curr_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list* fin_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	struct dd_task_list* missed_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));

	curr_tasks->next_task = NULL;
	fin_tasks->next_task = NULL;
	missed_tasks->next_task = NULL;

	struct dd_task new_task;
	uint32_t completed_task_id;
	int CPUAvailable = 1;
	while(1){

//		Check the created tasks queue and add them to the list
		if(xQueueReceive(Create_Queue, &new_task, 0)){
			addToList(curr_tasks, new_task, 1);
			printf("Received task: %d at: %d ms\n", (int)new_task.task_id, (int)xTaskGetTickCount());
		}

//		Check the completed tasks queue and delete the finished tasks
		if(xQueueReceive(Complete_Queue, &completed_task_id, 0)) {
			CPUAvailable = 1;
			struct dd_task removed_task = dd_delete(curr_tasks, completed_task_id);
			printf("Completed task: %d at: %d ms\n", (int)removed_task.task_id, (int)removed_task.completion_time);

			if(removed_task.completion_time < removed_task.absolute_deadline) {
				addToList(fin_tasks, removed_task, 0);
			}
			else{
				printf("Overdue Task: %d at: %d ms\n", (int)removed_task.task_id, (int)xTaskGetTickCount());
				addToList(missed_tasks, removed_task, 0);
			}

		}

		dd_remove_overdue(curr_tasks, missed_tasks);

		if(curr_tasks->next_task != NULL && CPUAvailable && (xQueuePeek(Create_Queue, &new_task, 0) == pdFALSE)) {

			CPUAvailable = 0;
			curr_tasks->next_task->task.release_time = (uint32_t)xTaskGetTickCount();
			printf("Released task: %d at: %d ms\n", (int)curr_tasks->next_task->task.task_id, (int)curr_tasks->next_task->task.release_time);
			xTaskCreate(Create_UD_Task, "UD Task", configMINIMAL_STACK_SIZE, &curr_tasks->next_task->task, 3, &(curr_tasks->next_task->task.t_handle));
//			dd_start(curr_tasks);
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
		//vTaskDelay(pdMS_TO_TICKS(1));

	}

}

void DD_Task_Monitor(void *pvParameters){
//	printf("Monitor starts \n");
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

	}
}

void DD_Task_Generator( void *pvParameters ){
	uint32_t i = 0;
	uint32_t test_bench = 2;
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
			vTaskDelay(pdMS_TO_TICKS(250));

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

		if (test_bench == 3) {
			test_period = 1500;
			create_dd_task(PERIODIC, 1, 100, 500 + i * test_period);
			create_dd_task(PERIODIC, 2, 200, 500 + i * test_period);
			create_dd_task(PERIODIC, 3, 200, 500 + i * test_period);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
	}
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
