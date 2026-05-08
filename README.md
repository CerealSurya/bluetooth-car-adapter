# Honda Fit 2010 — Bluetooth Audio Adapter (PCM5102A DAC)

Stream music from your phone to the Honda Fit's AUX input wirelessly.
This version uses an external PCM5102A DAC for clean, hiss-free audio.

---

## Parts list with Amazon links

| # | Part | Amazon link | Est. price |
|---|------|-------------|------------|
| 1 | **ESP32-WROOM-32 dev board** (2-pack, HiLetgo) | https://www.amazon.com/dp/B0FRD82Q54 | ~$11 / 2-pack |
| 2 | **PCM5102A DAC module** (GY-PCM5102, any brand) | https://www.amazon.com/dp/B0B1ZHTJS4 | ~$7 |
| 3 | **Jumper wires** (Dupont M-F/M-M/F-F kit, ELEGOO 120pc) | https://www.amazon.com/dp/B01EV70C78 | ~$7 |
| 4 | **3.5mm male-to-male stereo cable** (3ft) | Any dollar store or: https://www.amazon.com/s?k=3.5mm+stereo+cable | ~$3 |
| 5 | **USB-A to Micro-USB cable** (for power from car USB) | You likely already have one | $0–3 |

**Total: ~$25–30**

### Notes on the PCM5102A module
Several brands sell essentially the same board (Rakstore, Teyleten, CJMCU,
HiLetgo). They all work the same way. The link above goes to the Rakstore
listing but any GY-PCM5102 style board will do. Look for one with the 3.5mm
jack already soldered on — most of them have it.

**Important:** most PCM5102A modules ship with a few solder jumpers on the
back that need to be bridged for standalone (non-Raspberry Pi) use. Check
yours — they are usually labeled H1L, H2L, H3L. If they are not already
bridged, bridge them with a small blob of solder. A red LED on the module
indicates it is powered correctly.

---

## Wiring

### ESP32 → PCM5102A (I2S — 5 wires)

```
ESP32 pin       PCM5102A pin    Description
─────────────────────────────────────────────
GPIO 26    →    BCK             Bit clock
GPIO 25    →    LCK  (LRCK)    Left/Right word select
GPIO 22    →    DIN             Serial audio data
3.3V       →    VCC             Power (3.3V only — not 5V!)
GND        →    GND             Ground
```

Additionally, tie these PCM5102A control pins:
```
GND        →    FMT             Sets I2S format (required)
GND        →    SCK             No system clock needed
3.3V       →    XSMT            Soft mute disable (HIGH = audio on)
```

### PCM5102A → car
```
PCM5102A 3.5mm jack  →  car AUX jack  (via male-to-male 3.5mm cable)
```

### Power
```
Car USB port  →  ESP32 USB port  (powers everything)
ESP32 3.3V    →  PCM5102A VCC    (already covered above)
```

### Full picture
```
[Phone]
   |  Bluetooth A2DP
   ▼
[ESP32]─── GPIO26 (BCK)  ──────► [PCM5102A]
       ─── GPIO25 (LRCK) ──────►          │
       ─── GPIO22 (DATA) ──────►          │ 3.5mm jack
       ─── 3.3V  (VCC)   ──────►          │
       ─── GND            ──────►          │
                                           ▼
                                  [Car AUX jack] ──► [Head unit]
```

---

## PCM5102A solder jumper guide

Most GY-PCM5102 modules have three jumpers on the back. They are sometimes
pre-bridged, sometimes not. Check with a multimeter or just bridge them
during assembly:

| Jumper | Function          | Set to |
|--------|-------------------|--------|
| H1L    | Deemphasis        | L (bridge) |
| H2L    | Audio format      | L (bridge) — selects I2S |
| H3L    | Audio format bit  | L (bridge) |

If your module has a red power LED and audio comes out both channels, the
jumpers are fine. If one channel is silent or you get a buzzing tone, check
the jumpers first.

---

## Software setup

### 1. Install PlatformIO
VS Code extension (recommended) or CLI:
```bash
pip install platformio
```

### 2. Project structure
```
honda_bt_dac/
├── platformio.ini
└── src/
    └── main.cpp
```

### 3. Build and flash
```bash
cd honda_bt_dac
pio run --target upload
pio device monitor
```

### 4. First use
1. Power the ESP32 from the car's USB port.
2. Connect the 3.5mm cable from the PCM5102A to the car's AUX input.
3. Select AUX on the car stereo.
4. On your phone: Bluetooth settings → scan → pair "Honda Fit BT".
5. Play music. It auto-reconnects every time after the first pairing.

---

## LED status (onboard LED, GPIO2)

| Pattern | State |
|---------|-------|
| Slow blink (1 Hz) | Powered, waiting for phone |
| Fast blink (4 Hz) | Phone connected, music paused |
| Solid on | Music streaming |

---

## Customization

**Change the device name** — edit `platformio.ini`:
```
-DDEVICE_NAME='"My Car BT"'
```
Keep the outer single quotes and inner double quotes exactly as shown.

**Adjust volume** — change `INITIAL_VOLUME` in `main.cpp` (0–100).
The car stereo's volume knob is easier to use while driving.

**Change I2S pins** — edit the `#define` lines at the top of `main.cpp`:
```cpp
#define I2S_BCK_PIN   26
#define I2S_WS_PIN    25
#define I2S_DATA_PIN  22
```
Any available GPIO pins work except strapping pins (0, 2, 5, 12, 15).

---

## Troubleshooting

**No audio / silence**
- Check XSMT is tied HIGH (3.3V) — this is the soft mute pin.
  If it floats or is tied LOW, the DAC output is muted.
- Check the solder jumpers on the back of the PCM5102A (see table above).
- Confirm the 3.5mm cable is fully seated in both jacks.
- Check serial monitor — you should see "[BT] Audio stream started"
  when playing.

**One channel silent**
- Swap BCK and LRCK pins — it's an easy mix-up.
- Check solder jumpers H2L/H3L on the PCM5102A back.

**Audio crackles or drops out**
- Make sure the PCM5102A VCC is on 3.3V, not 5V.
- Try shortening the jumper wires between ESP32 and DAC.
- Check that GND is shared between ESP32 and PCM5102A.

**Can't find "Honda Fit BT" on phone**
- Wait 5 seconds after powering on for the ESP32 to boot.
- Delete old pairing from phone and re-pair.
- Check serial monitor for errors.

**PCM5102A module red LED is off**
- VCC is not connected or insufficient voltage.
- Try a different 3.3V pin on the ESP32 (some boards label it 3V3).