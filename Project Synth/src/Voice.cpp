#include <Voice.h>
#include <Arduino.h>
#include <AudioDebug.h>

Voice::Voice(
    ADSREnvelope adsrEnvelope, 
    float carrierFrequency, 
    float modulatorFrequency,
    float modulatorIndex,
    float currentAudioTime,
    int keyIndex
) : adsrEnvelope(adsrEnvelope), 
    carrierFrequency(carrierFrequency), 
    modulatorFrequency(modulatorFrequency), 
    modulatorIndex(modulatorIndex),
    keyIndex(keyIndex),
    timePressed(currentAudioTime),
    timeDepressed(-1.0f)
{
    Serial.printf("Voice constructor ADSR: attack=%.4f decay=%.4f sustain=%.4f release=%.4f\n",
                  adsrEnvelope.attack, adsrEnvelope.decay, 
                  adsrEnvelope.sustain, adsrEnvelope.release);
    Serial.printf("Voice %d created at audio time: %.4f\n", keyIndex, currentAudioTime);

    // Initialize oscillators with correct frequencies
    carrierWave = Wave(carrierFrequency, SINE);
    modulatorWave = Wave(modulatorFrequency, SINE);
    
    // Reset phases for clean start
    modulatorPhase = 0.0f;
    carrierPhase = 0.0f;
}

void Voice::press_button(float currentAudioTime) {
    timePressed = currentAudioTime;
    timeDepressed = -1.0f;
    // Reset phases when retriggering to ensure clean start
    modulatorPhase = 0.0f;
    carrierPhase = 0.0f;
    Serial.printf("[VOICE %d] press_button called at audio time %.4f\n", keyIndex, currentAudioTime);
}

void Voice::depress_button(float currentAudioTime) {
    timeDepressed = currentAudioTime;
    Serial.printf("[VOICE %d] depress_button called at audio time %.4f\n", keyIndex, currentAudioTime);
}

int Voice::get_key_index() {
    return keyIndex;
}

float Voice::fm_synthesis(float sampleRate, float currentAudioTime) {
#if AUDIO_DEBUG
    static unsigned long lastDebugTime = 0;
#endif

    // Calculate time since key press/release for ADSR
    float timeSincePressed = currentAudioTime - timePressed;
    float timeSinceRelease = (timeDepressed < 0.0f) ? 0.0f : (currentAudioTime - timeDepressed);
    bool isNoteOn = (timeDepressed < 0.0f);

    // Get the current amplitude from ADSR
    float amplitude = adsrEnvelope.get_amplitude(timeSincePressed, isNoteOn, timeSinceRelease);
    
    // If amplitude is 0, return early
    if (amplitude <= 0.0f) {
        return 0.0f;
    }

    // Calculate phase increments based on frequencies
    float modulatorPhaseIncrement = modulatorFrequency / sampleRate;
    float carrierPhaseIncrement = carrierFrequency / sampleRate;

    // Update modulator phase with anti-aliasing
    modulatorPhase += modulatorPhaseIncrement;
    if (modulatorPhase >= 1.0f) {
        modulatorPhase = fmod(modulatorPhase, 1.0f);
    }
    
    // Get modulator signal and apply FM
    float modulatorValue = modulatorWave.get_signal_value(modulatorPhase);
    float modulationAmount = modulatorValue * modulatorIndex;
    
    // Update carrier phase with anti-aliasing
    carrierPhase += carrierPhaseIncrement + modulationAmount;
    if (carrierPhase >= 1.0f) {
        carrierPhase = fmod(carrierPhase, 1.0f);
    }
    
    // Get carrier signal
    float sample = carrierWave.get_signal_value(carrierPhase);
    sample *= amplitude;

#if AUDIO_DEBUG
    // Debug output every 200ms
    unsigned long currentTime = millis();
    if (currentTime - lastDebugTime >= 200) {
        Serial.printf("Voice %d - Freq: %.1f Hz, Mod: %.1f Hz, ModIdx: %.3f, Amp: %.3f, AudioTime: %.4f, TimeSincePress: %.4f\n",
            keyIndex, carrierFrequency, modulatorFrequency, modulatorIndex, amplitude, currentAudioTime, timeSincePressed);
        lastDebugTime = currentTime;
    }
#endif

    return sample;
}

bool Voice::is_active(float currentTime) {
    float timeSincePressed = currentTime - timePressed;
    float timeSinceDepressed = (timeDepressed < 0.0f) ? 0.0f : (currentTime - timeDepressed);
    bool isNoteOn = (timeDepressed < 0.0f);
    
    float amp = adsrEnvelope.get_amplitude(timeSincePressed, isNoteOn, timeSinceDepressed);

#if AUDIO_DEBUG
    // Only print debug info occasionally
    static unsigned long lastPrintTime = 0;
    unsigned long nowMillis = millis();
    if (nowMillis - lastPrintTime >= 200) {
        Serial.printf("Voice %d - Active check: AudioTime=%.4f, TimeSincePress=%.4f, TimeSinceRelease=%.4f, Amp=%.4f\n",
            keyIndex, currentTime, timeSincePressed, timeSinceDepressed, amp);
        lastPrintTime = nowMillis;
    }
#endif

    return amp > 0.0f;
}




