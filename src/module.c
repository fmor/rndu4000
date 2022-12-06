#include <linux/kconfig.h>
#include <linux/compiler_types.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/ctype.h>

#include "context.h"
#include "lcd.h"
#include "led.h"
#include "buttons.h"

#include "config.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fmor");
MODULE_DESCRIPTION("Control leds and lcd panel of Netgear Readynas RNDU4000");
MODULE_VERSION( "1.0.0" );


static struct Context ctx =
{
        .mem_base = 0,
        .lock = __SPIN_LOCK_UNLOCKED( lock ),
};

static int __init rndu4000_init( void )
{
    int r;

    pr_debug("%s : Loading\n", THIS_MODULE->name );

    ctx.lpc = pci_get_device( LPC_VENDOR_ID, LPC_DEVICE_ID, NULL );
    if( ctx.lpc  == NULL )
    {
        pr_err( "%s : could not find pci device\n", THIS_MODULE->name  );
        goto LBL_FAILED_PCI_GET_DEVICE;
    }

    pr_debug( "%s : pci device found", THIS_MODULE->name );

    // Get base address from the LPC configuration space
    r = pci_read_config_dword( ctx.lpc, LPC_GPIO_REGISTERS_IO_BASE_ADDR, &ctx.mem_base );
    if( r != 0 )
    {
        pr_err( "%s : failed to read pci dev config dword ", THIS_MODULE->name);
        goto LBL_FAILED_GET_LPC_GPIO_REGISTERS_BASE_ADDR;
    }

    ctx.mem_base -= 1;
    pr_debug( "LPC_GPIO_BASE_ADDR = 0x%xh\n", ctx.mem_base );


    // Obtain excluse access to GPIO memory space
    if( ! request_region( ctx.mem_base, LPC_GPIO_REGISTERS_MEM_SIZE, THIS_MODULE->name ) )
    {
        pr_err( "%s : exclusive access to gpio registers failed\n", THIS_MODULE->name );
        goto LBL_FAILED_REQUEST_REGION;
    }
    pr_debug( "%s : Mem reserved\n", THIS_MODULE->name );


    // Set addrs of the gpio registers
    ctx.lpc_gpio_use_sel  = ctx.mem_base + LPC_GPIO_USE_SEL;
    ctx.lpc_gp_io_sel     = ctx.mem_base + LPC_GP_IO_SEL;
    ctx.lpc_gp_lvl        = ctx.mem_base + LPC_GP_LVL;
    ctx.lpc_gpo_blink     = ctx.mem_base + LPC_GPO_BLINK;
    ctx.lpc_gpi_inv       = ctx.mem_base + LPC_GPI_INV;
    ctx.lpc_gpio_use_sel2 = ctx.mem_base + LPC_GPIO_USE_SEL2;
    ctx.lpc_gp_io_sel2    = ctx.mem_base + LPC_GP_IO_SEL2;
    ctx.lpc_gp_lvl2       = ctx.mem_base + LPC_GP_LVL2;


    // /sys/devices/rn
    ctx.device = root_device_register( THIS_MODULE->name );
    if( ctx.device == NULL )
        goto LBL_FAILED_SYSFS_INIT;

     // LCD
    r = lcd_init( &ctx );
    if( r != 0 )
        goto LBL_FAILED_LCD_INIT;

    // LEDS
    r = led_init( &ctx );
    if( r != 0 )
        goto LBL_FAILED_LED_INIT;

    // Buttons
    r = buttons_init( &ctx );
    if( r != 0 )
        goto LBL_FAILED_BUTTONS_INIT;

    pr_info( "%s : Loaded\n", THIS_MODULE->name );


    return 0;

LBL_FAILED_BUTTONS_INIT:
    led_exit();

LBL_FAILED_LED_INIT:
    lcd_exit();

LBL_FAILED_LCD_INIT:
    root_device_unregister( ctx.device );
    ctx.device = NULL;

LBL_FAILED_SYSFS_INIT:
    ctx.lpc_gpio_use_sel  = 0;
    ctx.lpc_gp_io_sel     = 0;
    ctx.lpc_gp_lvl        = 0;
    ctx.lpc_gpo_blink     = 0;
    ctx.lpc_gpi_inv       = 0;
    ctx.lpc_gpio_use_sel2 = 0;
    ctx.lpc_gp_io_sel2    = 0;
    ctx.lpc_gp_lvl2       = 0;
    release_region( ctx.mem_base, LPC_GPIO_REGISTERS_MEM_SIZE );

LBL_FAILED_REQUEST_REGION:
LBL_FAILED_GET_LPC_GPIO_REGISTERS_BASE_ADDR:
    pci_dev_put( ctx.lpc );
    ctx.lpc = NULL;

LBL_FAILED_PCI_GET_DEVICE:
    return -ENODEV;
}


static void __exit rndu4000_exit( void )
{
    pr_debug( "%s :  Unloading\n", THIS_MODULE->name );

    buttons_exit();
    led_exit();
    lcd_exit();


    root_device_unregister( ctx.device );
    ctx.device = NULL;

    release_region( ctx.mem_base, LPC_GPIO_REGISTERS_MEM_SIZE );
    ctx.mem_base = 0;
    pr_debug( "%s : mem released", THIS_MODULE->name );

    pci_dev_put( ctx.lpc );
    ctx.lpc = NULL;

    pr_info("%s : Unloaded\n", THIS_MODULE->name );
}

module_init( rndu4000_init );
module_exit( rndu4000_exit );



