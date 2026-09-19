#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void rg_robgo_video_init(void);
void rg_robgo_video_end(void);
void rg_robgo_video_window(int x, int y, int w, int h);
void rg_robgo_video_write(const uint16_t *pixels, size_t count);
void rg_robgo_input_init(void);
uint32_t rg_robgo_gamepad(void);
void rg_robgo_keys(uint8_t keys[256]);
uint32_t rg_robgo_mouse(void);
#ifdef __cplusplus
}
#endif
