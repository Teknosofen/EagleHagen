# CO2 Analyzer Serial Interface Documentation

## CO2 Sensor Information

**Device:** Medair MaCO2-V3 OEM Module  
**Manufacturer:** MedAir AB (Delsbo, Sweden)  
**Technology:** MedAir EtCO2 capnography technology  
**Interface:** UART/RS232

**Note:** MedAir AB was acquired by Nonin Medical, Inc. in 2006. The MedAir EtCO2 technology continues to be used in Nonin's capnography products (RespSense, LifeSense series). Unfortunately, specific technical documentation for the legacy MaCO2-V3 OEM module is not readily available online. This documentation has been reverse-engineered from the PIC16F876A interface firmware.

## Overview
This document describes the serial interface protocol for a Medair MaCO2-V3 CO2 analyzer OEM module connected through a PIC16F876A microcontroller. The PIC acts as an intermediary, receiving data from the CO2 analyzer and forwarding it along with two additional analog measurements to a host system.

## System Architecture

### Complete Monitoring System

The complete system consists of three components:

```
[MedAir MaCO2-V3] ←→ [PIC16F876A Interface] → [Host Computer (LabVIEW)]
     (CO2 Sensor)      (Data Aggregator)        (Display/Control)
```

**Component Functions:**

1. **MedAir MaCO2-V3 CO2 Analyzer:**
   - Measures EtCO2, FiCO2, respiratory rate using NDIR spectroscopy
   - Contains internal pump for sidestream sampling
   - Sends 8-byte data packets at regular intervals
   - Receives commands (0xA5, 0x5A) for pump control and calibration

2. **PIC16F876A Microcontroller:**
   - Receives CO2 data via RS232/UART (9600 baud)
   - Samples two analog inputs (AN0, AN1) for O2 and volume measurements
   - Combines CO2 data + ADC readings into unified output packet
   - Forwards enhanced data to host computer
   - Acts as transparent pass-through with data augmentation

3. **Host Computer (LabVIEW Application):**
   - Receives combined data stream
   - Parses and displays CO2 waveforms, O2, volume
   - Monitors status flags (data valid, pump, leak, occlusion)
   - Sends control commands back to CO2 analyzer
   - Provides user interface for calibration and pump control

### Analog Input Purposes

Based on the LabVIEW parser labels:

- **AN0 (Analog Input 0):** **O2 Measurement**
  - 10-bit ADC with Vref+ reference
  - Scaled to 0-65535 (left-justified)
  - Likely connected to oxygen sensor for O2 concentration

- **AN1 (Analog Input 1):** **VOL (Volume) Measurement**  
  - 10-bit ADC without external reference
  - Range: 0-1023 (right-justified)
  - Displayed as "Vol in ADC_Counts" and "DC_counts/liter"
  - Possibly flow or tidal volume sensor

## Hardware Configuration

### Microcontroller
- **Device:** PIC16F876A
- **Clock Frequency:** 20 MHz
- **Serial Pins:** 
  - TX: RC6 (output)
  - RX: RC7 (input)

### Analog Inputs
- **AN0 (RA0):** Analog input with Vref+ reference
  - Left-justified 10-bit ADC
  - Range: 0-65535 (scaled)
  - Uses internal reference (Vref+ = AN3, Vref- = Vss)
  
- **AN1 (RA1):** Analog input without external reference
  - Right-justified 10-bit ADC
  - Range: 0-1023
  - Uses Vdd/Vss as reference

## Serial Communication Settings

| Parameter | Value |
|-----------|-------|
| Baud Rate | 9600 bps |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 (implied) |
| Flow Control | None |
| Mode | Asynchronous |

## Communication Protocol

### Bidirectional Communication

Unlike the PIC firmware which only receives data from the CO2 analyzer, the complete system supports bidirectional communication:

**Commands from Host to CO2 Analyzer:**
- **0xA5**: Start pump command
- **0x5A**: Zero calibration command
- **0xF1**: Restart pump (from LabVIEW diagram - may be host-side only)

These commands control the CO2 analyzer's internal pump and calibration functions.

### Initialization Sequence

1. **Power-up/Reset**
   - PIC initializes serial port
   - Performs overrun recovery
   - Waits for start byte from CO2 analyzer

2. **Handshake**
   ```
   CO2 Analyzer → PIC: 0x06 (start byte)
   PIC → Host:          0x1B (ESC - acknowledgment)
   CO2 Analyzer → PIC: 7 bytes (initialization data, discarded)
   ```

3. **Normal Operation Begins**
   - PIC enters main loop
   - Continuous data polling starts

### Data Frame Format

#### Input from CO2 Analyzer (8 bytes per cycle)

| Byte | Name | Description |
|------|------|-------------|
| d[0] | Status 1 | Status byte 1 |
| d[1] | Status 2 | Status byte 2 |
| d[2] | RR | Respiratory Rate |
| d[3] | - | Unused/reserved |
| d[4] | FetCO2 | End-tidal CO2 (waveform value) |
| d[5] | FiCO2 | Inspired CO2 |
| d[6] | - | Unused/reserved |
| d[7] | - | Unused/reserved |

#### Output to Host (per cycle)

The PIC sends a tab-delimited ASCII string followed by binary data:

```
<ESC><FetCO2_ASCII><TAB><AN0_ADC><TAB><AN1_ADC><TAB><Status1><Status2><RR><FetCO2><FiCO2><CR><LF>
```

**Field Descriptions:**

1. **ESC (0x1B)** - Start marker
2. **FetCO2_ASCII** - 3-digit ASCII representation of d[4] (000-255)
3. **TAB (0x09)** - Field separator
4. **AN0_ADC** - 5-digit ASCII representation of ADC reading (00000-65535)
5. **TAB (0x09)** - Field separator
6. **AN1_ADC** - 5-digit ASCII representation of ADC reading (00000-01023)
7. **TAB (0x09)** - Field separator
8. **Status1** - Raw binary byte (d[0])
9. **Status2** - Raw binary byte (d[1], replaced with 128 if zero)
10. **RR** - Raw binary byte (d[2], replaced with 255 if zero)
11. **FetCO2** - Raw binary byte (d[4], replaced with 255 if zero)
12. **FiCO2** - Raw binary byte (d[5], replaced with 255 if zero)
13. **CR (0x0D)** - Carriage return
14. **LF (0x0A)** - Line feed

### Example Output Frame

**ASCII representation (with binary shown as hex):**
```
<ESC>045<TAB>32768<TAB>00512<TAB>[0x20][0x80][0x0F][0x2D][0x05]<CR><LF>
```

**Interpretation:**
- FetCO2 waveform value: 45
- AN0 ADC reading: 32768
- AN1 ADC reading: 512
- Status1: 0x20
- Status2: 0x80 (128)
- Respiratory Rate: 0x0F (15)
- FetCO2: 0x2D (45)
- FiCO2: 0x05 (5)

## Data Processing Notes

### From LabVIEW Parser Analysis

The LabVIEW block diagram reveals additional protocol details:

**Complete Data String Format:**
```
$veptid = <ESC>ABC<1>DEFGH<1>IJKLM<1><0..n> 24 bytes.
Where:
- BCG = CO2 waveform value
- DEFGH = O2 measurement (5 digits from AN0 ADC)
- IJKLM = VOL measurement (5 digits from AN1 ADC, likely volume/flow)
- O = status 1, P = status 1 (possibly two separate status bytes)
- Q = RR (respiratory rate as integer)
- R = FiCO2 (inspired CO2 as integer)
- S = FetCO2 (end-tidal CO2 as integer)
```

**Data Processing:**

1. **CO2 in mmHg:**
   - Raw CO2 value is converted to mmHg with a gain factor
   - Displayed on continuous waveform chart

2. **Volume in ADC counts/liter:**
   - VOL parameter (AN1 ADC) used for volume/flow calculations
   - Displayed as DC_counts/liter

3. **Data Validity Checks:**
   - Status1 contains "Data Valid" flag
   - Zero CO2 detection (CO2 < F2 threshold)
   - Continuous monitoring of pump, leak, and occlusion status

4. **Calibration:**
   - Zero calibration command: 0x5A
   - Pump start command: 0xA5

### Zero Value Replacement
To prevent null bytes in the data stream (which could be interpreted as string terminators), the firmware replaces zero values:

- **Status2 (d[1]):** 0 → 128 (0x80)
- **RR (d[2]):** 0 → 255 (0xFF)
- **FetCO2 (d[4]):** 0 → 255 (0xFF)
- **FiCO2 (d[5]):** 0 → 255 (0xFF)

**Important:** When parsing, values of 128 (for Status2) or 255 (for RR, FetCO2, FiCO2) may represent actual zeros from the CO2 analyzer.

### Timing
- The system operates in a continuous polling loop
- No explicit timing delays between frames
- Frame rate depends on CO2 analyzer transmission speed
- ADC conversion time: ~20 μs acquisition + conversion time per channel

## Error Handling

### Serial Overrun Recovery
The firmware includes an overrun recovery function that:
1. Reads and discards two characters from the receive buffer
2. Disables continuous receive (CREN = 0)
3. Re-enables continuous receive (CREN = 1)

This is called once during initialization to clear any startup glitches.

### Transmit Protection
The `putchar()` function refuses to send null bytes (0x00) to prevent premature string termination.

## Parsing the Output

### Recommended Parsing Strategy

```python
# Pseudocode for parsing the output frame
def parse_co2_frame(data):
    if data[0] != 0x1B:  # Check for ESC
        return None
    
    # Split on tabs to get ASCII fields
    parts = data[1:].split(b'\t')
    
    # Parse ASCII values
    fetco2_ascii = int(parts[0])  # 3 digits
    an0_adc = int(parts[1])       # 5 digits  
    an1_adc = int(parts[2])       # 5 digits
    
    # Binary data follows last tab
    binary_data = parts[3]
    status1 = binary_data[0]
    status2 = binary_data[1] if binary_data[1] != 128 else 0
    rr = binary_data[2] if binary_data[2] != 255 else 0
    fetco2 = binary_data[3] if binary_data[3] != 255 else 0
    fico2 = binary_data[4] if binary_data[4] != 255 else 0
    
    return {
        'fetco2_waveform': fetco2_ascii,
        'an0': an0_adc,
        'an1': an1_adc,
        'status1': status1,
        'status2': status2,
        'respiratory_rate': rr,
        'end_tidal_co2': fetco2,
        'inspired_co2': fico2
    }
```

## CO2 Analyzer Data Fields

### Status Bytes (Decoded from LabVIEW Parser)

#### Status Byte 1 (d[0])
Based on the LabVIEW implementation, Status1 appears to be a general status/validity indicator:
- **Data Valid** flag - indicates if the CO2 measurement data is valid
- May include additional status flags (exact bit mapping not visible in diagram)

#### Status Byte 2 (d[1]) - System Status Flags

| Bit | Function | Value = 0 | Value = 1 |
|-----|----------|-----------|-----------|
| 0 | Pump Status | Pump Stopped | Pump Running |
| 1 | Leak Detection | No Leak | Leak Detected |
| 2 | Occlusion | No Occlusion | Occlusion Detected |
| 3-7 | Reserved | Unknown | Unknown |

**Critical Conditions:**
- **Pump stopped with leak/occlusion:** Status2 = 0b00000110 (bit 0=0, bit 1=1, bit 2=1)
- The firmware replaces Status2 = 0 with 128 (0x80), so a received value of 128 may indicate Status2 was originally 0

**Zero CO2 Detection:**
- LabVIEW checks if CO2 value < F2 threshold
- Indicates possible: no patient breathing, disconnected cannula, or system malfunction

### Command Interface (Host to MaCO2-V3)

The LabVIEW diagram reveals a bidirectional protocol:

**Command Byte Format:** `Command A5,hex = start pump, 5A,hex Zero calibrate`

- **0xA5**: Start pump command
- **0x5A**: Zero calibration command

These commands are sent from the host to the CO2 analyzer to control pump operation and initiate calibration.

### Respiratory Rate (RR)
- Likely measured in breaths per minute
- Typical range: 8-30 for adults

### End-Tidal CO2 (FetCO2)
- Peak CO2 concentration at end of expiration
- Typically measured in mmHg or %
- Normal range: 35-45 mmHg (or 4.5-6.0%)

### Inspired CO2 (FiCO2)
- CO2 concentration during inspiration
- Should be near zero in normal conditions
- Elevated values may indicate rebreathing

## Integration Notes

### Connecting to Host System
- Use a standard RS-232 serial connection
- May require level shifters (TTL to RS-232) if PIC outputs are used directly
- Ensure proper ground connection between devices

### Synchronization
- Wait for ESC (0x1B) byte to synchronize frame start
- Each frame ends with CR+LF
- No explicit frame number or checksum provided

### Troubleshooting
1. **No data received:** Check baud rate (9600), verify CO2 analyzer is sending data
2. **Garbled data:** Verify baud rate, check for ground loops
3. **Missing frames:** Check for serial overrun conditions
4. **Incorrect ADC values:** Verify analog input connections and reference voltages
5. **Pump issues:** 
   - Check Status2 bit 0 for pump state
   - Send 0xA5 command to restart pump
   - Monitor for leak (Status2 bit 1) and occlusion (Status2 bit 2) flags
6. **Zero CO2 readings:**
   - Verify data valid flag in Status1
   - Check for occlusion in sample line
   - Consider sending 0x5A calibration command
7. **Invalid readings:**
   - Monitor Status1 for data validity
   - Check for moisture trap saturation
   - Verify proper cannula/sample line connection

## Limitations and Considerations

1. **No Error Detection:** No checksum or CRC on data frames
2. **Zero Ambiguity:** Cannot distinguish true zeros from replaced values
3. **Fixed Baud Rate:** Hardcoded to 9600 bps
4. **No Flow Control:** May lose data if host cannot keep up
5. **Binary/ASCII Mix:** Makes parsing more complex than pure ASCII or binary protocols

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2024 | Initial documentation from legacy code analysis |
| 1.1 | 2024 | Added manufacturer information (MedAir AB/Nonin Medical) |

---

## Additional Resources

### MedAir/Nonin Information
- **MedAir AB** was a Swedish company (Delsbo, Sweden) specializing in capnography and pulse oximetry
- Acquired by **Nonin Medical, Inc.** in 2006
- MedAir EtCO2 technology now used in Nonin products:
  - RespSense capnography monitors
  - LifeSense multi-parameter monitors
  - CAP201 OEM capnography module (current product)

### Contact Information
For modern Nonin capnography OEM modules:
- Email: oem@nonin.com (for OEM sales)
- Email: info@nonin.com (for technical support)
- Website: https://www.nonin.com

### Related Technologies
The MaCO2-V3 uses sidestream capnography with:
- Non-dispersive infrared (NDIR) spectroscopy for CO2 measurement
- Sample pump to draw exhaled breath through tubing
- Moisture management/water trap system
- Typical sampling rate: ~50 ml/min (based on similar MedAir products)

---

**Note:** This documentation was reverse-engineered from PIC16F876A firmware code. The original MedAir MaCO2-V3 manufacturer's specifications and datasheets are not available online. Some field interpretations are based on common medical capnography practices and knowledge of similar MedAir/Nonin products.
