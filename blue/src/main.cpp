/**
 * Honda Fit 2010 Bluetooth Audio Adapter
 * WITH EXTERNAL PCM5102A DAC (I2S)
 * ----------------------------------------
 * ESP32 receives audio over Bluetooth A2DP from your phone,
 * then sends it digitally over I2S to a PCM5102A DAC module.
 * The PCM5102A does the analog conversion cleanly — no hiss,
 * no noise floor, no RC filter needed.
 *
 * PCM5102A wiring (I2S):
 * ┌─────────────┬──────────────┬────────────────────────────┐
 * │ PCM5102A pin│ ESP32 GPIO   │ Description                │
 * ├─────────────┼──────────────┼────────────────────────────┤
 * │ VCC         │ 3.3V         │ Power                      │
 * │ GND         │ GND          │ Ground                     │
 * │ BCK         │ GPIO 26      │ I2S Bit Clock              │
 * │ LCK (LRCK) │ GPIO 25      │ I2S Left/Right Word Select │
 * │ DIN         │ GPIO 22      │ I2S Serial Data            │
 * │ FMT         │ GND          │ I2S format (tie to GND)    │
 * │ XSMT        │ 3.3V         │ Soft mute — HIGH = on      │
 * │ SCK         │ GND          │ No system clock needed     │
 * └─────────────┴──────────────┴────────────────────────────┘
 *
 * 3.5mm jack wiring (from PCM5102A module):
 *   LOUT → Left channel  (tip)
 *   ROUT → Right channel (ring)
 *   GND  → Ground        (sleeve)
 *   (Most GY-PCM5102 modules already have a 3.5mm jack on-board)
 *
 * Power:
 *   ESP32 5V/VIN ← car USB port (data pins not connected)
 *   PCM5102A VCC ← ESP32 3.3V pin
 *
 * Library: pschatzmann/ESP32-A2DP
 * PlatformIO: see platformio.ini
 */

#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// ─────────────────────────────────────────────
// I2S pin definitions — match the wiring table above.
// Change these if you use different GPIO pins.
// ─────────────────────────────────────────────
#define I2S_BCK_PIN   26   // Bit clock
#define I2S_WS_PIN    25   // Word select (LRCK)
#define I2S_DATA_PIN  22   // Serial data out

// ─────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────
#ifndef DEVICE_NAME
  #define DEVICE_NAME "Honda Fit BT"
#endif

// Software volume: 0 (mute) to 100 (full).
// With the PCM5102A the output level is strong — 75 is a good start.
// The car stereo volume knob is your primary control.
static const int INITIAL_VOLUME = 75;

// ─────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────
I2SStream           i2s_out;
BluetoothA2DPSink   a2dp_sink(i2s_out);

static bool  bt_connected  = false;
static bool  audio_playing = false;
static ulong last_blink_ms = 0;

// ─────────────────────────────────────────────
// Callbacks
// ─────────────────────────────────────────────
void connection_state_changed(esp_a2d_connection_state_t state, void*) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        bt_connected = true;
        Serial.println("[BT] Phone connected");
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        bt_connected  = false;
        audio_playing = false;
        Serial.println("[BT] Phone disconnected");
    }
}

void audio_state_changed(esp_a2d_audio_state_t state, void*) {
    if (state == ESP_A2D_AUDIO_STATE_STARTED) {
        audio_playing = true;
        Serial.println("[BT] Audio stream started");
    } else {
        audio_playing = false;
        Serial.println("[BT] Audio stream stopped");
    }
}

// ─────────────────────────────────────────────
// LED blink patterns  (onboard LED = GPIO2)
//   Slow blink  (1 Hz)  — waiting for phone
//   Fast blink  (4 Hz)  — connected, idle
//   Solid on            — music playing
// ─────────────────────────────────────────────
static const int LED_PIN = 2;

void update_led() {
    if (audio_playing) {
        digitalWrite(LED_PIN, HIGH);
        return;
    }
    ulong interval = bt_connected ? 125 : 500;
    if (millis() - last_blink_ms >= interval) {
        last_blink_ms = millis();
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
}

// ─────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("=================================");
    Serial.println("  Honda Fit BT — PCM5102A DAC");
    Serial.println("  Starting up...");
    Serial.println("=================================");

    pinMode(LED_PIN, OUTPUT);

    // Configure I2S output to match the PCM5102A's requirements:
    //   44100 Hz sample rate, stereo, 16-bit (A2DP SBC default)
    auto cfg = i2s_out.defaultConfig(TX_MODE);
    cfg.pin_bck       = I2S_BCK_PIN;
    cfg.pin_ws        = I2S_WS_PIN;
    cfg.pin_data      = I2S_DATA_PIN;
    cfg.sample_rate   = 44100;
    cfg.channels      = 2;
    cfg.bits_per_sample = 16;
    i2s_out.begin(cfg);

    // Register state callbacks before starting Bluetooth
    a2dp_sink.set_on_connection_state_changed(connection_state_changed);
    a2dp_sink.set_on_audio_state_changed(audio_state_changed);

    // Set initial software volume
    a2dp_sink.set_volume(INITIAL_VOLUME);

    // Start Bluetooth advertising
    a2dp_sink.start(DEVICE_NAME);

    Serial.print("[BT] Advertising as: ");
    Serial.println(DEVICE_NAME);
    Serial.println("[BT] Waiting for phone to connect...");
    Serial.printf("[I2S] BCK=%d  WS=%d  DATA=%d\n",
                  I2S_BCK_PIN, I2S_WS_PIN, I2S_DATA_PIN);
}

// ─────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────
void loop() {
    update_led();

    // Print status every 5 seconds while playing
    static ulong last_stats_ms = 0;
    if (audio_playing && millis() - last_stats_ms >= 5000) {
        last_stats_ms = millis();
        Serial.printf("[AUDIO] Playing  Volume: %d/100\n",
                      a2dp_sink.get_volume());
    }

    delay(10);
}