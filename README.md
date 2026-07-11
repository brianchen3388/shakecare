# ShakeCare Elder Fall Detection and Help Signal Prototype

ShakeCare is a hackathon prototype built on the Waveshare ESP32-S3-Matrix. One board works as the elder wristband: it uses the onboard IMU to detect abnormal impacts, help-seeking hand shakes, and possible collapse. A second board works as the caregiver hub: it receives state updates over ESP-NOW, mirrors the LED color, and hosts a local Wi-Fi dashboard for a computer.

The current demo completes the loop from wristband sensing to caregiver display and web dashboard.

## Completed Features

- The elder wristband reads 3-axis acceleration from the QMI8658 IMU.
- The 8x8 RGB Matrix shows three states: green for normal, yellow for watch, red for alert.
- The detection logic handles normal impact, severe impact, help-seeking shake after warning, and 10 seconds of near-stillness after warning.
- The elder board sends state updates to the caregiver board through ESP-NOW MAC unicast.
- The caregiver board does not run IMU detection. It only receives state, mirrors the LED color, and serves the computer dashboard.
- The caregiver dashboard includes live state, 8x8 preview, basic metrics, bash-style log, timeline hover details, and state-change animations.
- The roadshow pitch deck and slide previews are included.

## Project Structure

```text
shakecare/
  README.md
  assets/
    shakecare-product-hero.png
  firmware/
    elder_wristband/
      elder_wristband.ino
      shakecare_shared.h
    caregiver_hub/
      caregiver_hub.ino
      dashboard_page.h
      shakecare_shared.h
    tools/
      mac_address_reader/
        mac_address_reader.ino
  presentations/
    roadshow/
      ShakeCare_Roadshow_Pitch.pptx
      preview/
        slide-1.png ... slide-9.png
      qa/
        ShakeCare_Roadshow_Pitch.inspect.ndjson
```

### Firmware Roles

| Path | Target board | Purpose |
| --- | --- | --- |
| `firmware/elder_wristband/elder_wristband.ino` | Elder wristband board | Reads IMU, decides state, displays LED state, sends ESP-NOW updates |
| `firmware/caregiver_hub/caregiver_hub.ino` | Caregiver hub board | Receives ESP-NOW, displays LED state, starts Wi-Fi AP and web dashboard |
| `firmware/tools/mac_address_reader/mac_address_reader.ino` | Any board | Prints the STA MAC address for ESP-NOW unicast setup |
| `firmware/*/shakecare_shared.h` | Local header in both role sketches | State enum, message struct, LED constants, matrix orientation mapping |
| `firmware/caregiver_hub/dashboard_page.h` | Caregiver dashboard | Embedded HTML/CSS/JS dashboard |

`shakecare_shared.h` is duplicated inside both role sketch folders so Arduino IDE can compile each sketch directly when that folder is opened. Keep both copies identical when changing the shared message contract or matrix constants.

## Hardware Setup

- Board: Waveshare ESP32-S3-Matrix
- LED Matrix: onboard 8x8 RGB, 64 pixels
- LED data pin: GPIO 14
- LED format: `NEO_RGB + NEO_KHZ800`
- IMU: QMI8658
- IMU I2C: SDA GPIO 11, SCL GPIO 12
- IMU I2C address: `0x6B`
- Arduino FQBN: `esp32:esp32:esp32s3`

The matrix orientation correction is kept in both `shakecare_shared.h` files:

```cpp
inline uint16_t shakeCarePixelIndex(uint8_t row, uint8_t col) {
  return (SHAKECARE_WIDTH - 1 - col) * SHAKECARE_WIDTH + row;
}
```

## State Logic

### Color Meaning

| State | Color | Meaning |
| --- | --- | --- |
| `STATE_NORMAL` | Green | Normal activity |
| `STATE_WARNING` | Yellow | Possible fall or abnormal movement; needs observation |
| `STATE_ALERT` | Red | Severe impact, active help signal, or possible collapse |

### Elder Board Flow

1. Read acceleration every `40ms`.
2. Compute `mag`: the current acceleration magnitude, used for instant impact strength.
3. Compute `delta`: the acceleration change between adjacent samples, used for sudden movement changes.
4. Compute `accelActivity`: recent accumulated acceleration activity. It is not physical distance; it is a simplified signal for recent motion intensity.
5. In the normal state, only evaluate impact thresholds after `accelActivity` crosses its gate.
6. Severe impact goes directly to red.
7. Normal impact enters yellow.
8. After entering yellow, wait for `SHAKE_ARM_MS` before accepting shake gestures, avoiding false alerts from fall aftershock.
9. Continuous shake gestures in yellow enter red.
10. Near-stillness for `10s` in yellow enters red.
11. Yellow returns to green after `15s` if it does not escalate.

## Current Parameters

These parameters are at the top of `firmware/elder_wristband/elder_wristband.ino`.

| Parameter | Current value | Purpose |
| --- | ---: | --- |
| `SAMPLE_MS` | `40` | IMU sampling interval |
| `IMPACT_DELTA_G` | `1.35` | Acceleration-change threshold for normal impact |
| `IMPACT_MAG_G` | `2.5` | Acceleration-magnitude threshold for normal impact |
| `SEVERE_IMPACT_DELTA_G` | `3.5` | Acceleration-change threshold for severe impact |
| `SEVERE_IMPACT_MAG_G` | `4.5` | Acceleration-magnitude threshold for severe impact |
| `ACCEL_ACTIVITY_G` | `3.5` | Recent motion intensity gate |
| `ACCEL_ACTIVITY_DECAY` | `0.86` | Motion intensity decay factor |
| `SHAKE_DELTA_G` | `0.65` | Single shake threshold for help signal |
| `SHAKE_HITS_REQUIRED` | `4` | Required shake count for help signal |
| `SHAKE_ARM_MS` | `1200` | Protection delay before shake detection after yellow |
| `STILL_DELTA_G` | `0.045` | Near-stillness threshold |
| `STILL_ALERT_MS` | `10000` | Stillness duration before alert in yellow |
| `WARNING_CLEAR_MS` | `15000` | Auto-clear duration for yellow |
| `ESPNOW_SEND_MS` | `150` | ESP-NOW state send interval |

## ESP-NOW Communication

The current setup uses MAC-address unicast, not broadcast.

The message structure is kept identical in both role sketches:

```cpp
struct ShakeCareMessage {
  uint8_t type;
  uint8_t state;
  uint32_t seq;
};
```

The elder board sends `state` to `CAREGIVER_MAC`. The caregiver board updates its LED matrix, `/status` JSON endpoint, and web log after receiving the packet.

If the caregiver board receives no ESP-NOW packet for more than `1000ms`, it shows yellow to indicate a connection or sync problem.

## Flashing Flow

The commands below are for compile checks. To upload from Arduino IDE, open the corresponding sketch folder, select the board and port, then click Upload. If using Arduino CLI, replace `compile` with `upload -p COMx`, where `COMx` is your board's serial port.

1. First upload the MAC reader to the caregiver board:

   ```powershell
   & "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "firmware/tools/mac_address_reader"
   ```

   After upload, open Serial Monitor at `115200` and copy the printed `STA MAC`.

2. Convert the MAC from `AA:BB:CC:DD:EE:FF` into this format, then put it into `CAREGIVER_MAC` in the elder sketch:

   ```cpp
   const uint8_t CAREGIVER_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
   ```

   The current code already contains one configured MAC:

   ```cpp
   const uint8_t CAREGIVER_MAC[] = {0xA0, 0xF2, 0x62, 0xEB, 0x27, 0x88};
   ```

   If you switch to another caregiver board, read its MAC again and replace this value.

3. Upload the elder wristband sketch:

   ```powershell
   & "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "firmware/elder_wristband"
   ```

4. Upload the caregiver hub sketch:

   ```powershell
   & "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "firmware/caregiver_hub"
   ```

5. Connect your computer to the caregiver Wi-Fi:

   - SSID: `ShakeCare-Caregiver`
   - Password: `12345678`
   - URL: `http://192.168.4.1`

You can open these sketch folders directly in Arduino IDE:

- Elder board: `firmware/elder_wristband`
- Caregiver board: `firmware/caregiver_hub`
- MAC tool: `firmware/tools/mac_address_reader`

## Usage Guide

### 1. Prepare the boards

Use two ESP32-S3-Matrix boards:

- Elder board: flash `firmware/elder_wristband`.
- Caregiver board: flash `firmware/caregiver_hub`.

Make sure the caregiver board STA MAC is already copied into `CAREGIVER_MAC` in `firmware/elder_wristband/elder_wristband.ino`. If the caregiver board is replaced, read its MAC again with `firmware/tools/mac_address_reader`.

### 2. Start the demo

1. Power on the caregiver board first.
2. Power on the elder board.
3. Wait a few seconds for ESP-NOW to start.
4. The elder board should show green if the IMU starts correctly.
5. The caregiver board should mirror the elder board color after receiving packets.

If the caregiver board stays yellow, it usually means no recent ESP-NOW packet was received. Check the MAC address, power, and board distance.

### 3. Open the dashboard

1. On your computer, connect to Wi-Fi `ShakeCare-Caregiver`.
2. Use password `12345678`.
3. Open `http://192.168.4.1`.

The dashboard shows:

- Current state: `NORMAL`, `WARNING`, or `ALERT`.
- Link status: online/offline based on recent ESP-NOW packets.
- Last packet age, sequence number, uptime, and connected client count.
- Bash-style event log.
- Timeline with hover details.

### 4. Trigger the states

- Normal movement should stay green.
- A suspicious impact should turn the elder and caregiver boards yellow.
- A severe impact should turn both boards red.
- After yellow, repeated hand shaking should turn both boards red.
- After yellow, about 10 seconds of near-stillness should turn both boards red.
- If yellow does not escalate, it returns to green after about 15 seconds.

For the live demo, use moderate movements first. The thresholds are intentionally adjustable because real IMU readings vary by board, strap position, and how the device is worn.

### 5. Tune the behavior

Tune these first in `firmware/elder_wristband/elder_wristband.ino`:

- `ACCEL_ACTIVITY_G`: raises or lowers the activity gate before impact checks.
- `IMPACT_DELTA_G` and `IMPACT_MAG_G`: tune yellow sensitivity.
- `SEVERE_IMPACT_DELTA_G` and `SEVERE_IMPACT_MAG_G`: tune direct red sensitivity.
- `SHAKE_DELTA_G` and `SHAKE_HITS_REQUIRED`: tune help-seeking shake detection.
- `STILL_DELTA_G` and `STILL_ALERT_MS`: tune stillness-to-alert behavior.

After changing parameters, re-upload only the elder board sketch unless the ESP-NOW message structure changes.

## Caregiver Web API

After the caregiver board starts its AP, it serves two HTTP routes:

| Path | Purpose |
| --- | --- |
| `/` | Dashboard page |
| `/status` | Live state JSON |

Example `/status` response:

```json
{
  "online": true,
  "state": 0,
  "stateKey": "normal",
  "stateText": "NORMAL",
  "ageMs": 120,
  "seq": 42,
  "uptimeMs": 60000,
  "clients": 1
}
```

Dashboard behavior:

- Append a bash-style log immediately on state change.
- Append a heartbeat log every 5 seconds if the state does not change.
- Timeline entries show state, connection status, and seq on hover.
- `Freeze log` pauses new log entries.
- `Clear log` clears both the log and timeline.

## Technical Iteration Log

### v0.1 Single-board Detection

- Implemented QMI8658 initialization and 3-axis acceleration reading.
- Implemented green, yellow, and red LED states.
- Implemented the state machine for normal impact, severe impact, help-seeking shake, and stillness alert after yellow.
- Renamed the earlier "distance" idea to `accelActivity` to avoid implying physical displacement.

### v0.2 Two-board Link

- Added ESP-NOW MAC unicast.
- Split elder and caregiver firmware into separate sketches.
- Removed IMU logic from the caregiver board; it only receives and displays state.
- Added the MAC address reader sketch.

### v0.3 Dashboard and Project Cleanup

- Added Wi-Fi AP and local dashboard to the caregiver board.
- Replaced EventStream with `/status` polling, bash-style log, and timeline history.
- Updated the page into a light dashboard style with state animation and matrix preview.
- Added the roadshow deck: `presentations/roadshow/ShakeCare_Roadshow_Pitch.pptx`.
- Reorganized the project into clear firmware, dashboard, presentation, and asset folders. Shared protocol headers stay inside each sketch folder so Arduino IDE can compile them directly.

## Roadshow Materials

- Deck: `presentations/roadshow/ShakeCare_Roadshow_Pitch.pptx`
- Slide previews: `presentations/roadshow/preview/`
- Product concept image: `assets/shakecare-product-hero.png`

The deck focuses on product creativity, user pain, demo flow, social value, and business direction instead of low-level implementation details.

## Pain Point and Value

Elders living alone or people with mobility issues may be unable to pick up a phone or press an emergency button after a fall, dizziness, or low-blood-sugar event. Camera-based systems can help, but they create privacy pressure in bedrooms and bathrooms.

ShakeCare uses a low-cost, low-intrusion wearable prototype to capture two important signals:

- Passive abnormal events: sudden impact and stillness after a possible fall.
- Active help signal: repeated hand shaking after the warning state.

It is not intended to replace medical equipment. It is designed as an earlier and more visible risk signal for family care, night patrols in care homes, and community elder support.

## Future Directions

- Add manual reset after red alert.
- Print `mag`, `delta`, `accelActivity`, and trigger reason over Serial for tuning.
- Add wearing-orientation calibration to reduce false positives from different strap positions.
- Upload caregiver dashboard events to a PC app or cloud service for event history.
- Add battery, battery indicator, vibration feedback, enclosure, and wrist strap.
- Collect samples for walking, sitting down, table hits, drops, and help-seeking shakes to reduce false positives and missed alerts.
