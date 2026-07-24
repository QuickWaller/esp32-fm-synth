#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <AudioTools.h>

#include <Waves.h>
#include <Notes.h>  // for MajorScale and Note
#include <State.h>
#include <AudioDebug.h>
#include <vector>
#include <map>

// Static buffer for audio processing
static const int MONO_BUFFER_SIZE = 256;  // Reduced from 512 for lower latency
static int16_t monoBuffer[MONO_BUFFER_SIZE];

// Global state
CurrentState state(44100);

// Button configuration
const int notePins[12] = { 32, 33, 25, 26, 27, 14, 23, 22, 21, 19, 18, 17};
int buttonsCurrentlyPressed[12] = {0};

// Button debouncing
static const unsigned long BUTTON_DEBOUNCE_MS = 10;  // Increased to 10ms for better stability
unsigned long lastDebounceTime[12] = {0};
int lastButtonState[12] = {HIGH};
int buttonState[12] = {HIGH};
int lastStableState[12] = {HIGH};  // Track the last stable state

// Audio callback - optimized for performance
int32_t audio_callback(uint8_t* data, int32_t byteCount) {
#if AUDIO_DEBUG
    static unsigned long lastDebugTime = 0;
#endif

    // Calculate buffer sizes
    int32_t monoSampleCount = byteCount / 4;  // 4 bytes per stereo sample
    if (monoSampleCount > MONO_BUFFER_SIZE) {
        monoSampleCount = MONO_BUFFER_SIZE;  // Prevent buffer overflow
    }
    
    // Generate mono audio
    int32_t result = state.generate_audio(reinterpret_cast<uint8_t*>(monoBuffer), monoSampleCount * 2);
    
    // Convert mono to stereo with direct memory access
    int16_t* stereoSamples = reinterpret_cast<int16_t*>(data);
    for (int i = 0; i < monoSampleCount; i++) {
        int32_t idx = i * 2;
        stereoSamples[idx] = monoBuffer[i];     // Left channel
        stereoSamples[idx + 1] = monoBuffer[i]; // Right channel
    }
    
#if AUDIO_DEBUG
    // Debug output
    unsigned long currentTime = millis();
    if (currentTime - lastDebugTime >= 1000) {
        Serial.printf("Audio callback - Buffer size: %d, Mono samples: %d\n",
            byteCount, monoSampleCount);
        lastDebugTime = currentTime;
    }
#endif

    return byteCount;
}

// Bluetooth setup
BluetoothA2DPSource bluetoothMaster;

// Setup function
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting ESP32 Synth...");

    // Configure pins with internal pullup
    for (int i = 0; i < 12; i++) {
        pinMode(notePins[i], INPUT_PULLUP);
        // Initialize button states
        buttonState[i] = digitalRead(notePins[i]);
        lastButtonState[i] = buttonState[i];
        lastStableState[i] = buttonState[i];
        Serial.printf("Configured pin %d for input\n", notePins[i]);
    }

    // Initialize Bluetooth
    Serial.println("Beginning bluetooth search...");
    bluetoothMaster.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void *) {
        switch (state) {
            case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
                Serial.println("Bluetooth disconnected");
                break;
            case ESP_A2D_CONNECTION_STATE_CONNECTING:
                Serial.println("Connecting to Bluetooth speaker...");
                break;
            case ESP_A2D_CONNECTION_STATE_CONNECTED:
                Serial.println("Connected to Bluetooth speaker!");
                break;
            case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
                Serial.println("Disconnecting from Bluetooth speaker...");
                break;
        }
    });

    // Start Bluetooth with default settings (44.1kHz, 16-bit, stereo)
    bluetoothMaster.start("Tronsmart Trip");
    bluetoothMaster.set_data_callback(audio_callback);
    Serial.println("Bluetooth initialized");
}

// Main loop
void loop() {
    static unsigned long lastPollTime = 0;
    const unsigned long POLL_INTERVAL = 1;  // 1ms polling interval
    
    unsigned long currentTime = millis();
    if (currentTime - lastPollTime >= POLL_INTERVAL) {
        float audioTime = state.get_audio_time();
        
        // Poll all buttons with improved debouncing
        for (int i = 0; i < 12; i++) {
            int reading = digitalRead(notePins[i]);
            
            // If the reading has changed, reset the debounce timer
            if (reading != lastButtonState[i]) {
                lastDebounceTime[i] = currentTime;
            }
            
            // Check if enough time has passed since the last change
            if ((currentTime - lastDebounceTime[i]) > BUTTON_DEBOUNCE_MS) {
                // If the button state has stabilized
                if (reading != buttonState[i]) {
                    buttonState[i] = reading;
                    
                    // Only trigger on stable state changes
                    if (buttonState[i] != lastStableState[i]) {
                        if (buttonState[i] == LOW && !buttonsCurrentlyPressed[i]) {
                            state.press_key(i, audioTime);
                            buttonsCurrentlyPressed[i] = 1;
                            Serial.printf("Key press  %d at time %.4f\n", i, audioTime);
                        }
                        else if (buttonState[i] == HIGH && buttonsCurrentlyPressed[i]) {
                            state.depress_key(i, audioTime);
                            buttonsCurrentlyPressed[i] = 0;
                            Serial.printf("Key depress %d at time %.4f\n", i, audioTime);
                        }
                        lastStableState[i] = buttonState[i];
                    }
                }
            }
            
            lastButtonState[i] = reading;
        }
        
        lastPollTime = currentTime;
    }
    
    // Small yield to prevent watchdog triggers
    vTaskDelay(1);
}


