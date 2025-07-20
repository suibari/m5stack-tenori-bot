#include <M5Core2.h>
#include "NetworkManager.h"
#include "config.h"
#include <base64.h>

NetworkManager::NetworkManager() {
  responseBuffer = nullptr;
  responseSize = 0;
  responseCapacity = 0;
  responseReady = false;
  isReceivingChunks = false;
  responseCode = 0;
}

NetworkManager::~NetworkManager() {
  if (responseBuffer) {
    free(responseBuffer);
  }
}

bool NetworkManager::connectWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    M5.Lcd.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    M5.Lcd.println("");
    M5.Lcd.println("WiFi connected");
    M5.Lcd.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    M5.Lcd.println("");
    M5.Lcd.println("WiFi connection failed");
    return false;
  }
}

bool NetworkManager::sendAudioData(uint8_t* audioData, size_t dataSize, const char* endpoint) {
  if (WiFi.status() != WL_CONNECTED) {
    M5.Lcd.println("WiFi not connected");
    return false;
  }
  
  // 前回のレスポンスバッファをクリア
  if (responseBuffer) {
    free(responseBuffer);
    responseBuffer = nullptr;
  }
  responseSize = 0;
  responseCapacity = 0;
  responseReady = false;
  
  // Base64エンコード
  String base64Audio = encodeBase64(audioData, dataSize);
  Serial.printf("base64AudioSize: %d\n", base64Audio.length());
  
  // JSON作成: JsonDocumentだとデータが大きすぎて入らない
  String jsonPayload = "{\"audio\":\"" + base64Audio + "\"}";
  
  // URL作成
  String url = String(SERVER_URL) + "/" + endpoint;
  
  // POST送信
  return sendPOSTRequest(url, jsonPayload);
}

bool NetworkManager::sendPOSTRequest(const String& url, const String& jsonPayload) {
  WiFiClient client;

  // エラー状態クリア
  hasErrorFlag = false;
  responseReady = false;
  responseCode = 0;

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000); // 30秒タイムアウト
  
  Serial.printf("Sending POST to: %s\n", url.c_str());
  Serial.printf("Payload size: %d bytes\n", jsonPayload.length());
  
  int httpResponseCode = http.POST(jsonPayload);
  responseCode = httpResponseCode;
  
  if (httpResponseCode == 200) {
    Serial.println("POST successful, processing response");
    processChunkedResponse();
    responseReady = true;
    return true;
  } else {
    Serial.printf("POST failed, error code: %d\n", httpResponseCode);
    hasErrorFlag = true;
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.printf("Error response: %s\n", response.c_str());
    }
    return false;
  }
}

void NetworkManager::processChunkedResponse() {
  WiFiClient* stream = http.getStreamPtr();

  allocateResponseBuffer(64 * 1024);

  while (true) {
    String chunkSizeLine = stream->readStringUntil('\n');
    chunkSizeLine.trim(); // 改行除去

    size_t chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);
    if (chunkSize == 0) break; // 終了チャンク

    while (chunkSize > 0) {
      size_t toRead = min(chunkSize, size_t(1024));
      uint8_t buffer[1024];
      size_t bytesRead = stream->readBytes(buffer, toRead);
      if (bytesRead > 0) {
        appendToResponseBuffer(buffer, bytesRead);
        chunkSize -= bytesRead;
      } else {
        break; // エラー
      }
    }

    // チャンク終端の \r\n を読み飛ばす
    stream->readStringUntil('\n');
  }

  http.end();
  Serial.printf("Total response size: %d bytes\n", responseSize);
}

bool NetworkManager::isResponseReady() {
  return responseReady;
}

uint8_t* NetworkManager::getResponseData() {
  return responseBuffer;
}

size_t NetworkManager::getResponseSize() {
  return responseSize;
}

String NetworkManager::encodeBase64(const uint8_t* data, size_t length) {
  return base64::encode(data, length);
}

void NetworkManager::allocateResponseBuffer(size_t size) {
  responseBuffer = (uint8_t*)malloc(size);
  if (responseBuffer) {
    responseCapacity = size;
    responseSize = 0;
  } else {
    Serial.println("Failed to allocate response buffer");
  }
}

void NetworkManager::appendToResponseBuffer(const uint8_t* data, size_t size) {
  if (!responseBuffer) return;
  
  // バッファサイズが不足する場合は拡張
  if (responseSize + size > responseCapacity) {
    size_t newCapacity = responseCapacity * 2;
    while (newCapacity < responseSize + size) {
      newCapacity *= 2;
    }
    
    uint8_t* newBuffer = (uint8_t*)realloc(responseBuffer, newCapacity);
    if (newBuffer) {
      responseBuffer = newBuffer;
      responseCapacity = newCapacity;
    } else {
      Serial.println("Failed to expand response buffer");
      return;
    }
  }
  
  memcpy(responseBuffer + responseSize, data, size);
  responseSize += size;
}

void NetworkManager::initConversation() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, String(SERVER_URL) + "/initConversation");
  int response = http.POST("");
  
  if (response == 200) {
    Serial.println("Conversation reset successfully.");
  } else {
    Serial.printf("Failed to reset conversation. HTTP code: %d\n", response);
  }

  http.end();
}

bool NetworkManager::hasError() {
  return hasErrorFlag;
}

void NetworkManager::clearError() {
  hasErrorFlag = false;
  responseCode = 0;
}
