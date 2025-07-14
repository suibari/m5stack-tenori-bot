#include <WiFiClientSecure.h>

bool sendChunk(WiFiClientSecure& client, const String& data);
String removeChunkHeaderFooter(const String& raw);
String urlEncode(const String& str);
