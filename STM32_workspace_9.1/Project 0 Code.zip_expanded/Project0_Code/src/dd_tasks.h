#ifndef DD_TASKS_H
#define DD_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "define_configs.h"




void addToList(struct dd_task_list*, struct dd_task, int);
void dd_remove_overdue(struct dd_task_list*, struct dd_task_list*);
struct dd_task dd_delete(struct dd_task_list*, uint32_t);
int get_active_dd_task_list(struct dd_task_list*);
int get_complete_dd_task_list(struct dd_task_list*);
int get_overdue_dd_task_list(struct dd_task_list*);
void create_dd_task(uint32_t , uint32_t , uint32_t, uint32_t );



#ifdef __cplusplus
}
#endif

#endif /* DD_TASKS_H */