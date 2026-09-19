#include "rg_robgo.h"
#include "config.h"
#include "rg_input.h"
#include "rg_system.h"
#include "fabgl.h"
#if CONFIG_ESP32_ULP_COPROC_RESERVE_MEM < 4096
#error Reserve 4096 bytes of RTC memory for the FabGL PS2 ULP program
#endif
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include "driver/gpio.h"
#include "esp_heap_caps.h"

using namespace fabgl;
static VGAController video;
static uint8_t red_bits[32], green_bits[64], blue_bits[32], sync_bits;
static Keyboard keyboard;
static Mouse mouse;
static KeyboardLayout physical_layout;
static bool input_ready;
static int video_width, video_height;

static int win_x, win_y, win_w, win_h;
static size_t win_pos;
// Lê o perfil de pinos da LilyGO em /sd/bootl.rc.
static void load_robgo_ps2_pins(gpio_num_t *kbdData, gpio_num_t *kbdClk,
                                gpio_num_t *mouseData, gpio_num_t *mouseClk)
{
    int kd = 32, kc = 33;
    bool haveData = false, haveClock = false;
    FILE *fp = fopen("/sd/bootl.rc", "rb");
    if (fp) {
        RG_LOGI("bootl.rc: opened /sd/bootl.rc");
        unsigned char bom[3] = {};
        size_t n = fread(bom, 1, 3, fp);
        bool utf16 = n >= 2 && ((bom[0] == 0xFF && bom[1] == 0xFE) ||
                                (bom[0] == 0xFE && bom[1] == 0xFF));
        if (utf16) {
            RG_LOGW("bootl.rc: UTF-16 unsupported; save as UTF-8. Using LilyGO defaults.");
        } else {
            if (!(n == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF))
                rewind(fp);
            char line[128];
            while (fgets(line, sizeof(line), fp)) {
                int value;
                if (sscanf(line, " kbddat = %d", &value) == 1) { kd = value; haveData = true; }
                else if (sscanf(line, " kbdclk = %d", &value) == 1) { kc = value; haveClock = true; }
            }
            RG_LOGI("bootl.rc: kbddat=%d found=%d; kbdclk=%d found=%d", kd, haveData, kc, haveClock);
            if (ferror(fp) || !haveData || !haveClock ||
                !((kd == 32 && kc == 33) || (kd == 27 && kc == 26))) {
                RG_LOGW("bootl.rc: incomplete/invalid profile or read error; using DATA32 CLK33");
                kd = 32; kc = 33;
            }
        }
        fclose(fp);
    } else if (errno == ENOENT) {
        fp = fopen("/sd/bootl.rc", "w");
        if (fp) {
            bool ok = fputs("# PS/2 profile\nkbddat=32\nkbdclk=33\n", fp) >= 0;
            if (fclose(fp) != 0) ok = false;
            RG_LOGI("bootl.rc: create LilyGO defaults success=%d", ok);
        } else {
            RG_LOGW("bootl.rc: create failed errno=%d; using LilyGO defaults", errno);
        }
    } else {
        RG_LOGW("bootl.rc: open failed errno=%d; using LilyGO defaults", errno);
    }
    if (kd == 27 && kc == 26) {
        *kbdData = GPIO_NUM_27; *kbdClk = GPIO_NUM_26;
        *mouseData = GPIO_NUM_32; *mouseClk = GPIO_NUM_33;
        RG_LOGI("PS/2 profile: keyboard DATA=27 CLK=26; mouse DATA=32 CLK=33");
    } else {
        *kbdData = GPIO_NUM_32; *kbdClk = GPIO_NUM_33;
        *mouseData = GPIO_NUM_27; *mouseClk = GPIO_NUM_26;
        RG_LOGI("PS/2 profile: LilyGO default, keyboard DATA=32 CLK=33; mouse DATA=27 CLK=26");
    }
}

extern "C" void rg_robgo_video_init(void)
{
    // FabGL recebe primeiro o bit alto (1) e depois o bit baixo (0).
    video.begin(RG_GPIO_VGA_R0, RG_GPIO_VGA_R1, RG_GPIO_VGA_G0, RG_GPIO_VGA_G1,
                RG_GPIO_VGA_B0, RG_GPIO_VGA_B1, RG_GPIO_VGA_HSYNC, RG_GPIO_VGA_VSYNC);
    RG_LOGI("VGA before allocation: internal=%u largest=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    // DoubleScan repete as linhas via DMA. Cada pixel cobre dois clocks
    // do modo 640x480, mantendo a area ativa completa sem copia 2x2.
    video.setResolution(QVGA_320x240_60Hz, 320, 240, false);
    video_width = video.getViewPortWidth();
    video_height = video.getViewPortHeight();
    RG_LOGI("VGA64 DMA allocated=%dx%d requested=320x240 DoubleScan", video_width, video_height);
    if (video_width != 320 || video_height != 240)
        RG_LOGW("VGA allocation incomplete: clipping writes to prevent memory corruption");
    sync_bits = video.createRawPixel(RGB222(0, 0, 0));
    for (unsigned i = 0; i < 32; ++i) {
        unsigned level = (i * 3 + 15) / 31;
        red_bits[i] = video.createRawPixel(RGB222(level, 0, 0)) ^ sync_bits;
        blue_bits[i] = video.createRawPixel(RGB222(0, 0, level)) ^ sync_bits;
    }
    for (unsigned i = 0; i < 64; ++i)
        green_bits[i] = video.createRawPixel(RGB222(0, (i * 3 + 31) / 63, 0)) ^ sync_bits;

}
extern "C" void rg_robgo_video_end(void) { video.end(); }
extern "C" void rg_robgo_video_window(int x, int y, int w, int h)
{
    win_x=x; win_y=y; win_w=w; win_h=h; win_pos=0;
}
extern "C" void rg_robgo_video_write(const uint16_t *pixels, size_t count)
{
    if (win_w <= 0 || win_h <= 0 || !count)
        return;
    const int max_pos = win_w * win_h;
    while (count && (int)win_pos < max_pos) {
        const int col = (int)win_pos % win_w;
        const int py = win_y + (int)win_pos / win_w;
        int n = win_w - col;
        if (n > (int)count)
            n = (int)count;
        if (n > max_pos - (int)win_pos)
            n = max_pos - (int)win_pos;
        if (py >= 0 && py < video_height) {
            uint8_t *dest = video.getScanline(py);
            int px = win_x + col;
            // Recorte uma vez por trecho de linha, evitando um teste por pixel.
            const int first = px < 0 ? -px : 0;
            const int last = n < video_width - px ? n : video_width - px;
            for (int i = first; i < last; ++i) {
                uint16_t c = __builtin_bswap16(pixels[i]);
                dest[(px + i) ^ 2] = (uint8_t)(sync_bits | red_bits[c >> 11]
                    | green_bits[(c >> 5) & 63] | blue_bits[c & 31]);
            }
        }
        pixels += n;
        count -= n;
        win_pos += n;
    }
}
extern "C" void rg_robgo_input_init(void)
{
    if (input_ready) return;
    gpio_num_t kbdData, kbdClk, mouseData, mouseClk;
    load_robgo_ps2_pins(&kbdData, &kbdClk, &mouseData, &mouseClk);
    RG_LOGI("PS/2 init: KBD CLK=GPIO%d DATA=GPIO%d; MOUSE CLK=GPIO%d DATA=GPIO%d", (int)kbdClk, (int)kbdData, (int)mouseClk, (int)mouseData);
    RG_LOGI("PS/2 levels before init: KBD CLK=%d DATA=%d; MOUSE CLK=%d DATA=%d", gpio_get_level(kbdClk), gpio_get_level(kbdData), gpio_get_level(mouseClk), gpio_get_level(mouseData));
    PS2Controller::begin(kbdClk, kbdData, mouseClk, mouseData);
    RG_LOGI("PS/2 controller initialized=%d", PS2Controller::initialized() ? 1 : 0);
    PS2Controller::setKeyboard(&keyboard);
    PS2Controller::setMouse(&mouse);
    keyboard.begin(true,true,0);
    RG_LOGI("PS/2 keyboard begin: available=%d scancodes=%d", keyboard.isKeyboardAvailable() ? 1 : 0, keyboard.scancodeAvailable());
    RG_LOGI("PS/2 levels after init: KBD CLK=%d DATA=%d; MOUSE CLK=%d DATA=%d", gpio_get_level(kbdClk), gpio_get_level(kbdData), gpio_get_level(mouseClk), gpio_get_level(mouseData));
    // O BIOS do MSX interpreta SHIFT/CTRL. Evita transformar SHIFT+2 em '@' no host.
    physical_layout=USLayout;
    memset(physical_layout.alternateVK,0,sizeof(physical_layout.alternateVK));
    keyboard.setLayout(&physical_layout);
    mouse.begin(1);
    input_ready=true;
}
static bool down(VirtualKey key) { return input_ready && keyboard.isVKDown(key); }
extern "C" uint32_t rg_robgo_gamepad(void)
{
    if (input_ready) {
        VirtualKeyItem item;
        for (int n = 0; n < 32 && keyboard.getNextVirtualKey(&item, 0); ++n)
            RG_LOGV("PS/2 key event: vk=%d down=%d", (int)item.vk, item.down ? 1 : 0);
    }
    uint32_t result=0;
    if (down(VK_F12)) return RG_KEY_MENU;
    if (down(VK_F11)) return RG_KEY_OPTION;
    if (down(VK_F10)) return RG_KEY_START;
    if (down(VK_F9)) return RG_KEY_SELECT;
    if (down(VK_UP) || down(VK_KP_UP)) result|=RG_KEY_UP;
    if (down(VK_DOWN) || down(VK_KP_DOWN)) result|=RG_KEY_DOWN;
    if (down(VK_LEFT) || down(VK_KP_LEFT)) result|=RG_KEY_LEFT;
    if (down(VK_RIGHT) || down(VK_KP_RIGHT)) result|=RG_KEY_RIGHT;
    if (down(VK_RETURN) || down(VK_KP_ENTER) || down(VK_SPACE)) result|=RG_KEY_A;
    if (down(VK_ESCAPE) || down(VK_LCTRL) || down(VK_RCTRL)) result|=RG_KEY_B;
    return result;
}
extern "C" void rg_robgo_keys(uint8_t keys[256])
{
    memset(keys,0,256);
    for(int i=0;i<26;++i) keys['a'+i]=down(VirtualKey(VK_a+i)) || down(VirtualKey(VK_A+i));
    for(int i=0;i<10;++i) keys['0'+i]=down(VirtualKey(VK_0+i));
    static const uint8_t keypad_codes[]={23,24,25,26,28,29,30,31,128,129};
    for(int i=0;i<10;++i) keys[keypad_codes[i]]=down(VirtualKey(VK_KP_0+i));
    // Códigos KBD_* definidos pelo núcleo fMSX (MSX.h).
    static const struct { VirtualKey vk; uint8_t code; } map[]={
        {VK_LEFT,1},{VK_UP,2},{VK_RIGHT,3},{VK_DOWN,4},
        {VK_LSHIFT,5},{VK_RSHIFT,5},{VK_LCTRL,6},{VK_RCTRL,6},
        {VK_LALT,7},{VK_BACKSPACE,8},{VK_TAB,9},{VK_CAPSLOCK,10},
        {VK_END,11},{VK_HOME,12},{VK_RETURN,13},{VK_KP_ENTER,13},
        {VK_DELETE,14},{VK_INSERT,15},{VK_RALT,16},{VK_PAUSE,17},
        {VK_F1,18},{VK_F2,19},{VK_F3,20},{VK_F4,21},{VK_F5,22},
        {VK_ESCAPE,27},{VK_SPACE,32},{VK_MINUS,'-'},{VK_EQUALS,'='},
        {VK_LEFTBRACKET,'['},{VK_RIGHTBRACKET,']'},{VK_BACKSLASH,92},
        {VK_SEMICOLON,';'},{VK_QUOTE,39},{VK_GRAVEACCENT,'`'},
        {VK_COMMA,','},{VK_PERIOD,'.'},{VK_SLASH,'/'},
    };
    for (const auto &entry:map) keys[entry.code]|=down(entry.vk);
}
extern "C" uint32_t rg_robgo_mouse(void)
{
    static uint8_t x=128,y=128;
    static uint32_t buttons=0;
    if (!input_ready) return 0;
    MouseDelta delta;
    while(mouse.getNextDelta(&delta,0)) {
        x+=delta.deltaX; y-=delta.deltaY;
        buttons=(delta.buttons.left?1u<<16:0)|(delta.buttons.right?1u<<17:0);
    }
    return x|(uint32_t(y)<<8)|buttons;
}


