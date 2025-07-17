#include <WiFiClientSecure.h>

bool sendChunk(WiFiClientSecure& client, const String& data);
void skipResponseHeaders(WiFiClient& client, unsigned long timeout_ms = 5000);
std::vector<uint8_t> readChunkedBody(WiFiClient& client, unsigned long timeout_ms = 30000);
std::vector<uint8_t> readBody(WiFiClient& client, unsigned long timeout_ms = 30000);
String urlEncode(const String& str);
