#include "WakeWordManager.h"
#include "config.h"
#include <M5Core2.h>
#include <SPIFFS.h>

WakeWordManager::WakeWordManager()
    : rawAudioBuffer(nullptr),
      micFrameBuffer(nullptr),
      // nsInst(nullptr), // NS disabled
      registeredWakeWord(nullptr) {}

WakeWordManager::~WakeWordManager() {
    if (rawAudioBuffer) heap_caps_free(rawAudioBuffer);
    if (micFrameBuffer) heap_caps_free(micFrameBuffer);
    if (registeredWakeWord) delete registeredWakeWord;
    // if (nsInst) ns_pro_destroy(nsInst); // NS disabled
}

#include <driver/i2s.h>

// --- I2S Configuration ---
// Copied from the old VoiceDetector, as this is a known-good config for Core2
static i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = WakeWordManager::kSampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512, // Buffer size
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
};

static i2s_pin_config_t i2s_pin_config = {
    .bck_io_num = I2S_PIN_NO_CHANGE,
    .ws_io_num = 0,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = 34
};
// --- End I2S Configuration ---


bool WakeWordManager::init() {
    // This function now only initializes the engines and allocates memory.
    // I2S hardware is handled by startListening().

    // 1. Configure Engines from config.h
    auto vadConfig = vadEngine.config();
    vadConfig.sample_rate = kSampleRate;
    
    // --- VAD Sensitivity Diagnostics ---
    vadConfig.vad_mode = simplevox::VadMode::Aggression_LV2;
    vadConfig.decision_time_ms = 150;
    
    auto mfccConfig = mfccEngine.config();
    mfccConfig.sample_rate = kSampleRate;

    // 2. Allocate Memory
    constexpr uint32_t memCaps = (CONFIG_SPIRAM) ? (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM) : MALLOC_CAP_8BIT;
    rawAudioBuffer = (int16_t*)heap_caps_malloc(kAudioLength * sizeof(*rawAudioBuffer), memCaps);
    if (!rawAudioBuffer) {
        M5.Lcd.println("Failed to allocate rawAudioBuffer");
        return false;
    }

    micFrameBuffer = (int16_t*)heap_caps_malloc(kRxBufferNum * vadConfig.frame_length() * sizeof(*micFrameBuffer), MALLOC_CAP_8BIT);
    if (!micFrameBuffer) {
        M5.Lcd.println("Failed to allocate micFrameBuffer");
        return false;
    }

    // I2S hardware is initialized in startListening()

    // 4. Initialize Noise Suppression (Disabled for ESP32)
    /*
    nsInst = ns_pro_create(vadConfig.frame_time_ms, 1, vadConfig.sample_rate);
    if (nsInst == NULL) {
        M5.Lcd.println("Failed to init ns.");
        return false;
    }
    */

    // 5. Initialize Engines
    if (!vadEngine.init(vadConfig)) {
        M5.Lcd.println("Failed to init vad.");
        return false;
    }
    if (!mfccEngine.init(mfccConfig)) {
        M5.Lcd.println("Failed to init mfcc.");
        return false;
    }

    // 6. Initialize SPIFFS and load wake word
    if (!SPIFFS.begin(true)) {
        M5.Lcd.println("SPIFFS Mount Failed");
        return false;
    }

    String wakeWordPath = String(kSpiffsBasePath) + kWakeWordFileName;
    if (SPIFFS.exists(wakeWordPath)) {
        M5.Lcd.println("Wake word file exists. Loading...");
        if (registeredWakeWord) delete registeredWakeWord;
        registeredWakeWord = mfccEngine.loadFile(wakeWordPath.c_str());
        if (registeredWakeWord) {
            M5.Lcd.println("Wake word loaded.");
        } else {
            M5.Lcd.println("Failed to load wake word.");
        }
    } else {
        M5.Lcd.println("No wake word file found.");
    }

    return true;
}

void WakeWordManager::reset() {
    vadEngine.reset();
}

bool WakeWordManager::startListening() {
    Serial.println("Starting wake word listener (I2S)...");
    i2s_driver_uninstall(I2S_NUM_0); // Ensure clean state
    esp_err_t result = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (result != ESP_OK) {
        Serial.printf("I2S driver install failed: %d\n", result);
        return false;
    }
    result = i2s_set_pin(I2S_NUM_0, &i2s_pin_config);
    if (result != ESP_OK) {
        Serial.printf("I2S set pin failed: %d\n", result);
        return false;
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
    return true;
}

void WakeWordManager::stopListening() {
    Serial.println("Stopping wake word listener (I2S)...");
    i2s_driver_uninstall(I2S_NUM_0);
}

int16_t* WakeWordManager::readMicFrame() {
    static int rxIndex = 0;
    const int frameLength = vadEngine.config().frame_length();
    const int frameBytes = frameLength * sizeof(int16_t);
    size_t bytesRead = 0;

    // Read one frame's worth of data from I2S
    esp_err_t result = i2s_read(I2S_NUM_0, &micFrameBuffer[frameLength * rxIndex], frameBytes, &bytesRead, portMAX_DELAY);

    if (result != ESP_OK || bytesRead != frameBytes) {
        M5.Lcd.printf("i2s_read error: %d, bytes: %d\n", result, bytesRead);
        return nullptr;
    }

    // Return the buffer that was just filled
    return &micFrameBuffer[frameLength * rxIndex];
}

bool WakeWordManager::listenAndDetect() {
    if (!registeredWakeWord) {
        // No wake word registered, cannot detect.
        return false;
    }

    auto* frameData = readMicFrame();
    if (frameData == nullptr) {
        return false;
    }

    // Apply software gain
    const int frameLength = vadEngine.config().frame_length();
    int16_t* samples = frameData;
    for (size_t i = 0; i < frameLength; i++) {
        int32_t amplified_sample = (int32_t)samples[i] * SOFTWARE_GAIN;
        if (amplified_sample > 32767) amplified_sample = 32767;
        if (amplified_sample < -32768) amplified_sample = -32768;
        samples[i] = (int16_t)amplified_sample;
    }

    // Apply noise suppression (Disabled for ESP32)
    // ns_process(nsInst, frameData, frameData);

    // Perform VAD
    int detectedLength = vadEngine.detect(rawAudioBuffer, kAudioLength, frameData);
    if (detectedLength <= 0) {
        return false; // No speech detected yet
    }

    // Speech detected, now compare with wake word
    Serial.println("Speech detected, comparing...");
    std::unique_ptr<simplevox::MfccFeature> currentFeature(mfccEngine.create(rawAudioBuffer, detectedLength));
    
    if (!currentFeature) {
        M5.Lcd.println("MFCC creation failed.");
        vadEngine.reset();
        return false;
    }

    const auto dist = simplevox::calcDTW(*registeredWakeWord, *currentFeature);
    
    // Threshold for DTW distance needs tuning. Lower is better match.
    const int dtwThreshold = 180; 
    Serial.printf("DTW Distance: %6lu (Threshold: %d)\n", dist, dtwThreshold);

    vadEngine.reset();

    if (dist < dtwThreshold) {
        Serial.println(">>> WAKE WORD DETECTED! <<<");
        return true;
    }

    return false;
}

int WakeWordManager::captureAndRegisterWakeWord() {
    M5.Lcd.println("Listening for wake word...");

    // Start I2S listener specifically for registration
    if (!startListening()) {
        M5.Lcd.println("Failed to start listener for registration.");
        return 0;
    }

    // Loop until a single utterance is captured by VAD
    int detectedLength = 0;
    while (detectedLength <= 0) {
        M5.update(); // Update button states etc.
        // Button B check is now handled globally in main.cpp loop()

        auto* frameData = readMicFrame();
        if (frameData == nullptr) {
            continue;
        }
        
        // Apply software gain
        const int frameLength = vadEngine.config().frame_length();
        int16_t* samples = frameData;
        for (size_t i = 0; i < frameLength; i++) {
            int32_t amplified_sample = (int32_t)samples[i] * SOFTWARE_GAIN;
            if (amplified_sample > 32767) amplified_sample = 32767;
            if (amplified_sample < -32768) amplified_sample = -32768;
            samples[i] = (int16_t)amplified_sample;
        }

        // ns_process(nsInst, frameData, frameData); // NS disabled
        detectedLength = vadEngine.detect(rawAudioBuffer, kAudioLength, frameData);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    M5.Lcd.printf("Captured %d samples. Creating MFCC...\n", detectedLength);

    // Delete old feature if it exists
    if (registeredWakeWord != nullptr) {
        delete registeredWakeWord;
        registeredWakeWord = nullptr;
    }

    // Create new MFCC feature
    registeredWakeWord = mfccEngine.create(rawAudioBuffer, detectedLength);

    if (registeredWakeWord) {
        String wakeWordPath = String(kSpiffsBasePath) + kWakeWordFileName;
        if (mfccEngine.saveFile(wakeWordPath.c_str(), *registeredWakeWord)) {
            M5.Lcd.println("Wake word registered and saved!");
        } else {
            M5.Lcd.println("ERROR: Failed to save wake word!");
            delete registeredWakeWord;
            registeredWakeWord = nullptr;
        }
    } else {
        M5.Lcd.println("ERROR: MFCC creation failed!");
    }

    vadEngine.reset();
    stopListening(); // Stop listener after registration
    return detectedLength;
}
