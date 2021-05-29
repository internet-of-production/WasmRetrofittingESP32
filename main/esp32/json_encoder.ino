#include <ArduinoJson.h>

StaticJsonDocument<400> docConfig;

//Create Json: "{axis_N: value}"
String jsonEncoder(const uint8_t * buf, uint32_t len, uint32_t axisNumber, float axisValue){

  StaticJsonDocument<200> doc;
  
  int outlen = len*4;
  int inlen = len*2;
  byte out[outlen];
  
  UTF16toUTF8(out, &outlen, buf, &inlen);
  
  char key[len + 1];
  memcpy(key, out, len);
  key[len] = 0; // Null termination.
  String keyString = String(key)+String(axisNumber);


  String jsonResult;
  doc[keyString] = axisValue;
  
  serializeJson(doc, jsonResult);
  //serializeJson(doc, Serial);

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

String jsonMerge(String destString, String srcString)
{
   StaticJsonDocument<400> docDest;
   StaticJsonDocument<200> docSrc;
  
   deserializeJson(docDest,destString);
   deserializeJson(docSrc,srcString);

   JsonObject dest = docDest.as<JsonObject>();
   JsonObjectConst src = docSrc.as<JsonObject>();  
     
   for (auto item : src)
   {
     dest[item.key()] = item.value();
   }
   
   String res;
   serializeJson(docDest,res);

   return res;
}

bool isJsonStrValid(String src){
  
  Serial.println(src);
  StaticJsonDocument<400> docCheck;
 
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(docCheck,src);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  float value = docCheck["invalid0"];
  Serial.println(value);
  
  return value == 0; 
  }
