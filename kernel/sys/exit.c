#include <kernel/process.h>

/*===========================================================================*
 *                                  sys_exit                                 *
 *===========================================================================*/
int sys_exit(int return_value) {

    process_current->errno = return_value;
    strcpy(process_current->name, "<defunct>");
    process_exit(process_current);

    return 0;

}
