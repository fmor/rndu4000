#ifndef RNDU4000_LED_HEADER
#define RNDU4000_LED_HEADER


#include <linux/kconfig.h>
#include <linux/compiler_types.h>
#include <linux/device.h>

#include "context.h"


enum Led_mode
{
    LED_MODE_OFF,
    LED_MODE_ON,
    LED_MODE_BLINK
};

enum Led
{
    LED_BACKUP = 0,
    LED_POWER  = 1,
    LED_DISK_1 = 2,
    LED_DISK_2 = 3,
    LED_DISK_3 = 4,
    LED_DISK_4 = 5,
};

int led_init( struct Context* ctx );
void led_exit( void );

enum Led_mode led_get_mode( enum Led led );
void led_set_mode( enum Led led, enum Led_mode mode );
void led_set_all_to_mode( enum Led_mode mode );
enum Led led_from_name( const char* name );
ssize_t led_sysfs_show( struct device* dev, struct device_attribute* attr, char* buffer );
ssize_t led_sysfs_store( struct device* dev, struct device_attribute* attr, const char* buffer, size_t count );
int led_sysfs_init( struct kobject* root );
void led_sysfs_exit( void );

#endif
