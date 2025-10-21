# aurora-go-esp32 (ESP-IDF)

Complete ESP32 port of aurora-go with HTTP endpoints, robust TCP querying, and full Aurora inverter data parsing.

## Requirements
- ESP-IDF v5.x installed and exported (`. $IDF_PATH/export.sh`)
- An ESP32/ESP32-S3 board
- Network connectivity to Aurora inverter (TCP port 1470)

## Configure Wi‑Fi and inverter
Use menuconfig to set Wi‑Fi credentials and inverter address:

```bash
idf.py menuconfig
```

Under "Aurora-Go ESP32 Configuration" set:
- Wi‑Fi SSID / Password
- Inverter IP (default `192.168.0.190`)
- Inverter TCP port (default `1470`)

## Build & Flash
```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## HTTP Endpoints

### `/` - Plain text summary
Returns human-readable inverter status:
```
State: OK
model: Aurora 3.6 kW indoor
serial: ABC123
globalState: Run
inverterState: MPPT
alarm: No Alarm
totalKWh: 1234.56
todayKWh: 12.34
powerOutW: 2500.00
powerInW: 2600.00
```

### `/json/` - JSON data
Returns structured JSON with all inverter data:
```json
{
  "state": "OK",
  "versionRaw": "023A00000000",
  "model": "Aurora 3.6 kW indoor",
  "serial": "ABC123",
  "globalStateRaw": 5,
  "inverterStateRaw": 44,
  "alarmRaw": 0,
  "global": "Run",
  "inverter": "MPPT",
  "alarm": "No Alarm",
  "alarmCode": "",
  "totalKWh": 1234.56,
  "todayKWh": 12.34,
  "powerOutW": 2500.00,
  "powerInW": 2600.00
}
```

### `/xml/` - XML data
Returns XML structure with all inverter data:
```xml
<inverter>
  <state>OK</state>
  <versionRaw>023A00000000</versionRaw>
  <model>Aurora 3.6 kW indoor</model>
  <serial>ABC123</serial>
  <globalStateRaw>5</globalStateRaw>
  <inverterStateRaw>44</inverterStateRaw>
  <alarmRaw>0</alarmRaw>
  <global>Run</global>
  <inverter>MPPT</inverter>
  <alarm>No Alarm</alarm>
  <totalKWh>1234.56</totalKWh>
  <todayKWh>12.34</todayKWh>
  <powerOutW>2500.00</powerOutW>
  <powerInW>2600.00</powerInW>
</inverter>
```

### `/health/` - Health check
Lightweight health probe:
- Returns `OK` (HTTP 200) if inverter responds
- Returns `ERROR` (HTTP 503) if inverter is unreachable

## Features

### Robust TCP Communication
- **Timeouts**: 1s dial timeout, 1s read/write deadlines
- **Retry with backoff**: 5 attempts with exponential backoff (250ms → 2s)
- **Automatic reconnection**: Handles inverter standby/restart without ESP32 reboot
- **CRC validation**: Verifies all Aurora protocol responses

### Complete Data Parsing
- **Model detection**: Maps version codes to human-readable model names
- **State labels**: Converts raw state codes to descriptive text
- **Alarm codes**: Provides both alarm descriptions and error codes
- **Power calculations**: Computes input/output power from DSP values
- **Energy totals**: Converts raw counters to kWh values

### Aurora Protocol Support
Queries all standard Aurora inverter registers:
- `ST`: Global/inverter/alarm states
- `VR`: Version information
- `SN`: Serial number
- `CET`: Total energy production
- `CED`: Daily energy production
- `DSP3`: Output power
- `DSP23/25/26/27`: Input power components

## Troubleshooting

### Connection Issues
- Verify inverter IP/port in menuconfig
- Check network connectivity: `ping <inverter-ip>`
- Ensure inverter TCP interface is enabled
- Check firewall rules

### Nighttime Behavior
- Inverter standby is normal - endpoints will return `ERROR`
- Automatic recovery when inverter wakes up
- No ESP32 restart required

### Debugging
- Monitor serial output for connection attempts
- Use `/health/` endpoint for quick connectivity test
- Check Wi‑Fi connection status in logs

## Comparison with Go Version

This ESP32 port provides the same functionality as the Go version:
- ✅ All HTTP endpoints (`/`, `/json/`, `/xml/`, `/health/`)
- ✅ Complete Aurora protocol parsing
- ✅ Robust TCP with retry/backoff
- ✅ Human-readable state labels
- ✅ Model detection and alarm codes
- ✅ Power and energy calculations

**Advantages over go version:**
- Lower power consumption
- Standalone operation (no PC required)
- Built-in Wi‑Fi connectivity
- Configurable via menuconfig
