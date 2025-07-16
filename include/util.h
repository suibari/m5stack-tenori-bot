#include <WiFiClientSecure.h>

bool sendChunk(WiFiClientSecure& client, const String& data);
String removeChunkHeaderFooter(const String& raw);
String readHttpBody(WiFiClient& client);
String urlEncode(const String& str);
