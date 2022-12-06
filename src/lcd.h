#ifndef RNDU4000_LCD_HEADER
#define RNDU4000_LCD_HEADER

#include <linux/kconfig.h>
#include <linux/compiler_types.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/fs.h>


struct Context;





int  lcd_init( struct Context* ctx );
void lcd_exit( void );


// dev
int  lcd_dev_init( void );
void lcd_dev_exit( void );
int  lcd_dev_open(struct inode *inode, struct file *file);
int  lcd_dev_release(struct inode *inode, struct file *file);
long lcd_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t lcd_dev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
ssize_t lcd_dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset );


// sys
int  lcd_sysfs_init( struct kobject* root  );
void lcd_sysfs_exit( void );
ssize_t lcd_sysfs_show( struct device* dev, struct device_attribute* attr, char* buff );
ssize_t lcd_sysfs_store( struct device* dev, struct device_attribute* attr, const char* buffer, size_t buffer_size  );

void lcd_reset( void );
void lcd_send_e( u32 state );
void lcd_send_cmd( u16 msg);
void lcd_cmd_clear( void );
void lcd_cmd_display_state( int display_enable, int cursor_visible, int cursor_blink );
void lcd_cmd_entry_set_mode( int cursor_move_left, int display_shift );
void lcd_cmd_set_cursor_pos( int col, int row );
void lcd_cmd_set_cursor_blink( int enable );
void lcd_cmd_set_cursor_visible( int visible );
void lcd_cmd_set_display_enable( int enable );
void lcd_cmd_set_backlight_enable( int enable );
void lcd_cmd_write_char( char c );
void lcd_cmd_write_string( const char* str );

void lcd_banner( void );


void lcd_upload_character( void );
void lcd_set_pogress( int row, int progress );
void test_lcd( void );


#endif
