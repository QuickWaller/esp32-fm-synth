#include <Arduino.h>
#include <ADSR.h>
#include <AudioDebug.h>
#include <cmath>

ADSREnvelope::ADSREnvelope(float attack, float decay, float sustain, float release)
    : attack(attack), decay(decay), sustain(sustain), release(release) {}

ADSREnvelope::ADSREnvelope()
    : attack(0.005f), decay(0.1f), sustain(0.7f), release(0.3f) {}

float ADSREnvelope::get_amplitude(float timeSincePressed, bool isNoteOn, float timeSinceReleased) {
#if AUDIO_DEBUG
    // Debug output occasionally
    static unsigned long lastDebugTime = 0;
    unsigned long currentTime = millis();
#endif

    float amplitude = 0.0f;
    
    // Ensure time values are not negative
    timeSincePressed = std::max(0.0f, timeSincePressed);
    timeSinceReleased = std::max(0.0f, timeSinceReleased);
    
    if (isNoteOn) {
        // Note is still being held
        if (timeSincePressed < attack) {
            // Attack phase - linear ramp up
            amplitude = (timeSincePressed / attack);
#if AUDIO_DEBUG
            if (currentTime - lastDebugTime >= 200) {
                Serial.printf("ADSR Attack - TimeSincePress: %.4f, Target: %.4f, Amp: %.4f\n",
                    timeSincePressed, attack, amplitude);
            }
#endif
        } else if (timeSincePressed < (attack + decay)) {
            // Decay phase - exponential decay to sustain level
            float decayProgress = (timeSincePressed - attack) / decay;
            amplitude = 1.0f - ((1.0f - sustain) * decayProgress);
#if AUDIO_DEBUG
            if (currentTime - lastDebugTime >= 200) {
                Serial.printf("ADSR Decay - TimeSincePress: %.4f, Progress: %.4f, Target: %.4f, Amp: %.4f\n",
                    timeSincePressed, decayProgress, sustain, amplitude);
            }
#endif
        } else {
            // Sustain phase
            amplitude = sustain;
#if AUDIO_DEBUG
            if (currentTime - lastDebugTime >= 200) {
                Serial.printf("ADSR Sustain - TimeSincePress: %.4f, Level: %.4f\n",
                    timeSincePressed, amplitude);
            }
#endif
        }
    } else {
        // Note has been released
        float releaseProgress = timeSinceReleased / release;
        
        // Calculate the starting amplitude based on where we were in the envelope
        float startAmp;
        if (timeSincePressed < attack) {
            startAmp = (timeSincePressed / attack);
        } else if (timeSincePressed < (attack + decay)) {
            float decayProgress = (timeSincePressed - attack) / decay;
            startAmp = 1.0f - ((1.0f - sustain) * decayProgress);
        } else {
            startAmp = sustain;
        }
        
        // Apply exponential release curve
        if (releaseProgress < 1.0f) {
            // Use exponential curve for smoother release
            amplitude = startAmp * expf(-4.0f * releaseProgress);
#if AUDIO_DEBUG
            if (currentTime - lastDebugTime >= 200) {
                Serial.printf("ADSR Release - TimeSinceRelease: %.4f, Progress: %.4f, StartAmp: %.4f, Amp: %.4f\n",
                    timeSinceReleased, releaseProgress, startAmp, amplitude);
            }
#endif
        } else {
            amplitude = 0.0f;
        }
    }

#if AUDIO_DEBUG
    if (currentTime - lastDebugTime >= 200) {
        lastDebugTime = currentTime;
    }
#endif

    return amplitude;
} 