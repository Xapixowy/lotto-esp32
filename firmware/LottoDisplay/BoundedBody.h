#pragma once

#include <Arduino.h>
#include <string>

class BoundedBody : public Stream {
public:
    std::string body;
    bool overflow = false;
    unsigned long startedAt = millis();

    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t* data, size_t length) override {
        if (body.size() + length > 32768 || millis() - startedAt > 10000) { overflow = true; return 0; }
        body.append(reinterpret_cast<const char*>(data), length);
        return length;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
