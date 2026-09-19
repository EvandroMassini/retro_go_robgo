#include "rg_robgo.h"
static uint16_t lcd_buffer[LCD_BUFFER_LENGTH];
static void lcd_init(void) { rg_robgo_video_init(); }
static void lcd_deinit(void) { rg_robgo_video_end(); }
static void lcd_sync(void) {}
static void lcd_set_rotation(int rotation) {}
static void lcd_set_backlight(float percent) {}
static void lcd_set_window(int x, int y, int w, int h) { rg_robgo_video_window(x,y,w,h); }
static inline uint16_t *lcd_get_buffer(size_t length) { return lcd_buffer; }
static inline void lcd_send_buffer(uint16_t *buffer, size_t length) { rg_robgo_video_write(buffer,length); }
