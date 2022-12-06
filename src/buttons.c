#include "buttons.h"

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/device.h>
#include <uapi/asm-generic/param.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "config.h"



static struct Context* ctx = NULL;
static struct kobject* sysfs_buttons = NULL;
static struct timer_list buttons_check_timer = {};


struct Button
{
    const char* name;
    int     state;
    char*   cmdline;
    struct work_struct work;
    spinlock_t lock;
};

static struct Button backup_button = {
    .name = "Backup",
    .state = 0,
    .cmdline = NULL,
    .work = {},
    .lock = __SPIN_LOCK_UNLOCKED( lock ),
};

static struct Button reset_button = {
    .name = "Reset",
    .state = 0,
    .cmdline = NULL,
    .work = {},
    .lock = __SPIN_LOCK_UNLOCKED( lock ),
};


static struct device_attribute sysfs_backup_cmdline_attr  = __ATTR( backup_cmdline, 0660, buttons_sysfs_show, buttons_sysfs_store );
static struct device_attribute sysfs_reset_cmdline_attr   = __ATTR( reset_cmdline,  0660, buttons_sysfs_show, buttons_sysfs_store );

static inline void button_set_cmdline( struct Button* b, char* cmdline )
{
    pr_debug( "Setting button cmdline to %s\n", cmdline);

    spin_lock( &b->lock );
    if( b->cmdline )
        kfree( b->cmdline );
    b->cmdline = cmdline;
    spin_unlock( &b->lock );
}

static inline void button_clear( struct Button* b )
{
    b->state = 0;
    if( b->cmdline )
    {
        kfree( b->cmdline );
        b->cmdline = NULL;
    }
}

static inline void button_set_state( struct Button* b, int state  )
{
    if( state )
    {
        pr_debug( "%s/buttons : Button %s is triggered\n", THIS_MODULE->name, b->name );

        spin_lock( &b->lock );
        if( b->cmdline )
            schedule_work( &b->work );
        spin_unlock( &b->lock );

    }
    b->state = state;
}


int buttons_init( struct Context* _ctx )
{
    int r;
    u32 data;
    const u32 buttons_pin_mask = (1<<BUTTON_GPIO_PIN_BACKUP) + (1<<BUTTON_GPIO_PIN_RESET);

    pr_debug( "%s/buttons : buttons_init()\n", THIS_MODULE->name );

    ctx = _ctx;

    // Use buttons pins as GPIO
    data = inl( ctx->lpc_gpio_use_sel );
    data |= buttons_pin_mask;
    outl( data, ctx->lpc_gpio_use_sel );

    // use buttons as input
    data = inl( ctx->lpc_gp_io_sel );
    data |= buttons_pin_mask;
    outl( data, ctx->lpc_gp_io_sel );

    // Buttons
    INIT_WORK( &backup_button.work, button_work_cmd );
    INIT_WORK( &reset_button.work, button_work_cmd );


    // timer
    timer_setup( &buttons_check_timer, buttons_check_timer_callback, 0  );
    mod_timer( &buttons_check_timer, jiffies + msecs_to_jiffies(100) );

    // sysfs
    r = buttons_sysfs_init();
    if( r != 0 )
        goto LBL_FAILED_SYSFS_INIT;


    return 0;

LBL_FAILED_SYSFS_INIT:
    del_timer_sync( &buttons_check_timer );
    flush_work( &backup_button.work  );
    flush_work( &reset_button.work );

    return -1;
}

void buttons_exit( void )
{
    pr_debug( "%s/buttons : buttons_exit()\n", THIS_MODULE->name );

    // sysfs
    buttons_sysfs_exit();

    // Work
    del_timer_sync( &buttons_check_timer );
    flush_work( &backup_button.work);
    flush_work( &reset_button.work );


    button_clear( &backup_button );
    button_clear( &reset_button );

    ctx = NULL;
}

int  buttons_sysfs_init( void )
{
    int r;

    // /sys/devices/rndu4000/buttons
    sysfs_buttons = kobject_create_and_add( "buttons", &ctx->device->kobj );
    if( sysfs_buttons == NULL )
        goto LBL_FAILED_CREATE_BUTTONS;


    // /sys/devices/rndu4000/backup_cmdline
    r = sysfs_create_file( sysfs_buttons, &sysfs_backup_cmdline_attr.attr );
    if( r != 0 )
        goto LBL_FAILED_BACKUP_CMDLINE;


    // /sys/devices/rndu4000/reset_cmdline
    r = sysfs_create_file( sysfs_buttons, &sysfs_reset_cmdline_attr.attr );
    if( r != 0 )
        goto LBL_FAILED_RESET_CMDLINE;

    return 0;

LBL_FAILED_RESET_CMDLINE:
    sysfs_remove_file( sysfs_buttons, &sysfs_backup_cmdline_attr.attr );

LBL_FAILED_BACKUP_CMDLINE:
    kobject_put( sysfs_buttons );
    sysfs_buttons = NULL;

LBL_FAILED_CREATE_BUTTONS:
    return -1;
}

void buttons_sysfs_exit( void )
{
    sysfs_remove_file( sysfs_buttons, &sysfs_reset_cmdline_attr.attr );
    sysfs_remove_file( sysfs_buttons, &sysfs_backup_cmdline_attr.attr );
    kobject_put( sysfs_buttons );
    sysfs_buttons = NULL;
}

ssize_t buttons_sysfs_show( struct device* dev, struct device_attribute* attr, char* buff )
{
    ssize_t sz;
    struct Button* b;

    if( strcmp( "backup_cmdline", attr->attr.name) == 0 )
        b = &backup_button;
    else if( strcmp( "reset_cmdline", attr->attr.name) == 0 )
        b = &reset_button;

    // Should never happen
    else
        return -EAGAIN;



    sz = 0;
    spin_lock( &b->lock );
    if( b->cmdline )
    {
        strcpy( buff, b->cmdline );
        strcat( buff, "\n" );
        sz = strlen( buff ) + 1;
    }
    spin_unlock( &b->lock );

    return sz;
}



ssize_t buttons_sysfs_store( struct device* dev, struct device_attribute* attr, const char* buffer, size_t buffer_size  )
{

    char* cmdline;
    size_t cmdline_sz;



    pr_debug( "name='%s' buffer='%s' buffer_size = %d zero=%d terminate with enter=%d\n", attr->attr.name, buffer, (int)  buffer_size, buffer[buffer_size-1] == 0, buffer[buffer_size-1] == '\n' );


    cmdline_sz = buffer_size;

    if( buffer[buffer_size-1] == '\n' )
        --cmdline_sz;


    cmdline = (char*) kmalloc( cmdline_sz+1, GFP_KERNEL );
    if( cmdline == NULL )
    {
        pr_err( "%s/buttons : kmalloc failed !\n", THIS_MODULE->name );
        goto LBL_EXIT;
    }

    memcpy( cmdline, buffer, cmdline_sz );
    cmdline[ cmdline_sz ] = 0;
    pr_debug( "cmdline='%s' sz=%d buffer='%*s'\n", cmdline, (int) cmdline_sz, (int) buffer_size, buffer );

    if( strcmp( "backup_cmdline", attr->attr.name) == 0 )
    {
        button_set_cmdline( &backup_button, cmdline );
    }
    else if( strcmp( "reset_cmdline", attr->attr.name) == 0 )
    {
        button_set_cmdline( &reset_button, cmdline );
    }

    // This should never happen
    else
    {
        kfree( cmdline );
        cmdline = NULL;
        cmdline_sz = 0;
        return -ENODEV;
    }

LBL_EXIT:
    return buffer_size;
}



void buttons_check_timer_callback( struct timer_list* t )
{
    unsigned long irqdata;
    u32 data;
    int state;

    irqdata = 0;
    spin_lock_irqsave( &ctx->lock, irqdata );
    data = inl( ctx->lpc_gp_lvl );
    spin_unlock_irqrestore( &ctx->lock, irqdata );

    // Backup
    state = data & (1<< BUTTON_GPIO_PIN_BACKUP);
    state = ! state;
    if( state != backup_button.state )
        button_set_state( &backup_button, state );

    // Reset
    state = data & (1<< BUTTON_GPIO_PIN_RESET);
    state = ! state;
    if( state != reset_button.state )
        button_set_state( &reset_button, state );


    mod_timer( &buttons_check_timer, jiffies + msecs_to_jiffies(100) );
}

void button_work_cmd( struct work_struct* work )
{
    int r;
    struct Button* button;
    char* argv[4];
    char* envp[4];
    unsigned long flags;

    pr_debug( "%s/buttons : button_work_cmd\n", THIS_MODULE->name );


    button = container_of( work, struct Button, work );


    flags = 0;
    spin_lock_irqsave( &button->lock, flags );
    if( button->cmdline == NULL )
    {
        spin_unlock_irqrestore( &button->lock, flags );
        pr_debug( "%s/button : no cmdline defined\n", THIS_MODULE->name  );
        return;
    }

    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = button->cmdline;
    argv[3] = NULL;

    envp[0] = "HOME=/";
    envp[1] = "TERM=linux";
    envp[2] = "PATH=/sbin:/usr/sbin:/bin:/usr/bin";
    envp[3] = NULL;

    r = call_usermodehelper( argv[0], argv, envp, UMH_WAIT_EXEC );
    pr_debug( "%s/buttons : call_usermodehelper return=%d cmdline=%s\n", THIS_MODULE->name, r, button->cmdline );
    spin_unlock_irqrestore( &button->lock, flags );
}
