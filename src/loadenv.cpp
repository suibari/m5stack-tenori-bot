#include <SPIFFS.h>
#include <unordered_map>
#include <string>

std::unordered_map<std::string, std::string> loadEnv(const std::string& filename) {
    std::unordered_map<std::string, std::string> env;

    File file = SPIFFS.open(filename.c_str(), "r");
    if (!file) {
        Serial.println("Failed to open .env file");
        return env;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim(); // 改行や余分な空白を削除

        if (line.length() == 0 || line.startsWith("#")) continue;

        int separatorIndex = line.indexOf('=');
        if (separatorIndex != -1) {
            String key = line.substring(0, separatorIndex);
            String value = line.substring(separatorIndex + 1);
            key.trim();
            value.trim();
            env[key.c_str()] = value.c_str();  // String -> std::string
        }
    }

    file.close();
    return env;
}
