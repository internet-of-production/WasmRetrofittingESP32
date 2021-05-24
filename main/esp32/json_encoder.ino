#include <ArduinoJson.h>

StaticJsonDocument<200> doc;
StaticJsonDocument<400> docConfig;

String jsonEncoder(const uint8_t * buf1, uint32_t len1, float value1, const uint8_t * buf2, uint32_t len2, float value2){
  
  
  int outlen1 = len1*4;
  int inlen1 = len1*2;
  byte out1[outlen1];

  int outlen2 = len2*4;
  int inlen2 = len2*2;
  byte out2[outlen2];
  
  UTF16toUTF8(out1, &outlen1, buf1, &inlen1);
  UTF16toUTF8(out2, &outlen2, buf2, &inlen2);
  
  char key1[len1 + 1];
  memcpy(key1, out1, len1);
  key1[len1] = 0; // Null termination.
  String keyString1 = String(key1);

  char key2[len2 + 1];
  memcpy(key2, out2, len2);
  key2[len2] = 0; // Null termination.
  String keyString2 = String(key2);

  String jsonResult;
  doc[keyString1] = value1;
  doc[keyString2] = value2;
  
  serializeJson(doc, jsonResult);
  //serializeJson(doc, Serial);

  //String jsonString = "{"+ key3 + ":" + String(value1) + "," + key2 + ":" + String(value2) + "}";
  return jsonResult;

}

JsonObject setConfJson(const uint8_t * buf, uint32_t len){

  int outlen = len*4;
  int inlen = len*2;
  byte out[outlen];

  UTF16toUTF8(out, &outlen, buf, &inlen);

  char inputCharArray[len + 1];
  memcpy(inputCharArray, out, len);
  inputCharArray[len] = 0; // Null termination.
  String input = String(inputCharArray);
  
  deserializeJson(docConfig, input);
  JsonObject obj = docConfig.as<JsonObject>();
  return obj;
  
  }
