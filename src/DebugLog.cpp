// DebugLog.cpp
// Implementation of gated, line-prefixed diagnostic output

#include "DebugLog.h"

DebugLogger DebugOut;

void DebugLogger::setEnabled(bool enabled) {
#ifdef DEBUG_LOG_ALWAYS
    enabled = true;
#endif
    if (enabled && !_enabled) {
        _atLineStart = true;
    }
    _enabled = enabled;
}

size_t DebugLogger::write(uint8_t c) {
#ifndef DEBUG_LOG_ALWAYS
    if (!_enabled) {
        return 1;  // Swallow silently so callers see success
    }
#endif
    // Mark every diagnostic line with "# " (unless it already starts with '#')
    if (_atLineStart && c != '\r' && c != '\n' && c != '#') {
        Serial.print("# ");
    }
    _atLineStart = (c == '\n');
    return Serial.write(c);
}

size_t DebugLogger::write(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        write(buffer[i]);
    }
    return size;
}
