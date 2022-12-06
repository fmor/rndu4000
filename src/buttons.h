#ifndef RNDU400_BUTTONS_HEADERS
#define RNDU400_BUTTONS_HEADERS

#include <linux/kconfig.h>
#include <linux/compiler_types.h>

#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/device.h>

#include "context.h"


int  buttons_init( struct Context* ctx );
void buttons_exit( void );

int     buttons_sysfs_init( void );
void    buttons_sysfs_exit( void );
ssize_t buttons_sysfs_show( struct device* dev, struct device_attribute* attr, char* buff );
ssize_t buttons_sysfs_store( struct device* dev, struct device_attribute* attr, const char* buffer, size_t buffer_size  );


void buttons_check_timer_callback( struct timer_list* t );
void button_work_cmd( struct work_struct* work );


#endif
