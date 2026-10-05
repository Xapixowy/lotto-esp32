#pragma once

#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <XPT2046_Touchscreen.h>
#include "Hardware.h"
#include "Controller.h"

class Display {
public:
    Display() : lcd_(&lcdBus_, LCD_DC, LCD_CS, LCD_RST), touch_(TOUCH_CS, TOUCH_IRQ) {}

    void begin() {
        pinMode(LCD_BACKLIGHT, OUTPUT);
        digitalWrite(LCD_BACKLIGHT, HIGH);
        lcdBus_.begin(LCD_SCLK, LCD_MISO, LCD_MOSI, LCD_CS);
#if DISPLAY_ST7789
        lcd_.init(240, 320);
#else
        lcd_.begin(40000000);
#endif
        lcd_.setRotation(DISPLAY_ROTATION);
        lcd_.invertDisplay(DISPLAY_INVERT);
        text_.begin(lcd_);
        text_.setFontMode(1);
        text_.setFontDirection(0);
        touchBus_.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
        touch_.begin(touchBus_);
        touch_.setRotation(DISPLAY_ROTATION);
    }

    bool readTouch(lotto::Touch& action, bool diagnostic) {
        const bool touching = touch_.touched();
        if (!touching) { pressed_ = false; return false; }
        if (pressed_) return false;
        pressed_ = true;
        auto point = touch_.getPoint();
        if (diagnostic) Serial.printf("Touch raw x=%d y=%d pressure=%d\n", point.x, point.y, point.z);
        int rawX = TOUCH_SWAP_XY ? point.y : point.x;
        int rawY = TOUCH_SWAP_XY ? point.x : point.y;
        int x = constrain(map(rawX, TOUCH_X_MIN, TOUCH_X_MAX, 0, 319), 0, 319);
        int y = constrain(map(rawY, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 239), 0, 239);
        if (TOUCH_FLIP_X) x = 319 - x;
        if (TOUCH_FLIP_Y) y = 239 - y;
        if (diagnostic) Serial.printf("Touch screen x=%d y=%d\n", x, y);
        if (y < 192 || x < 160) return false;
        if (x < 208) action = lotto::Touch::Previous;
        else if (x >= 212 && x < 260) action = lotto::Touch::Next;
        else if (x >= 264) action = lotto::Touch::Lock;
        else return false;
        return true;
    }

    void draw(const lotto::View& view) {
        if (drawn_ && sameContent(view, previous_)) {
            if (view.refreshing != previous_.refreshing) drawRefreshing(view.refreshing);
            previous_ = view;
            return;
        }
        lcd_.fillScreen(BACKGROUND);
        if (view.status != lotto::Status::Ready) drawError(view.status);
        else {
            drawRefreshing(view.refreshing);
            print(10, 45, view.title.c_str(), u8g2_font_helvB18_te, FOREGROUND);
            print(275, 43, (std::to_string(view.page + 1) + "/" + std::to_string(view.pages)).c_str());
            print(10, 69, view.groupLabel.c_str());
            for (std::size_t i = 0; i < view.values.size(); ++i) {
                auto value = std::to_string(view.values[i]);
                text_.setFont(u8g2_font_helvB24_tn);
                auto width = text_.getUTF8Width(value.c_str());
                print(10 + static_cast<int>(i % 4) * 76 + (70 - width) / 2,
                      107 + static_cast<int>(i / 4) * 36, value.c_str(), u8g2_font_helvB24_tn,
                      view.kind == "additional" ? ACCENT : FOREGROUND);
            }
            lcd_.drawFastHLine(8, 190, 304, SECONDARY);
            print(8, 209, ("Lotto: " + view.lottoTime).c_str());
            print(8, 231, ("Sync:  " + view.syncTime).c_str());
            drawControls(view.locked);
        }
        previous_ = view;
        drawn_ = true;
    }

private:
    static constexpr uint16_t BACKGROUND = 0x0841;
    static constexpr uint16_t FOREGROUND = 0xffff;
    static constexpr uint16_t SECONDARY = 0xc638;
    static constexpr uint16_t ACCENT = 0xff27;
    static constexpr uint16_t BUTTON = 0x2124;
    SPIClass lcdBus_{HSPI};
    SPIClass touchBus_{VSPI};
#if DISPLAY_ST7789
    Adafruit_ST7789 lcd_;
#else
    Adafruit_ILI9341 lcd_;
#endif
    XPT2046_Touchscreen touch_;
    U8G2_FOR_ADAFRUIT_GFX text_;
    lotto::View previous_;
    bool drawn_ = false;
    bool pressed_ = false;

    static bool sameContent(const lotto::View& a, const lotto::View& b) {
        return a.status == b.status && a.title == b.title && a.groupLabel == b.groupLabel &&
            a.kind == b.kind && a.values == b.values && a.lottoTime == b.lottoTime &&
            a.syncTime == b.syncTime && a.page == b.page && a.pages == b.pages && a.locked == b.locked;
    }

    void print(int x, int y, const char* value, const uint8_t* font = u8g2_font_unifont_t_polish,
               uint16_t color = FOREGROUND) {
        text_.setFont(font);
        text_.setForegroundColor(color);
        text_.setCursor(x, y);
        text_.print(value);
    }

    void drawRefreshing(bool refreshing) {
        lcd_.fillRect(0, 0, 320, 21, BACKGROUND);
        if (refreshing) print(10, 16, "Odświeżanie...", u8g2_font_unifont_t_polish, SECONDARY);
    }

    void drawControls(bool locked) {
        lcd_.fillRoundRect(160, 192, 48, 48, 5, BUTTON);
        lcd_.fillRoundRect(212, 192, 48, 48, 5, BUTTON);
        lcd_.fillRoundRect(264, 192, 56, 48, 5, locked ? ACCENT : BUTTON);
        lcd_.fillTriangle(173, 216, 189, 204, 189, 228, FOREGROUND);
        lcd_.fillTriangle(247, 216, 231, 204, 231, 228, FOREGROUND);
        auto ink = locked ? BACKGROUND : FOREGROUND;
        lcd_.fillRoundRect(282, 213, 20, 17, 2, ink);
        lcd_.drawRoundRect(285, 200, 14, 20, 6, ink);
        lcd_.drawRoundRect(286, 201, 12, 19, 5, ink);
        if (!locked) lcd_.fillRect(284, 207, 5, 7, BUTTON);
    }

    void drawError(lotto::Status status) {
        const char* title = "Błąd danych";
        const char* message = "Backend zwrócił nieprawidłowe dane. Sprawdź wersję API.";
        switch (status) {
            case lotto::Status::Fetching:
                title = "Pobieranie wyników";
                message = "Czekam na dane z backendu. Spróbuję ponownie automatycznie."; break;
            case lotto::Status::WifiDisconnected:
                title = "Brak Wi-Fi";
                message = "Nie mogę połączyć się z siecią. Sprawdź router i ustawienia Wi-Fi."; break;
            case lotto::Status::BackendUnreachable:
                title = "Brak połączenia";
                message = "Nie mogę połączyć się z API. Sprawdź internet, adres API i certyfikat."; break;
            case lotto::Status::AccessDenied:
                title = "Brak dostępu";
                message = "API odrzuciło token. Sprawdź token urządzenia w konfiguracji."; break;
            case lotto::Status::BackendUnavailable:
                title = "API niedostępne";
                message = "Backend nie może teraz zwrócić wyników. Ponawiam połączenie co 15 s."; break;
            case lotto::Status::LottoFailed:
                title = "Błąd Lotto API";
                message = "Backend nie pobrał wyników z Lotto. Kolejna próba co 4 minuty."; break;
            case lotto::Status::Stale:
                title = "Nieaktualne dane";
                message = "Wyniki nie zostały odświeżone od ponad 8 minut. Czekam na backend."; break;
            default: break;
        }
        print(10, 38, title, u8g2_font_helvB18_te, ACCENT);
        std::string line;
        std::string word;
        int y = 80;
        auto flushWord = [&]() {
            std::string candidate = line.empty() ? word : line + " " + word;
            text_.setFont(u8g2_font_unifont_t_polish);
            if (text_.getUTF8Width(candidate.c_str()) > 296 && !line.empty()) {
                print(12, y, line.c_str()); y += 25; line = word;
            } else line = candidate;
            word.clear();
        };
        for (const char* p = message; *p; ++p) {
            if (*p == ' ') flushWord();
            else word += *p;
        }
        flushWord();
        if (!line.empty()) print(12, y, line.c_str());
        print(12, 229, "Ponawiam automatycznie", u8g2_font_unifont_t_polish, SECONDARY);
    }
};
