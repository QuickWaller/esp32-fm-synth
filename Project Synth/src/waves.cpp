#include <math.h>
#include <Waves.h>
#include <Arduino.h>
#include <AudioDebug.h>

#include <vector>
#include <string>
#include <stdexcept>

// Pre-calculated sine table for faster lookup
static const int SINE_TABLE_SIZE = 1024;
static float sineTable[SINE_TABLE_SIZE];
static bool sineTableInitialized = false;

void initSineTable() {
    if (!sineTableInitialized) {
        for (int i = 0; i < SINE_TABLE_SIZE; i++) {
            sineTable[i] = sinf(2.0f * M_PI * i / SINE_TABLE_SIZE);
        }
        sineTableInitialized = true;
    }
}

Wave::Wave(float frequency, WaveType waveform)
    : frequency(frequency), waveform(waveform) {
    initSineTable();  // Initialize sine table if not already done
    Serial.printf("Wave created with frequency %.1f Hz, type %d\n", frequency, waveform);
}

// getters and setters
float Wave::get_amplitude() { 
    return amplitude; 
}

void Wave::set_amplitude(float amplitude) { 
    this->amplitude = amplitude; 
}

float Wave::get_frequency() { 
    return frequency; 
}

void Wave::set_frequency(float frequency) { 
    this->frequency = frequency; 
}

WaveType Wave::get_wave_type() { 
    return waveform; 
}

void Wave::set_wave_type(WaveType waveform) { 
    this->waveform = waveform; 
}

float Wave::get_signal_value(float phase) {
    // Ensure phase is between 0 and 1
    phase = fmod(phase, 1.0f);
    if (phase < 0.0f) phase += 1.0f;

    float value = 0.0f;

#if AUDIO_DEBUG
    // Debug output occasionally
    static unsigned long lastDebugTime = 0;
    unsigned long currentTime = millis();
#endif

    switch (waveform) {
        case SINE: {
            // Fast sine lookup using pre-calculated table
            float tableIndex = phase * SINE_TABLE_SIZE;
            int index1 = (int)tableIndex;
            int index2 = (index1 + 1) % SINE_TABLE_SIZE;
            float frac = tableIndex - index1;
            
            // Linear interpolation between table values
            value = sineTable[index1] * (1.0f - frac) + sineTable[index2] * frac;
            break;
        }
            
        case SQUARE: {
            // Optimized square wave with reduced harmonics
            value = (phase < 0.5f) ? 0.7f : -0.7f;
            break;
        }
            
        case TRIANGLE: {
            // Optimized triangle wave
            value = (phase < 0.5f) ? 
                (4.0f * phase - 1.0f) : 
                (3.0f - 4.0f * phase);
            value *= 0.8f;  // Scale to prevent clipping
            break;
        }
            
        case SAWTOOTH: {
            // Optimized sawtooth with fewer harmonics
            value = 0.0f;
            const int numHarmonics = 4;  // Reduced harmonics for better performance
            float phaseMultiple = 2.0f * M_PI * phase;
            
            for (int i = 1; i <= numHarmonics; i++) {
                // Use sine table for harmonics
                float harmonicPhase = fmod(phase * i, 1.0f) * SINE_TABLE_SIZE;
                int index = (int)harmonicPhase;
                float frac = harmonicPhase - index;
                value += (sineTable[index] * (1.0f - frac) + 
                         sineTable[(index + 1) % SINE_TABLE_SIZE] * frac) / i;
            }
            value *= 0.5f;  // Scale down to prevent clipping
            break;
        }
            
        default:
            value = 0.0f;
            break;
    }
    
    value *= amplitude;

#if AUDIO_DEBUG
    // Debug output every 500ms
    if (currentTime - lastDebugTime >= 500) {
        Serial.printf("Wave %.1f Hz - Phase: %.4f, Raw: %.4f, Amp: %.4f, Final: %.4f\n",
            frequency, phase, value/amplitude, amplitude, value);
        lastDebugTime = currentTime;
    }
#endif

    return value;
}
