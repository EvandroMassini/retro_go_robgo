// hal_input.cpp - teclado e mouse PS/2 pela FabGL
#include <Arduino.h>
#include <string.h>
#include "fabgl.h"
#include "hal.h"
#include "board.h"

using namespace fabgl;

static Keyboard keyboard;
static Mouse mouse;
static KeyboardLayout physical_layout;
static bool input_ready;

// Perfil de pinos vindo do /sd/bootl.rc (kbddat/kbdclk). Só aceita 32/33 ou 27/26.
static bool keyboard_on_port_b(void)
{
    char v[16];
    int data = PIN_PS2_A_DATA, clk = PIN_PS2_A_CLK;
    bool have_data = false, have_clk = false;
    if (hal_rc_get("kbddat", v, sizeof(v))) { data = atoi(v); have_data = true; }
    if (hal_rc_get("kbdclk", v, sizeof(v))) { clk = atoi(v); have_clk = true; }
    if (have_data && have_clk && data == PIN_PS2_B_DATA && clk == PIN_PS2_B_CLK)
        return true;
    if (have_data || have_clk) {
        if (!(have_data && have_clk && data == PIN_PS2_A_DATA && clk == PIN_PS2_A_CLK))
            hal_log("bootl.rc: perfil PS/2 invalido, usando DATA32 CLK33\n");
    }
    return false;
}

extern "C" void hal_input_init(void)
{
    if (input_ready) return;
    gpio_num_t kbdData, kbdClk, mouseData, mouseClk;
    if (keyboard_on_port_b()) {
        kbdData = (gpio_num_t)PIN_PS2_B_DATA;   kbdClk = (gpio_num_t)PIN_PS2_B_CLK;
        mouseData = (gpio_num_t)PIN_PS2_A_DATA; mouseClk = (gpio_num_t)PIN_PS2_A_CLK;
    } else {
        kbdData = (gpio_num_t)PIN_PS2_A_DATA;   kbdClk = (gpio_num_t)PIN_PS2_A_CLK;
        mouseData = (gpio_num_t)PIN_PS2_B_DATA; mouseClk = (gpio_num_t)PIN_PS2_B_CLK;
    }
    hal_log("PS/2: teclado DATA=%d CLK=%d, mouse DATA=%d CLK=%d\n",
            (int)kbdData, (int)kbdClk, (int)mouseData, (int)mouseClk);

    PS2Controller::begin(kbdClk, kbdData, mouseClk, mouseData);
    PS2Controller::setKeyboard(&keyboard);
    PS2Controller::setMouse(&mouse);
    keyboard.begin(true, true, 0);

    // O BIOS do MSX interpreta SHIFT/CTRL: evita que o host transforme SHIFT+2 em '@'.
    physical_layout = USLayout;
    memset(physical_layout.alternateVK, 0, sizeof(physical_layout.alternateVK));
    keyboard.setLayout(&physical_layout);
    mouse.begin(1);
    input_ready = true;
}

static bool down(VirtualKey key) { return input_ready && keyboard.isVKDown(key); }

// Esvazia a fila de eventos (o estado das teclas já é mantido pela FabGL).
static void drain_events(void)
{
    VirtualKeyItem item;
    for (int n = 0; n < 32 && keyboard.getNextVirtualKey(&item, 0); ++n) {}
}

extern "C" uint32_t hal_input_nav(void)
{
    if (!input_ready) return 0;
    drain_events();
    uint32_t r = 0;
    if (down(VK_F10)) r |= HAL_HOT_MENU;
    if (down(VK_F11)) r |= HAL_HOT_SKIP;
    if (down(VK_F12)) r |= HAL_HOT_RESET;
    if (down(VK_UP)    || down(VK_KP_UP))    r |= HAL_NAV_UP;
    if (down(VK_DOWN)  || down(VK_KP_DOWN))  r |= HAL_NAV_DOWN;
    if (down(VK_LEFT)  || down(VK_KP_LEFT))  r |= HAL_NAV_LEFT;
    if (down(VK_RIGHT) || down(VK_KP_RIGHT)) r |= HAL_NAV_RIGHT;
    if (down(VK_RETURN) || down(VK_KP_ENTER) || down(VK_SPACE)) r |= HAL_NAV_A;
    if (down(VK_ESCAPE) || down(VK_LCTRL) || down(VK_RCTRL))    r |= HAL_NAV_B;
    return r;
}

// Preenche keys[] com os códigos KBD_* do fMSX (MSX.h) das teclas apertadas.
extern "C" void hal_input_keys(uint8_t keys[256])
{
    memset(keys, 0, 256);
    if (!input_ready) return;
    for (int i = 0; i < 26; ++i)
        keys['a' + i] = down(VirtualKey(VK_a + i)) || down(VirtualKey(VK_A + i));
    for (int i = 0; i < 10; ++i)
        keys['0' + i] = down(VirtualKey(VK_0 + i));
    static const uint8_t keypad_codes[] = {23, 24, 25, 26, 28, 29, 30, 31, 128, 129};
    for (int i = 0; i < 10; ++i)
        keys[keypad_codes[i]] = down(VirtualKey(VK_KP_0 + i));
    static const struct { VirtualKey vk; uint8_t code; } map[] = {
        {VK_LEFT, 1}, {VK_UP, 2}, {VK_RIGHT, 3}, {VK_DOWN, 4},
        {VK_LSHIFT, 5}, {VK_RSHIFT, 5}, {VK_LCTRL, 6}, {VK_RCTRL, 6},
        {VK_LALT, 7}, {VK_BACKSPACE, 8}, {VK_TAB, 9}, {VK_CAPSLOCK, 10},
        {VK_END, 11}, {VK_HOME, 12}, {VK_RETURN, 13}, {VK_KP_ENTER, 13},
        {VK_DELETE, 14}, {VK_INSERT, 15}, {VK_RALT, 16}, {VK_PAUSE, 17},
        {VK_F1, 18}, {VK_F2, 19}, {VK_F3, 20}, {VK_F4, 21}, {VK_F5, 22},
        {VK_ESCAPE, 27}, {VK_SPACE, 32}, {VK_MINUS, '-'}, {VK_EQUALS, '='},
        {VK_LEFTBRACKET, '['}, {VK_RIGHTBRACKET, ']'}, {VK_BACKSLASH, 92},
        {VK_SEMICOLON, ';'}, {VK_QUOTE, 39}, {VK_GRAVEACCENT, '`'},
        {VK_COMMA, ','}, {VK_PERIOD, '.'}, {VK_SLASH, '/'},
    };
    for (const auto &entry : map)
        keys[entry.code] |= down(entry.vk);
}

// Posição acumulada (0..255) nos 16 bits baixos; botões esquerdo/direito nos bits 16 e 17.
extern "C" uint32_t hal_input_mouse(void)
{
    static uint8_t x = 128, y = 128;
    static uint32_t buttons = 0;
    if (!input_ready) return 0;
    MouseDelta delta;
    while (mouse.getNextDelta(&delta, 0)) {
        x += delta.deltaX;
        y -= delta.deltaY;
        buttons = (delta.buttons.left ? 1u << 16 : 0) | (delta.buttons.right ? 1u << 17 : 0);
    }
    return x | (uint32_t(y) << 8) | buttons;
}
