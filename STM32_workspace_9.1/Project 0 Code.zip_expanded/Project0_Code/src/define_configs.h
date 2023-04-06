#ifndef DEFINE_CONFIGS_H
#define DEFINE_CONFIGS_H

#ifdef __cplusplus
extern "C" {
#endif

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

typedef enum {PERIODIC, APERIODIC} task_type;

xQueueHandle active_task_queue = 0;
xQueueHandle complete_task_queue = 0;
xQueueHandle overdue_task_queue = 0;
xQueueHandle Complete_Queue = 0;
xQueueHandle Create_Queue = 0;



#ifdef __cplusplus
}
#endif

#endif /* DEFINE_CONFIGS_H */