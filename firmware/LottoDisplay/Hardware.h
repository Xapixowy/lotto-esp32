#pragma once

// Standard ESP32-2432S028R profile. Confirm using display-check mode before use.
// Some 2.8-inch variants use ST7789; set this to 1 for those panels.
#define DISPLAY_ST7789 0
#define DISPLAY_ROTATION 1
#define DISPLAY_INVERT false

constexpr int LCD_SCLK = 14;
constexpr int LCD_MISO = 12;
constexpr int LCD_MOSI = 13;
constexpr int LCD_CS = 15;
constexpr int LCD_DC = 2;
constexpr int LCD_RST = -1;
constexpr int LCD_BACKLIGHT = 21;
constexpr int TOUCH_SCLK = 25;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_CS = 33;
constexpr int TOUCH_IRQ = 36;

// Raw resistive-touch calibration; use Serial Monitor in display-check mode
// and tune these bounds to match the left/right/top/bottom edges of your panel.
constexpr int TOUCH_X_MIN = 200;
constexpr int TOUCH_X_MAX = 3900;
constexpr int TOUCH_Y_MIN = 200;
constexpr int TOUCH_Y_MAX = 3900;
constexpr bool TOUCH_SWAP_XY = false;
constexpr bool TOUCH_FLIP_X = false;
constexpr bool TOUCH_FLIP_Y = false;
