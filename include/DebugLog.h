// DebugLog.h
// Diagnostic text output that shares the USB CDC port with the host data stream.
//
// The legacy LabVIEW program reads fixed 24-byte frames, so any stray text on the
// port corrupts its framing. Diagnostics are therefore:
//  - muted while the host output is in Legacy LabVIEW format
//  - prefixed with "# " on every line in tab-separated format, so parsers can skip them
//
// DataLogger switches this automatically when the output format changes.
// Build with -DDEBUG_LOG_ALWAYS=1 to force diagnostics on (e.g. to see the boot log).

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <Arduino.h>

class DebugLogger : public Print {
public:
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buffer, size_t size) override;

private:
    bool _enabled = false;      // DataLogger starts in Legacy LabVIEW format
    bool _atLineStart = true;
};

extern DebugLogger DebugOut;

#endif // DEBUG_LOG_H
