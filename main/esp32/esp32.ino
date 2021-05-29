#include "wasm3.h"
#include "m3_env.h"
#include "m3_api_defs.h"

#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "app.wasm.h"

#define WASM_STACK_SLOTS    4000
#define INPUT_BYTE_LENGTH    9

// For (most) devices that cannot allocate a 64KiB wasm page
//#define WASM_MEMORY_LIMIT   4096

#define FATAL(func, msg) { Serial.print("Fatal: " func " "); Serial.println(msg); return; }

int UTF16toUTF8(unsigned char* out, int* outlen,
          const unsigned char* inb, int* inlenb);

typedef struct{
  int number;
  float value;
  } axis_t;


IM3Environment env;
IM3Runtime runtime;
IM3Module module;
IM3Function dataProcessWasm;
IM3Function setAxisData;
IM3Function getDistance;
IM3Function isUnsafe;
IM3Function initAxesCoordinates;
IM3Function setNetConfigJson;
//IM3Function add; //add function is declared in Wasm Module.

uint8_t testByteArray[INPUT_BYTE_LENGTH] = {3,0,0,0,32,128,44,128,0};//(DEC)8400000 = (HEX)802C80, (HEX)80=128, (HEX)2C=44
axis_t axis = {0,0.0};
byte inputBytes[INPUT_BYTE_LENGTH];
int axisNumber = 0;
double distance = 0;
bool unsafe = false;
int JSONCounter = 0;

String jsonString;
String mergedJsonString="{}";
JsonObject confJson;

WiFiClient wifiClient;
PubSubClient client(wifiClient);


/*This function is called in Wasm module*/
void setAxis(int number, float value){
  axis.number = number;
  axis.value = value;
  }

void showArrayRaw(float v1,float v2,float v3,float v4){
  Serial.print(v1);
  Serial.print(", ");
  Serial.print(v2);
  Serial.print(", ");
  Serial.print(v3);
  Serial.print(", ");
  Serial.print(v4);
  Serial.println(", ");
  }


m3ApiRawFunction(m3_arduino_printUTF16)
{
    m3ApiGetArgMem  (const uint8_t *, buf)
    m3ApiGetArg     (uint32_t,        len)

    int outlen = len*4;
    int inlen = len*2;
    byte out[outlen];
    UTF16toUTF8(out, &outlen, buf, &inlen);

    Serial.write(out, outlen);
    client.publish("KUKA",out, outlen);
    m3ApiSuccess();
}

m3ApiRawFunction(m3_arduino_jsonEncoder)
{
    m3ApiGetArgMem  (const uint8_t *, buf)
    m3ApiGetArg     (uint32_t,        len)
    m3ApiGetArg     (uint32_t,        number)
    m3ApiGetArg     (float,        value)
   

    jsonString = jsonEncoder(buf, len, number, value);
    mergedJsonString = jsonMerge(mergedJsonString,jsonString);
    
    
    m3ApiSuccess();
}

m3ApiRawFunction(m3_arduino_setConfJson)
{
    m3ApiGetArgMem  (const uint8_t *, buf)
    m3ApiGetArg     (uint32_t,        len)

    confJson = setConfJson(buf, len);
    
    m3ApiSuccess();
}

m3ApiRawFunction(m3_arduino_setaxis)
{
    m3ApiGetArg     (uint32_t, number)
    m3ApiGetArg     (float, value)

    setAxis(number, value);

    m3ApiSuccess();
}

m3ApiRawFunction(m3_arduino_showArrayRaw)
{
    m3ApiGetArg     (float, v1)
    m3ApiGetArg     (float, v2)
    m3ApiGetArg     (float, v3)
    m3ApiGetArg     (float, v4)

    showArrayRaw(v1,v2,v3,v4);

    m3ApiSuccess();
}


M3Result LinkArduino(IM3Runtime runtime) {
    IM3Module module = runtime->modules;
    const char* arduino = "arduino";

    m3_LinkRawFunction (module, arduino, "printUTF16", "v(*i)", &m3_arduino_printUTF16);
    m3_LinkRawFunction (module, arduino, "jsonEncoder", "v(*iif)", &m3_arduino_jsonEncoder);
    m3_LinkRawFunction (module, arduino, "setConfJson", "v(*i)", &m3_arduino_setConfJson);
    m3_LinkRawFunction(module, arduino, "setAxis", "v(if)", &m3_arduino_setaxis);
    m3_LinkRawFunction(module, arduino, "showArrayRaw", "v(ffff)", &m3_arduino_showArrayRaw);

    return m3Err_none;
}

/*setup of the wasm module*/
static void run_wasm(void*)
{
  M3Result result = m3Err_none;

//it warks also without using variable
  //uint8_t* wasm = (uint8_t*)build_app_wasm;
  //uint32_t fsize = build_app_wasm_len;

  env = m3_NewEnvironment ();
  if (!env) FATAL("NewEnvironment", "failed");

  runtime = m3_NewRuntime (env, WASM_STACK_SLOTS, NULL);
   if (!runtime) FATAL("m3_NewRuntime", "failed");

#ifdef WASM_MEMORY_LIMIT
    runtime->memoryLimit = WASM_MEMORY_LIMIT;
#endif

   result = m3_ParseModule (env, &module, build_app_wasm, build_app_wasm_len);
   if (result) FATAL("m3_ParseModule", result);

   result = m3_LoadModule (runtime, module);
   if (result) FATAL("m3_LoadModule", result);

   // link
   result = LinkArduino (runtime);
   if (result) FATAL("LinkArduino", result);


   result = m3_FindFunction (&initAxesCoordinates, runtime, "initAxesCoordinates");
   if (result) FATAL("m3_FindFunction(initAxesCoordinates)", result);

   result = m3_FindFunction (&dataProcessWasm, runtime, "dataProcessWasm");
   if (result) FATAL("m3_FindFunction(dataProcessWasm)", result);

   result = m3_FindFunction (&setAxisData, runtime, "setAxisData");
   if (result) FATAL("m3_FindFunction(setAxisData)", result);

   
   result = m3_FindFunction (&getDistance, runtime, "getDistance");
   if (result) FATAL("m3_FindFunction(getDistance)", result);
/*
   result = m3_FindFunction (&isUnsafe, runtime, "isUnsafe");
   if (result) FATAL("m3_FindFunction(isUnsafe)", result);*/

   result = m3_Call(initAxesCoordinates,0,NULL);                       
        if(result){
        FATAL("m3_Call(initAxesCoordinates):", result);
        }

   result = m3_FindFunction (&setNetConfigJson, runtime, "setNetConfigJson");
   if (result) FATAL("m3_FindFunction(setNetConfigJson)", result);

   result = m3_Call(setNetConfigJson,0,NULL);                       
        if(result){
        FATAL("m3_Call(setNetConfigJson):", result);
        }

   Serial.println("Running WebAssembly...");

  
  }

//MQTT Client
//Add your MQTT Broker IP address, example:
/*
const char* mqtt_server = "MQTT_BROKER_IP_ADDRESS";
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mqttUser = "BROKER_USER_NAME";
const char* mqttPassword = "BROKER_PASSWORD";
int status = WL_IDLE_STATUS;*/

int status = WL_IDLE_STATUS;


 void setupWifi() {

  const char* ssid = confJson["ssid"];
  const char* password = confJson["password"];
  
  client.setKeepAlive(60);

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (status != WL_CONNECTED) {
    delay(500);
    Serial.print(status);
    status = WiFi.status();
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

 
void reconnect() {

  const char* mqttUser = confJson["mqtt_user"];
  const char* mqttPassword = confJson["mqtt_password"];

  Serial.println(mqttUser);
  Serial.println(mqttPassword);
  

  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("KUKA", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void printWifiData(){
  
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  }

void printCurrentNet(){

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  //print the received signal strength
  long rssi = WiFi.RSSI();
  //Serial.print("signal strength (RSSI):");
  //Serial.println(rssi);

  }

void callback(char* topic, byte* payload, unsigned int length){
    
    }


void setup() {
  Serial.begin(9600);

  run_wasm(NULL);
  
  setupWifi();

  printCurrentNet();
  printWifiData();

  printCurrentNet();
  printWifiData();
  //arguments: server(IPAddress, const char[ ]), port(int)

  const char* mqtt_server = confJson["mqtt_server"];
  int port = confJson["mqtt_port"];
  client.setServer(mqtt_server,port);
  client.setCallback(callback);    
}


//i_argv is a const char* array. I have to check how one can insert a binary value in the variable.
void loop() {
  
  if (!client.connected()) {
    reconnect();
  }
  //printCurrentNet();
  
  M3Result result = m3Err_none;

  if (Serial.available() > 0) {
    // read the incoming bytes:
    int bufferLength = Serial.readBytes(inputBytes, INPUT_BYTE_LENGTH);

    // prints the received data
    //Serial.println("reading data...");
    //pointers of arguments for dataProcessWasm
    const void *i_argptrs[INPUT_BYTE_LENGTH];

    for(int i=0; i<INPUT_BYTE_LENGTH ;i++){
      i_argptrs[i] = &inputBytes[i];
    }

    /*result = m3_Call(dataProcessWasm,INPUT_BYTE_LENGTH,i_argptrs);
    if(result){
      FATAL("m3_Call(dataProcessWasm):", result);
    }

    JSONCounter++;

    if(JSONCounter == 7){
      if(isJsonStrValid(mergedJsonString)){
        client.publish("KUKA", mergedJsonString.c_str());
      }
      mergedJsonString = "{}";
      JSONCounter = 0;
    }*/
    
    result = m3_Call(setAxisData,INPUT_BYTE_LENGTH,i_argptrs);                       
    if(result){
      FATAL("m3_Call(setAxisData):", result);
    }

    result = m3_GetResultsV(setAxisData, &axisNumber);
      if(result){
      FATAL("m3_GetResultsV(setAxisData):", result);
      }
  

    if(axisNumber == 7){    
      result = m3_Call(getDistance,0,NULL);                       
        if(result){
        FATAL("m3_CallV(getMinDistance):", result);
        }
       

      result = m3_GetResultsV(getDistance, &distance);
      if(result){
      FATAL("m3_GetResultsV(getDistance):", result);
      }

      Serial.println(distance);
      /*char cstr[16];
      itoa(distance, cstr, 10);
      client.publish("KUKA", cstr);*/
    }

  
  }
    
    delay(100);
   
}
