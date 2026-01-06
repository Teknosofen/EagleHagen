// Button.hpp
// Interrupt-driven button handler with debounce and long-press detection

#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <Arduino.h>

class Button {
public:
    Button(uint8_t pin, unsigned long longPressMs = 1000, unsigned long debounceMs = 50);
    
    void begin();
    void update();
    
    bool wasPressed();
    bool wasReleased();
    bool wasLongPress();
    
private:
    void IRAM_ATTR handleInterrupt();
    
    uint8_t _pin;
    unsigned long _longPressMs;
    unsigned long _debounceMs;
    
    volatile bool _pressedFlag = false;
    volatile bool _releasedFlag = false;
    volatile bool _longPressFlag = false;
    volatile unsigned long _lastInterruptTime = 0;
    volatile unsigned long _pressStartTime = 0;
    volatile unsigned long _pressDuration = 0;
};

#endif // BUTTON_HPP
