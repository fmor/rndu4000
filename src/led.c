#include "led.h"

#include <linux/io.h>
#include <linux/module.h>


#include "config.h"



static struct device_attribute led_backup_attr = __ATTR( backup, 0660, led_sysfs_show, led_sysfs_store );
static struct device_attribute led_power_attr  = __ATTR( power,  0660, led_sysfs_show, led_sysfs_store );
static struct device_attribute led_disk_0_attr = __ATTR( disk1,  0660, led_sysfs_show, led_sysfs_store );
static struct device_attribute led_disk_1_attr = __ATTR( disk2,  0660, led_sysfs_show, led_sysfs_store );
static struct device_attribute led_disk_2_attr = __ATTR( disk3,  0660, led_sysfs_show, led_sysfs_store );
static struct device_attribute led_disk_3_attr = __ATTR( disk4,  0660, led_sysfs_show, led_sysfs_store );




static struct Context* ctx   = NULL;
static struct kobject* k_leds = NULL;
static int led_gpio[6] =
{
    LED_GPIO_PIN_BACKUP,
    LED_GPIO_PIN_POWER,
    LED_GPIO_PIN_DISK0,
    LED_GPIO_PIN_DISK1,
    LED_GPIO_PIN_DISK2,
    LED_GPIO_PIN_DISK3,
};

static u32 pin_led_mask = (1<<LED_GPIO_PIN_BACKUP)+ (1<<LED_GPIO_PIN_POWER) + (1<<LED_GPIO_PIN_DISK0) + (1<<LED_GPIO_PIN_DISK1) + (1<<LED_GPIO_PIN_DISK2) + (1<<LED_GPIO_PIN_DISK3);	/* 28 is power led */


int led_init( struct Context* _ctx )
{
    u32 data;
    int r;

    pr_debug( "%s/led : led_init", THIS_MODULE->name );

    ctx = _ctx;


    // Led : use pins as GPIO
    data = inl( ctx->lpc_gpio_use_sel );
    data |= pin_led_mask;
    outl( data, ctx->lpc_gpio_use_sel );

    // Led : use as output
    data = inl( ctx->lpc_gp_io_sel );
    data &= ~pin_led_mask;
    outl( data, ctx->lpc_gp_io_sel );

    // Led : off
    data = inl( ctx->lpc_gp_lvl );
    data |= pin_led_mask;
    outl( data, ctx->lpc_gp_lvl );

    // Led : no blink
    data = inl( ctx->lpc_gpo_blink );
    data &= ~pin_led_mask;
    outl( data, ctx->lpc_gpo_blink );


    // /sys
    r = led_sysfs_init( &ctx->device->kobj  );
    if( r != 0 )
        return -1;

    led_set_mode( LED_POWER, LED_MODE_ON );

    return 0;
}

void led_exit( void )
{
    pr_debug( "%s/led : led_exit", THIS_MODULE->name );
    led_set_all_to_mode( LED_MODE_OFF );
    led_sysfs_exit();
    ctx = NULL;
}

enum Led_mode led_get_mode( enum Led led )
{
    u32 data;
    unsigned long flags;
    int pin;
    enum Led_mode mode;
    bool enable;
    bool blinking;

    data = 0;
    flags = 0;
    pin = led_gpio[ led ];

    enable = 0;
    blinking = 0;

    spin_lock_irqsave( &ctx->lock, flags );
    data = inl( ctx->lpc_gp_lvl );
    spin_unlock_irqrestore( &ctx->lock, flags );

    enable = test_bit(  pin, (void*) &data );
    if( enable == false )
    {
        data= inl( ctx->lpc_gpo_blink );
        blinking = test_bit( pin, (void*) &data );
        mode = blinking ? LED_MODE_BLINK : LED_MODE_ON;
    }
    else
        mode = LED_MODE_OFF;


    return mode;
}

void led_set_mode( enum Led led, enum Led_mode mode )
{
    u32 data;
    int pin;

    data = 0;
    pin = led_gpio[ led ];

    switch( mode )
    {
        case LED_MODE_OFF:
            pr_debug( "%s/led :  Setting led %d to OFF\n", THIS_MODULE->name, led  );
            data = inl( ctx->lpc_gp_lvl );
            set_bit( pin, (void*) &data );
            outl( data, ctx->lpc_gp_lvl );

            // Clear blink
            data = inl( ctx->lpc_gpo_blink );
            clear_bit( pin, (void*) &data );
            outl( data, ctx->lpc_gpo_blink );
        break;


        case LED_MODE_ON:
            pr_debug( "%s/led :  Setting led %d to ON\n", THIS_MODULE->name, led  );
            data = inl( ctx->lpc_gp_lvl );
            clear_bit( pin, (void*) &data );
            outl( data, ctx->lpc_gp_lvl );

            // Clear blink
            data = inl( ctx->lpc_gpo_blink );
            clear_bit( pin, (void*) &data );
            outl( data, ctx->lpc_gpo_blink );
        break;


        case LED_MODE_BLINK:
            pr_debug( "%s/led :  Setting led %d to BLINK\n", THIS_MODULE->name, led  );
            data = inl( ctx->lpc_gp_lvl );
            clear_bit( pin, (void*) &data );
            outl( data, ctx->lpc_gp_lvl );

            data = inl( ctx->lpc_gpo_blink );
            set_bit( pin, (void*) &data );
            outl( data, ctx->lpc_gpo_blink );
        break;
    }


}


void led_set_all_to_mode( enum Led_mode mode )
{
    int i;

    pr_debug( "%s/led : led_set_all_to_mode %d\n", THIS_MODULE->name,  mode );

    for( i = 0; i < 6; ++i )
        led_set_mode( i, mode );
}

enum Led led_from_name( const char* name )
{
    if( strcmp( "backup", name) == 0 )
        return LED_BACKUP;

    if( strcmp( "power", name) == 0 )
        return LED_POWER;

    if( strcmp( "disk1", name) == 0 )
        return LED_DISK_1;

    if( strcmp( "disk2", name) == 0 )
        return LED_DISK_2;

    if( strcmp( "disk3", name) == 0 )
        return LED_DISK_3;

    if( strcmp( "disk4", name) == 0 )
        return LED_DISK_4;

    return -1;
}

ssize_t led_sysfs_show( struct device* dev, struct device_attribute* attr, char* buffer )
{
    ssize_t sz;
    enum Led led;
    enum Led_mode mode;

    led = led_from_name( attr->attr.name );
    if( led == -1 )
    {
        pr_err( "%s/led : Failed to get led from name '%s'\n", THIS_MODULE->name, attr->attr.name );
        return 0;
    }

    mode = led_get_mode( led );
    sz = sprintf( buffer, "%d\n", mode );

    pr_debug(  "%s/led : Get Mode for led %s : mode=%d\n", THIS_MODULE->name, attr->attr.name, mode );
    return sz;
}



ssize_t led_sysfs_store( struct device* dev, struct device_attribute* attr, const char* buffer, size_t count )
{
    enum Led led;
    enum Led_mode mode;
    unsigned long flags;

    if( count == 0 )
        return 0;

    mode = -1;


    if( strcmp( "backup", attr->attr.name) == 0 )
        led = LED_BACKUP;
    else if( strcmp( "power", attr->attr.name) == 0 )
        led = LED_POWER;
    else if( strcmp( "disk1", attr->attr.name) == 0 )
        led = LED_DISK_1;
    else if( strcmp( "disk2", attr->attr.name) == 0 )
        led = LED_DISK_2;
    else if( strcmp( "disk3", attr->attr.name) == 0 )
        led = LED_DISK_3;
    else if( strcmp( "disk4", attr->attr.name) == 0 )
        led = LED_DISK_4;

    else
        return count;

    if( buffer[0] == '0' )
        mode = LED_MODE_OFF;

    else if( buffer[0] == '1' )
        mode = LED_MODE_ON;

    else if( buffer[0] == '2' )
        mode = LED_MODE_BLINK;

    else
        pr_err( "%s/led : bad store file\n", THIS_MODULE->name  );


    if( mode != -1 )
    {
        flags = 0;
        spin_lock_irqsave( &ctx->lock, flags );
        led_set_mode(  led, mode );
        spin_unlock_irqrestore( &ctx->lock, flags );
    }

    return count;
}

int led_sysfs_init( struct kobject* root )
{
    int r;


    // LEDS
    k_leds = kobject_create_and_add( "leds", root );
    if( k_leds  == NULL )
        goto LBL_FAILED;

    // LEDS : Backup
    r = sysfs_create_file( k_leds, &led_backup_attr.attr );
    if( r != 0 )
    {
        printk( KERN_ERR "Failed to create sys/devices/rn/leds/backup\n" );
        goto LBL_FAILED_LEDS_BACKUP;
    }

    // LEDS : Power
    r = sysfs_create_file( k_leds, &led_power_attr.attr );
    if( r != 0 )
    {
        printk( KERN_ERR "%s : Failed to create sysfs file leds/power\n", module_name(THIS_MODULE) );
        goto LBL_FAILED_LEDS_POWER;
    }

    // LEDS : Disk
    r = sysfs_create_file( k_leds, &led_disk_0_attr.attr );
    if( r != 0 )
        goto LBL_FAILED_LEDS_DISKS_0;

    r = sysfs_create_file( k_leds, &led_disk_1_attr.attr );
    if( r != 0 )
        goto LBL_FAILED_LEDS_DISKS_1;

    r = sysfs_create_file( k_leds, &led_disk_2_attr.attr );
    if( r != 0 )
        goto LBL_FAILED_LEDS_DISKS_2;

    r = sysfs_create_file( k_leds, &led_disk_3_attr.attr );
    if( r != 0 )
        goto LBL_FAILED_LEDS_DISKS_3;

    return 0;

LBL_FAILED_LEDS_DISKS_3:
    sysfs_remove_file( k_leds, &led_disk_2_attr.attr );

LBL_FAILED_LEDS_DISKS_2:
    sysfs_remove_file( k_leds, &led_disk_1_attr.attr );

LBL_FAILED_LEDS_DISKS_1:
    sysfs_remove_file( k_leds, &led_disk_0_attr.attr );

LBL_FAILED_LEDS_DISKS_0:
    sysfs_remove_file( k_leds, &led_power_attr.attr );

LBL_FAILED_LEDS_POWER:
    sysfs_remove_file( k_leds, &led_backup_attr.attr );

LBL_FAILED_LEDS_BACKUP:
    kobject_put( k_leds );
    k_leds = NULL;

LBL_FAILED:
    return -1;
}

void led_sysfs_exit( void )
{
    sysfs_remove_file( k_leds, &led_disk_0_attr.attr );
    sysfs_remove_file( k_leds, &led_disk_1_attr.attr );
    sysfs_remove_file( k_leds, &led_disk_2_attr.attr );
    sysfs_remove_file( k_leds, &led_disk_3_attr.attr );
    sysfs_remove_file( k_leds, &led_backup_attr.attr );
    sysfs_remove_file( k_leds, &led_power_attr.attr );
    kobject_put( k_leds );
    k_leds = NULL;
}


