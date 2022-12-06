#include "lcd.h"
#include <linux/kconfig.h>
#include <linux/compiler_types.h>

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>



#include "context.h"
#include "config.h"



#define LCD_D0  (1<<0)
#define LCD_D1  (1<<1)
#define LCD_D2  (1<<2)
#define LCD_D3  (1<<3)
#define LCD_D4  (1<<4)
#define LCD_D5  (1<<5)
#define LCD_D6  (1<<6)
#define LCD_D7  (1<<7)
#define LCD_RS  (1<<8)
#define LCD_RW  (1<<9)
#define LCD_E   (1<<10)




enum LCD_flag
{
    LCD_FLAG_DISPLAY_ON                 = 1<<0,
    LCD_FLAG_CURSOR_VISIBLE             = 1<<1,
    LCD_FLAG_CURSOR_BLINK               = 1<<2,
    LCD_FLAG_BACKLIGHT                  = 1<<3,
};

static u32 pin_lcd_data_mask = (1<<4) | (1<<5) | (1<<6) | (1<<7);
static u32 pin_lcd_mask      = (1<<1)+(1<<2)+(1<<0)+(1<<4)+(1<<5)+(1<<6)+(1<<7)+(1<<16);

static struct device_attribute lcd_backlight_attr       = __ATTR( backlight, 0660, lcd_sysfs_show, lcd_sysfs_store );

static unsigned int flags       = 0;
static struct kobject* k_lcd    = NULL;
static struct Context* ctx      = NULL;
static int dev_major            = 0;
static struct class* dev_class  = NULL;
struct cdev dev_cdev            = {};
static atomic_t dev_in_use      = ATOMIC_INIT( 0 );
static int cursor_col             = 0;
static int cursor_row             = 0;


static char dev_buffer[ LCD_DEV_ESCAPE_SEQUENCE_MAX_LENGTH + 1 ] = {};
static int  dev_buffer_index    = 0;
enum lcd_parser_mode
{
    LCD_PARSER_CHAR_MODE,
    LCD_PARSER_ESCAPE_MODE,
};

static enum lcd_parser_mode parser_mode= LCD_PARSER_CHAR_MODE;

static const struct file_operations dev_fops = {
    .owner = THIS_MODULE,
    .write = lcd_dev_write,
    .open =  lcd_dev_open,
    .release = lcd_dev_release,
};

void lcd_reset_escape_buffer( void )
{
    memset( dev_buffer, 0, LCD_DEV_ESCAPE_SEQUENCE_MAX_LENGTH +1  );
    dev_buffer_index = 0;
}




int lcd_init( struct Context* _ctx )
{
    int r;
    u32 data;

    pr_debug( "%s/lcd : lcd_init\n", THIS_MODULE->name );


    ctx = _ctx;

    // Use pins connected to lcd as gpio
    pr_debug( "%s/lcd : Setup lcd gpio pins", THIS_MODULE->name );
    data = inl( ctx->lpc_gp_io_sel2 );
    data |= pin_lcd_mask;
    outl( data, ctx->lpc_gpio_use_sel2 );

    lcd_reset();

    r = lcd_dev_init();
    if( r != 0 )
        goto LBL_FAILED;


    r = lcd_sysfs_init( &ctx->device->kobj );
    if( r != 0 )
        goto LBL_FAILED_INIT_SYSFS;



    lcd_banner();
    return 0;

LBL_FAILED_INIT_SYSFS:
    lcd_dev_exit();

LBL_FAILED:
    flags = 0;
    ctx = NULL;
    return -1;
}


void lcd_exit( void )
{
    pr_debug( "%s/lcd : lcd_exit", THIS_MODULE->name );

    lcd_cmd_set_backlight_enable( 0 );
    lcd_sysfs_exit();
    lcd_dev_exit();

    ctx = NULL;
    pr_debug( "%s/lcd : lcd_exit END", THIS_MODULE->name );
}

////////////////////////////////////////////////////////////////////
// dev
int lcd_dev_init( void )
{
    struct device* device;

    device = NULL;

    pr_debug( "%s/lcd : lcd_init_devfs", THIS_MODULE->name );

    // cat /proc/devices | grep lcd
    dev_major = register_chrdev( 0, LCD_DEV_NAME, &dev_fops );
    if( dev_major < 0 )
    {
        pr_err( "%s/lcd : failed to register major number", THIS_MODULE->name  );
        goto LBL_FAILED;

    }

    dev_class = class_create( THIS_MODULE, LCD_DEV_NAME );
    if( dev_class == NULL )
        goto LBL_FAILED_CLASS_CREATE;

    device = device_create( dev_class, NULL, MKDEV(dev_major,0), NULL, LCD_DEV_NAME );
    if( device == NULL )
        goto LBL_FAILED_DEVICE_CREATE;

    pr_debug( "%s/lcd : lcd_init END", THIS_MODULE->name );
    return 0;

LBL_FAILED_DEVICE_CREATE:
    class_destroy( dev_class );
    dev_class = NULL;


LBL_FAILED_CLASS_CREATE:
    unregister_chrdev( dev_major, LCD_DEV_NAME );
    dev_major = 0;

LBL_FAILED:
    return -1;
}

void lcd_dev_exit( void )
{
    pr_debug( "%s/lcd : lcd_devfs_exit", THIS_MODULE->name  );

    device_destroy( dev_class, MKDEV(dev_major,0) );
    class_destroy( dev_class );
    dev_class = NULL;
    unregister_chrdev( dev_major, LCD_DEV_NAME );
}


int lcd_dev_open(struct inode *inode, struct file *file)
{
    int r;

    pr_debug("%s/lcd : lcd_dev_open\n", THIS_MODULE->name );

    r = atomic_cmpxchg( &dev_in_use, 0, 1 );
    if( r )
        return -EBUSY;

    try_module_get( THIS_MODULE );

    return 0;
}


int lcd_dev_release(struct inode *inode, struct file *file)
{
    pr_debug("%s/lcd : lcd_dev_release\n", THIS_MODULE->name );

    lcd_reset_escape_buffer();
    atomic_set( &dev_in_use, 0 );
    module_put( THIS_MODULE );

    return 0;
}

ssize_t lcd_dev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    (void) file;
    (void)  buf;
    (void) count;
    (void) offset;

    return -EINVAL;
}

ssize_t lcd_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    unsigned long flags;

    pr_debug( "%s/lcd : /dev/lcd write count=%d buf=%.*s\n", THIS_MODULE->name, (int) count,(int) count, buf );

    flags = 0;
    spin_lock_irqsave( &ctx->lock, flags );
    lcd_cmd_write_char( buf[0] );
    spin_unlock_irqrestore( &ctx->lock, flags );

    return 1;
}



////////////////////////////////////////////////////////////////////
// sysfs
int lcd_sysfs_init( struct kobject* root  )
{
    int r;

    pr_debug( "%s/lcd : lcd_init_sysfs", THIS_MODULE->name );

    k_lcd = kobject_create_and_add( "lcd", root );
    if( k_lcd == NULL )
        goto LBL_FAILED;

    // LCD : Backlight
    r = sysfs_create_file( k_lcd, &lcd_backlight_attr.attr );
    if( r != 0 )
    {
        pr_err( "%s/lcd : sysfs failed to create %s", THIS_MODULE->name, lcd_backlight_attr.attr.name );
        goto LBL_FAILED_CREATE_LCD_BACKLIGHT;
    }
    return 0;

LBL_FAILED_CREATE_LCD_BACKLIGHT:
    kobject_put( k_lcd );
    k_lcd = NULL;

LBL_FAILED:
    return -1;
}

void lcd_sysfs_exit( void )
{
    pr_debug( "%s/lcd : lcd_sysfs_exit", THIS_MODULE->name );

    // /sys/devices/rndu4000/lcd/backlight
    sysfs_remove_file( k_lcd, &lcd_backlight_attr.attr );

    // /sys/devices/rndu4000/lcd
    kobject_put( k_lcd );
    k_lcd = NULL;
}

ssize_t lcd_sysfs_show( struct device* dev, struct device_attribute* attr, char* buff )
{
    ssize_t sz;

    if( strcmp( attr->attr.name, "backlight") == 0 )
        sz = sprintf( buff, "%d\n", flags & LCD_FLAG_BACKLIGHT );
    else
        sz = 0;
    return sz;
}

ssize_t lcd_sysfs_store( struct device* dev, struct device_attribute* attr, const char* buffer, size_t buffer_size  )
{
    pr_debug( "buffer='%*s' buffer_size=%d\n", (int) buffer_size, buffer, (int) buffer_size );

    // backlight
    if( strcmp( attr->attr.name, "backlight") == 0 )
    {
        if( buffer_size >= 1 )
        {

            if( buffer[0] == '1' )
                lcd_cmd_set_backlight_enable( 1 );
            else if( buffer[0] == '0' )
                lcd_cmd_set_backlight_enable( 0 );
        }
        return buffer_size;
    }

    pr_debug( "%s : Unhandled store on sysfs file %s\n", "lcd", attr->attr.name );
    return -ENODEV;
}


void lcd_reset( void )
{
    pr_debug( "%s/lcd : lcd_reset", THIS_MODULE->name );

    flags = 0;

    lcd_send_cmd( 0x3 );
    mdelay( 5 );

    lcd_send_cmd( 0x3 );
    udelay( 100 );

    lcd_send_cmd( 0x3 );
    mdelay( 5 );

    lcd_send_cmd( 0x2 );
    udelay( 100 );

    // 4 Bits / 2 Lines / 5x11
    lcd_send_cmd( LCD_D5 | LCD_D3 | LCD_D2  );

    // Display on, cursor visible, cursor blink
    lcd_cmd_display_state(  1, 1, 1 );

    lcd_cmd_clear();

    // Entry set mode : cursor move left, display does not shift
    lcd_cmd_entry_set_mode( 1, 0 );
}

void lcd_send_e( u32 state )
{
    state |= ( 1 << LCD_GPIO_PIN_E );
    outl( state, ctx->lpc_gp_lvl2 );
    udelay( 50 );

    state &= ~( 1 << LCD_GPIO_PIN_E );
    outl(state, ctx->lpc_gp_lvl2 );
    udelay( 50 );
}

void lcd_send_cmd( u16 msg)
{
    u32 data;

    data = 0;

    if( msg & LCD_RS )
        data |= 1 << LCD_GPIO_PIN_RS;

    if( msg & LCD_RW )
        data |= 1 << LCD_GPIO_PIN_RW;

    // 4 Bit mode
    if( msg & LCD_D7  )
        data |= 1 << LCD_GPIO_PIN_D7;
    if( msg & LCD_D6 )
        data |= 1 << LCD_GPIO_PIN_D6;
    if( msg & LCD_D5 )
        data |= 1 << LCD_GPIO_PIN_D5;
    if( msg & LCD_D4 )
        data |= 1 << LCD_GPIO_PIN_D4;

    lcd_send_e( data );

    data &= ~( pin_lcd_data_mask );

    if( msg & LCD_D3  )
        data |= 1 << LCD_GPIO_PIN_D7;
    if( msg & LCD_D2 )
        data |= 1 << LCD_GPIO_PIN_D6;
    if( msg & LCD_D1 )
        data |= 1 << LCD_GPIO_PIN_D5;
    if( msg & LCD_D0 )
        data |= 1 << LCD_GPIO_PIN_D4;

    lcd_send_e( data );

    // We clear all gpio
    data = 0;
    outl( 0,  ctx->lpc_gp_lvl2 );

}

void lcd_cmd_clear( void )
{
    pr_debug( "%s/lcd : lcd_cmd_clear\n", THIS_MODULE->name );

    lcd_send_cmd( LCD_D0 );
    mdelay( 2 );
    cursor_col = 0;
    cursor_row = 0;
}

void lcd_cmd_display_state( int display_enable, int cursor_visible, int cursor_blink)
{
    u16 msg;

    msg = LCD_D3;

    // Display enable
    if( display_enable )
    {
        msg |= LCD_D2;
        flags |= LCD_FLAG_DISPLAY_ON;
    }
    else
        flags &= ~LCD_FLAG_DISPLAY_ON;

    // Cursor visible
    if( cursor_visible )
    {
        msg |= LCD_D1;
        flags |= LCD_FLAG_CURSOR_VISIBLE;
    }
    else
        flags &= ~LCD_FLAG_CURSOR_VISIBLE;

    // Cursor blink
    if( cursor_blink )
    {
        msg |= LCD_D0;
        flags |= LCD_FLAG_CURSOR_BLINK;
    }
    else
        flags &= ~LCD_FLAG_CURSOR_BLINK;

    lcd_send_cmd( msg  );
}

void lcd_cmd_entry_set_mode( int cursor_move_left, int display_shift)
{
    u16 msg;

    msg = LCD_D2;

    if( cursor_move_left )
        msg |= LCD_D1;

    if( display_shift )
        msg |= LCD_D0;

    lcd_send_cmd( msg );
}

void lcd_cmd_set_cursor_pos( int col, int row )
{
    u16 msg;

    pr_debug( "%s/lcd : lcd_cmd_set_cursor_pos to col=%d row=%d\n", THIS_MODULE->name, col, row );

    msg = LCD_D7;
    if( row == 1 )
        msg += 0x40;

    msg += col;
    lcd_send_cmd( msg );

    cursor_col = col;
    cursor_row = row;
}

void lcd_cmd_set_cursor_blink( int enable )
{
    pr_debug( "%s/lcd : lcd_cmd_set_cursor_blink\n", THIS_MODULE->name );
    lcd_cmd_display_state( flags & LCD_FLAG_DISPLAY_ON, flags & LCD_FLAG_CURSOR_VISIBLE, enable );
}

void lcd_cmd_set_cursor_visible( int visible )
{
    pr_debug( "%s/lcd : lcd_cmd_set_cursor_visible\n", THIS_MODULE->name );
    lcd_cmd_display_state( flags & LCD_FLAG_DISPLAY_ON, visible, flags & LCD_FLAG_CURSOR_BLINK );
}

void lcd_cmd_set_display_enable( int enable )
{
    pr_debug( "%s/lcd : lcd_cmd_set_display_enable\n", THIS_MODULE->name );
    lcd_cmd_display_state( enable, flags & LCD_FLAG_CURSOR_VISIBLE, flags & LCD_FLAG_CURSOR_BLINK );
}

void lcd_cmd_set_backlight_enable( int enable )
{
    u32 data;

    pr_debug( "%s/lcd : Setting lcd backlight to %d\n", THIS_MODULE->name, enable );

    data = inl( ctx->lpc_gp_lvl2 );
    if( enable )
    {
        data &= ~( 1 << LCD_GPIO_PIN_BL );
        flags |= LCD_FLAG_BACKLIGHT;
    }
    else
    {
        data |= ( 1 << LCD_GPIO_PIN_BL );
        flags &= ~LCD_FLAG_BACKLIGHT;
    }
    outl( data, ctx->lpc_gp_lvl2 );
    udelay( 500 );


}

int lcd_process_escape( void )
{

    // Clear
    if( strcmp( "[2J", dev_buffer) == 0 )
    {
        lcd_cmd_clear();
        return 1;
    }

    // Goto to home
    if( strcmp( "[H", dev_buffer) == 0 )
    {
        lcd_cmd_set_cursor_pos( 0, 0 );
        return 1;
    }

    // Cursor Visible on
    if( strcmp( "[LC", dev_buffer) == 0  )
    {
        lcd_cmd_set_cursor_visible( 1 );
        return 1;
    }

    // Cursor Visible off
    if( strcmp( "[Lc", dev_buffer) == 0 )
    {
        lcd_cmd_set_cursor_visible( 0 );
        return 1;
    }

    // Cursor blink on
    if( strcmp( "[LB", dev_buffer) == 0  )
    {
        lcd_cmd_set_cursor_blink( 1 );
        return 1;
    }

    // Cursor blink off
    if( strcmp( "[Lb", dev_buffer) == 0 )
    {
        lcd_cmd_set_cursor_blink( 0 );
        return 1;
    }

    // Backlight on
    if( strcmp( "[L+", dev_buffer) == 0  )
    {
        lcd_cmd_set_backlight_enable( 1 );
        return 1;
    }

    // backlight off
    if( strcmp( "[L-", dev_buffer) == 0 )
    {
        lcd_cmd_set_backlight_enable( 0 );
        return 1;
    }

    // Display enable
    if( strcmp( "[LD", dev_buffer) == 0 )
    {
        lcd_cmd_set_display_enable( 1 );
        return 1;
    }

    // Display disable
    if( strcmp( "[Ld", dev_buffer) == 0 )
    {
        lcd_cmd_set_display_enable( 0 );
        return 1;
    }


    return 0;

}

void lcd_cmd_write_char( char c)
{
    int r;

    pr_debug( "lcd_write_char '%d'\n", c );

    if( LCD_PARSER_ESCAPE_MODE == parser_mode )
    {
        dev_buffer[dev_buffer_index] = c;
        ++dev_buffer_index;
        dev_buffer[dev_buffer_index] = 0;

//        pr_debug( "--->escape = %s", dev_buffer );

        r = lcd_process_escape();
        if( r )
        {
            lcd_reset_escape_buffer();
            parser_mode = LCD_PARSER_CHAR_MODE;
        }
        else if( dev_buffer_index >= LCD_DEV_ESCAPE_SEQUENCE_MAX_LENGTH  )
        {
            pr_err( "%s/lcd : escape sequence too long\n", THIS_MODULE->name );
            lcd_reset_escape_buffer();
            parser_mode = LCD_PARSER_CHAR_MODE;
        }

    }
    else
    {
        switch( c )
        {
            case LCD_ESCAPE_CHAR:
//                pr_info( "--------------------Enter escape mode" );
                parser_mode = LCD_PARSER_ESCAPE_MODE;
                break;


            case '\n':
                lcd_cmd_set_cursor_pos( 0, ! cursor_row );
            break;




            default:
                lcd_send_cmd( LCD_RS | c  );
                ++cursor_col;
                if( cursor_col >= LCD_COLS )
                {
                    static int count = 0;
                    ++count;
//                    printk( "cursor_x=%d count=%d\n", cursor_col, count  );
                    lcd_cmd_set_cursor_pos( 0, ! cursor_row );
                }

        }

    }
}


void lcd_cmd_write_string(const char* str )
{
    int i;

    pr_debug( "%s/lcd : lcd_write_string BEGIN\n", THIS_MODULE->name  );
    i = 0;

    for(;;)
    {
        if( str[i] == 0 )
            break;

        lcd_cmd_write_char( str[i] );
        ++i;
    }
    pr_debug( "%s/lcd : lcd_write_string END\n", THIS_MODULE->name );
}

void lcd_upload_character( void )
{
    u16 msg;
    int pos = 0;
    int i;
    pos = 1;

    for( i = 0; i < 8; ++i  )
    {

        // Set CGRAM addr
        msg = LCD_D6;
        msg += pos<<3; // addr du char
        msg += i;
        lcd_send_cmd( msg );

        // Write data to CGRAM
        msg = LCD_RS;
        msg += 0b11111;
        lcd_send_cmd( msg );
    }
}



void lcd_set_pogress( int row, int progress )
{
    int i;
    int bars;
    char buffer[6];
    int str_len;


    progress = clamp( progress, 0, 100 );
    bars = progress / 10;
    sprintf( buffer, "%d %%", progress );
    str_len = strlen( buffer );

    lcd_cmd_set_cursor_pos( 0, row );
    for( i = 0; i < bars; ++i )
        lcd_cmd_write_char( 1 );

    for( ; i < LCD_COLS-str_len; ++i )
        lcd_cmd_write_char( ' ' );

    //    lcd_cmd_set_cursor_pos( LCD_COLS-str_len, row );

    for( i = 0 ; i < str_len; ++i )
        lcd_cmd_write_char( buffer[i] );


    if( bars < 10 )
    {
        lcd_cmd_set_cursor_pos( bars, row );
        lcd_cmd_set_cursor_blink( 1 );
    }
}

void lcd_banner( void )
{
    int pad;

    lcd_cmd_set_cursor_blink( 0 );
    lcd_cmd_set_cursor_visible( 0 );

    pad = ( LCD_COLS - strlen(THIS_MODULE->name) ) / 2;
    lcd_cmd_set_cursor_pos( pad, 0 );
    lcd_cmd_write_string( THIS_MODULE->name );

    pad = ( LCD_COLS - strlen(THIS_MODULE->version) ) / 2;
    lcd_cmd_set_cursor_pos( pad, 1 );
    lcd_cmd_write_string( THIS_MODULE->version );
}

void test_lcd( void )
{

    //    lcd_test_line();
    // Entry set mode : cursor move right, display does not shift

    lcd_cmd_write_string( "0123456789ABCDEF");
    lcd_cmd_write_string( "0123456789ABCDEF");
    lcd_cmd_write_string( "ABCD");


    /*
    lcd_cmd_set_cursor_pos( 10, 1 );
    lcd_cmd_write_string( "AA" );
    lcd_cmd_set_cursor_blink( 1 );
    lcd_cmd_set_cursor_pos( 9, 1 );
    lcd_cmd_set_cursor_visible( 0 );

    lcd_cmd_set_cursor_pos( 0, 0 );
    lcd_cmd_write_string("Hello" );
    */
}
