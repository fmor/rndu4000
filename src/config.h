#ifndef RNDU4000_CONFIG_HEADER
#define RNDU4000_CONFIG_HEADER

// LPC page 534 of doc/ich9
#define LPC_VENDOR_ID       0x8086
#define LPC_DEVICE_ID       0x2918
#define LPC_GPIO_REGISTERS_IO_BASE_ADDR  0x48
#define LPC_GPIO_REGISTERS_MEM_SIZE      64
#define LPC_GPIO_USE_SEL   0x00 // 0 : Use as native func, 1 : Use a gpio
#define	LPC_GP_IO_SEL      0x04
#define	LPC_GP_LVL 		   0x0c
#define	LPC_GPO_BLINK	   0x18
#define	LPC_GPI_INV        0x2c
#define	LPC_GPIO_USE_SEL2  0x30
#define	LPC_GP_IO_SEL2	   0x34
#define	LPC_GP_LVL2		   0x38


// Leds ( SEL )
#define LED_GPIO_PIN_BACKUP      22
#define LED_GPIO_PIN_POWER       28
#define LED_GPIO_PIN_DISK0       16
#define LED_GPIO_PIN_DISK1       20
#define LED_GPIO_PIN_DISK2       6
#define LED_GPIO_PIN_DISK3       7


// Buttons ( SEL )
#define BUTTON_GPIO_PIN_BACKUP   4
#define BUTTON_GPIO_PIN_RESET    5

// Lcd ( SEL2 )
#define LCD_COLS            16
#define LCD_ROWS            2
#define LCD_ESCAPE_CHAR     0x1b
#define LCD_DEV_NAME        "lcd"
#define LCD_DEV_ESCAPE_SEQUENCE_MAX_LENGTH 6
#define LCD_GPIO_PIN_E      0
#define LCD_GPIO_PIN_RS     1
#define LCD_GPIO_PIN_RW     2
#define LCD_GPIO_PIN_D4     4
#define LCD_GPIO_PIN_D5     5
#define LCD_GPIO_PIN_D6     6
#define LCD_GPIO_PIN_D7     7
#define LCD_GPIO_PIN_BL    16

#endif
