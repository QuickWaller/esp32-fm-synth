#include <State.h>
#include <Arduino.h>
#include <AudioDebug.h>


CurrentState::CurrentState(int sampleRate)
    : sampleRate(sampleRate) , audioTime(0.0f)
{
    // Adjust ADSR for more pronounced sound:
    // - Faster attack (0.005s)
    // - Longer decay (0.1s)
    // - Higher sustain level (0.9)
    // - Longer release (0.8s)
    this->currentADSREnvelope = ADSREnvelope(0.005f, 0.1f, 0.9f, 0.8f);
    Serial.printf("CurrentState ADSR: attack=%.4f decay=%.4f sustain=%.4f release=%.4f\n",
        currentADSREnvelope.attack, currentADSREnvelope.decay,
        currentADSREnvelope.sustain, currentADSREnvelope.release);

    //initialise default values
    this->ledPattern = majorPattern;
    this->currentOctave = 4;  // Middle octave for more audible frequencies
    this->currentModulatorIndex = 0.5f;  // Increased modulation for more noticeable FM
    this->modulatorRatio = 2.0f;

    // pre-generate all possible note frequencies
    const float c1Frequency = 32.7;
    const int c1Octave = 1;
    
    // pre-generate all note frequencies
    for (int octave = 0; octave <= 8; octave++) {
        for (int noteIndex = 0; noteIndex < 12; noteIndex++) {
            float frequency = c1Frequency * pow(2.0, (noteIndex + 12 * (octave - c1Octave)) / 12.0);
            this->allFrequencies.push_back(frequency);
        }
    }
    render_keyboard_frequencies();
}

void CurrentState::set_ADSRPreset(ADSREnvelope newADSREnvelope) {
    currentADSREnvelope = newADSREnvelope;
};

void CurrentState::set_scale(std::vector<int> ledPattern) {
    this->ledPattern = ledPattern;
};

void CurrentState::set_octave(int newOctave) {
    currentOctave = newOctave;
};

void CurrentState::render_keyboard_frequencies() {
    keyboardFrequencies.clear();
    int baseIndex = currentOctave * 12;
    for (int i = 0; i < 12; i++) {
        keyboardFrequencies.push_back(allFrequencies[baseIndex + i]);
    }
    // Serial.println("Keyboard frequencies:");
    // for (int i = 0; i < keyboardFrequencies.size(); i++) {
    //     Serial.printf("Key %d freq: %f\n", i, keyboardFrequencies[i]);
    // }
};

void CurrentState::press_key(int keyPressed, float currentTime) {
    if (keyPressed < 0 || keyPressed >= keyboardFrequencies.size()) {
        Serial.printf("Invalid key index: %d\n", keyPressed);
        return;
    }

    // First check if this key is already active
    for (Voice& voice : activeVoices) {
        if (voice.get_key_index() == keyPressed) {
            // If it is, just reset its pressed state
            voice.press_button(currentTime);
            Serial.printf("Retriggering voice for key %d at time %.4f\n", keyPressed, currentTime);
            return;
        }
    }

    // If we get here, create a new voice
    Voice voice = Voice(
        currentADSREnvelope,
        keyboardFrequencies[keyPressed],
        keyboardFrequencies[keyPressed] * modulatorRatio,
        currentModulatorIndex,
        currentTime,
        keyPressed
    );
    activeVoices.push_back(voice);
    Serial.printf("Added voice for key %d. Active voices now: %d\n", keyPressed, (int)activeVoices.size());
}

void CurrentState::depress_key(int keyPressed, float time) {
    if (keyPressed < 0 || keyPressed >= keyboardFrequencies.size()) {
        Serial.printf("Invalid key index: %d\n", keyPressed);
        return;
    }

    bool found = false;
    for (Voice& voice : activeVoices) {
        if (voice.get_key_index() == keyPressed) {
            voice.depress_button(time);
            Serial.printf("Released voice for key %d at time %.4f\n", keyPressed, time);
            found = true;
            break;
        }
    }

    if (!found) {
        Serial.printf("Warning: Tried to release non-existent voice for key %d\n", keyPressed);
    }
}

float CurrentState::render_voices(float currentAudioTime) {
    float audioOut = 0.0f;
    int activeVoiceCount = 0;
#if AUDIO_DEBUG
    static unsigned long lastDebugTime = 0;
#endif
    static const float MAX_VOICES = 6;  // Reduced from 8 for better performance
    static const float VOICE_HEADROOM = 0.7f;  // Increased headroom

    // First pass: count active voices and remove inactive ones
    for (int i = (int)activeVoices.size() - 1; i >= 0; i--) {
        if (!activeVoices[i].is_active(currentAudioTime)) {
            activeVoices.erase(activeVoices.begin() + i);
        } else {
            activeVoiceCount++;
        }
    }

    // If we have too many voices, remove the oldest ones
    while (activeVoices.size() > MAX_VOICES) {
        activeVoices.erase(activeVoices.begin());
        activeVoiceCount--;
        Serial.println("Removed oldest voice due to voice limit");
    }

    // Calculate scaling factor for all voices
    float scalingFactor = VOICE_HEADROOM / std::max(1.0f, sqrtf(activeVoiceCount));

    // Second pass: render active voices with scaling
    for (Voice& voice : activeVoices) {
        float voiceOutput = voice.fm_synthesis(sampleRate, currentAudioTime);
        audioOut += voiceOutput * scalingFactor;
    }

#if AUDIO_DEBUG
    // Debug output occasionally
    unsigned long now = millis();
    if (now - lastDebugTime >= 200) {
        Serial.printf("Voice rendering - Active: %d, Scale: %.3f, Output: %.3f\n",
            activeVoiceCount, scalingFactor, audioOut);
        lastDebugTime = now;
    }
#endif

    return audioOut;
}

int32_t CurrentState::generate_audio(uint8_t* data, int32_t byteCount) {
#if AUDIO_DEBUG
    static unsigned long lastDebugTime = 0;
    static int sampleCounter = 0;
    static float maxSample = 0.0f;
#endif
    static const float OUTPUT_SCALE = 22000.0f;  // Further reduced for more headroom
    static const float SOFT_CLIP_THRESHOLD = 0.7f;
    static const float SOFT_CLIP_FACTOR = 6.0f;

    // Pre-calculate values
    const float timeIncrement = 1.0f / sampleRate;
    const int sampleCount = byteCount / 2;
    int16_t* samples = reinterpret_cast<int16_t*>(data);

    // Process audio in chunks for better cache utilization
    static const int CHUNK_SIZE = 32;
    for (int chunk = 0; chunk < sampleCount; chunk += CHUNK_SIZE) {
#if AUDIO_DEBUG
        float chunkMax = 0.0f;
#endif
        int chunkEnd = std::min(chunk + CHUNK_SIZE, sampleCount);

        for (int i = chunk; i < chunkEnd; ++i) {
            // Generate and process sample
            float sample = render_voices(audioTime);

            // Soft clip with more aggressive curve
            if (sample > SOFT_CLIP_THRESHOLD) {
                sample = SOFT_CLIP_THRESHOLD +
                    (sample - SOFT_CLIP_THRESHOLD) /
                    (1.0f + ((sample - SOFT_CLIP_THRESHOLD) * SOFT_CLIP_FACTOR));
            } else if (sample < -SOFT_CLIP_THRESHOLD) {
                sample = -SOFT_CLIP_THRESHOLD +
                    (sample + SOFT_CLIP_THRESHOLD) /
                    (1.0f - ((sample + SOFT_CLIP_THRESHOLD) * SOFT_CLIP_FACTOR));
            }

#if AUDIO_DEBUG
            // Track maximum amplitude
            float absSample = fabsf(sample);
            if (absSample > chunkMax) {
                chunkMax = absSample;
            }
#endif

            // Convert to 16-bit
            samples[i] = static_cast<int16_t>(sample * OUTPUT_SCALE);
            audioTime += timeIncrement;
        }

#if AUDIO_DEBUG
        // Update overall max
        if (chunkMax > maxSample) {
            maxSample = chunkMax;
        }
        sampleCounter += CHUNK_SIZE;
#endif
    }

#if AUDIO_DEBUG
    // Debug output every second
    unsigned long currentTime = millis();
    if (currentTime - lastDebugTime >= 1000) {
        Serial.printf("Audio stats - Voices: %d, MaxAmp: %.4f, Samples: %d, Time: %.4f\n",
            (int)activeVoices.size(), maxSample, sampleCounter, audioTime);
        maxSample = 0.0f;
        sampleCounter = 0;
        lastDebugTime = currentTime;
    }
#endif

    return byteCount;
}

float CurrentState::get_audio_time() const {
    return audioTime;
}


//pre-generate all possible note frequencies (at all ocataves)


const std::vector<int> majorPattern                 {0, 2, 4, 5, 7, 9, 11};
const std::vector<int> minorPattern                 {0, 2, 3, 5, 7, 8, 10};
const std::vector<int> pentatonicMajorPattern 	    {0, 2, 4, 7, 9};
const std::vector<int> pentatonicMinorPattern 	    {0, 3, 5, 7, 10};
const std::vector<int> bluesPattern 	            {0, 3, 5, 6, 7, 10};
