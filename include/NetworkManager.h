#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class NetworkManager {
private:
  HTTPClient http;
  uint8_t* responseBuffer;
  size_t responseSize;
  size_t responseCapacity;
  bool responseReady;
  
  // チャンクデータ処理用
  String currentChunk;
  bool isReceivingChunks;
  
public:
  NetworkManager();
  ~NetworkManager();
  
  bool connectWiFi(const char* ssid, const char* password);
  bool sendAudioData(uint8_t* audioData, size_t dataSize, const char* endpoint);
  bool isResponseReady();
  uint8_t* getResponseData();
  size_t getResponseSize();
  void initConversation();
  
private:
  bool sendPOSTRequest(const String& url, const String& jsonPayload);
  void processChunkedResponse();
  String encodeBase64(const uint8_t* data, size_t length);
  void allocateResponseBuffer(size_t size);
  void appendToResponseBuffer(const uint8_t* data, size_t size);
};

#endif
