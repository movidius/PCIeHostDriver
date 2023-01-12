///
/// @file      rtems_config.c
/// @copyright All code copyright Intel Corporation 2018, all rights reserved.
///            For License Warranty see: common/license.txt
///
/// @brief     Application configuration Leon code
///
#include <bsp.h>
#include <rtems.h>
#include <rtems/bspIo.h>

#include "fatalExtension.h"
#include "OsDrvCpr.h"
#include "OsDrvInit.h"

// LEON L1 and L2 cache configuration.
BSP_SET_L1C_CONFIG(1,1);
BSP_SET_L2C_CONFIG(1, DEFAULT_RTEMS_L2C_REPLACEMENT_POLICY,
                   DEFAULT_RTEMS_L2C_LOCKED_WAYS, DEFAULT_RTEMS_L2C_MODE, 0, 0);


// Entry-point forward declaration.
void *POSIX_Init(void *args);

// RTEMS configuration.
#define CONFIGURE_INIT
#define CONFIGURE_MICROSECONDS_PER_TICK             1000    // 1 millisecond
#define CONFIGURE_TICKS_PER_TIMESLICE               10      // 10 milliseconds
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_POSIX_INIT_THREAD_TABLE
#define CONFIGURE_MINIMUM_TASK_STACK_SIZE           4096
#define CONFIGURE_MAXIMUM_TASKS                     8
#define CONFIGURE_MAXIMUM_POSIX_THREADS             2
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES            2
#define CONFIGURE_MAXIMUM_TIMERS                    4
#define CONFIGURE_MAXIMUM_SEMAPHORES                20
#define CONFIGURE_MAXIMUM_DRIVERS                   6
#define CONFIGURE_MAXIMUM_USER_EXTENSIONS           1
#define CONFIGURE_INITIAL_EXTENSIONS                { .fatal = Fatal_extension }
#define CONFIGURE_APPLICATION_EXTRA_DRIVERS         OS_DRV_INIT_TABLE_ENTRY

#include <rtems/confdefs.h>
