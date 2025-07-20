#ifndef WAKE_WORD_MANAGER_H
#define WAKE_WORD_MANAGER_H

#include <memory>
// #include <esp_ns.h> // ESP32-S3 only, disabled.
#include "simplevox.h"

class WakeWordManager {
public:
    static constexpr int kSampleRate = 16000;

    WakeWordManager();
    ~WakeWordManager();

    bool init();
    void reset();
    
    bool startListening();
    void stopListening();

    // Wake word detection loop
    bool listenAndDetect();

    // Wake word registration process
    int captureAndRegisterWakeWord();

private:
    static constexpr int kAudioLengthSecs = 3; // Max audio length for VAD buffer
    static constexpr int kAudioLength = kSampleRate * kAudioLengthSecs;
    static constexpr int kRxBufferNum = 3; // Number of frames for mic buffer
    static constexpr const char* kWakeWordFileName = "/wakeword.bin";
    static constexpr const char* kSpiffsBasePath = ""; // Use root of SPIFFS

    int16_t* rawAudioBuffer; // Buffer to hold detected speech
    int16_t* micFrameBuffer; // Circular buffer for microphone frames

    // ns_handle_t nsInst; // Noise suppression instance (ESP32-S3 only)
    simplevox::VadEngine vadEngine;
    simplevox::MfccEngine mfccEngine;
    simplevox::MfccFeature* registeredWakeWord; // Stored wake word feature

    int16_t* readMicFrame();
};

#endif // WAKE_WORD_MANAGER_H
