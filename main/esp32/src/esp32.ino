#include "wasm3.h"
#include "m3_env.h"
#include "wasm3_defs.h"



#include <SPI.h>
//#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_wpa2.h>
//Web Server
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>

#include "app.wasm.h"
#include "Secret.h"

//setting for rs232 (max3323)
#include <ctype.h>

#define bit9600Delay 100  
#define halfBit9600Delay 50
#define bit4800Delay 188
#define halfBit4800Delay 94

byte rx = 6;
byte tx = 7;
byte SWval;

#define WASM_STACK_SLOTS    4000
#define INPUT_BYTE_LENGTH    9
#define SERIAL_SIZE_RX  256
#define INTERVAL 0
#define RTT_NUMBER 100 

// For (most) devices that cannot allocate a 64KiB wasm page
//#define WASM_MEMORY_LIMIT   4096

#define FATAL(func, msg) { Serial.print("Fatal: " func " "); Serial.println(msg); return; }

int UTF16toUTF8(unsigned char* out, int* outlen,
          const unsigned char* inb, int* inlenb);

void callback(char* topic, byte* payload, unsigned int length);

typedef struct{
  int number;
  float value;
  } axis_t;

AsyncWebServer server(80);


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


unsigned long prev, interval;
uint8_t testByteArray[INPUT_BYTE_LENGTH] = {3,0,0,0,32,128,44,128,0};//(DEC)8400000 = (HEX)802C80, (HEX)80=128, (HEX)2C=44
axis_t axis = {0,0.0};
int axisNumber = 0;
double distance = 0;
bool unsafe = false;
int JSONCounter = 0;
int arrival_counter = 0;

uint8_t newMACAddress[] = {0x24, 0x6f, 0x28, 0x24, 0xff, 0x60};

String mergedJsonString="{}";
JsonObject confJson;

const char *root_ca = ROOTCERT;

WiFiClientSecure wifiClient;
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
   

    String jsonString = jsonEncoder(buf, len, number, value);
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
  //const char* ssid = RWTH_ID;
  //const char* password = RWTH_PASS;
  
  client.setKeepAlive(10);

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  //esp_wifi_set_mac(WIFI_IF_STA, &newMACAddress[0]);
  
  WiFi.begin(ssid, password);
  //WiFi.begin(ssid);

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
      boolean success = client.subscribe("KUKAAxes/return");
      if(success){
        Serial.print("Subscribe successful");
        }
      
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
    unsigned long rtt_end = millis();
    //Serial.print("Message arrived in topic: ");
    //Serial.println(topic);

    if(arrival_counter < 100){
        char cstr[16];
        itoa(rtt_end, cstr, 10);
        client.publish("RTTEnd", cstr);
    }
    arrival_counter++;
    //char cstr[16];
    //itoa(rtt_end, cstr, 10);
    //client.publish("RTTEnd", cstr);
    
    
  }


void SWprint(int data)
{
  byte mask;
  //startbit
  digitalWrite(tx,LOW);
  delayMicroseconds(bit9600Delay);
  for (mask = 0x01; mask>0; mask <<= 1) {
    if (data & mask){ // choose bit
     digitalWrite(tx,HIGH); // send 1
    }
    else{
     digitalWrite(tx,LOW); // send 0
    }
    delayMicroseconds(bit9600Delay);
  }
  //stop bit
  digitalWrite(tx, HIGH);
  delayMicroseconds(bit9600Delay);
}

int SWread()
{
  byte val = 0;
  while (digitalRead(rx));
  //wait for start bit
  if (digitalRead(rx) == LOW) {
    delayMicroseconds(halfBit9600Delay);
    for (int offset = 0; offset < 8; offset++) {
     delayMicroseconds(bit9600Delay);
     val |= digitalRead(rx) << offset;
    }
    //wait for stop bit + extra
    delayMicroseconds(bit9600Delay);
    delayMicroseconds(bit9600Delay);
    return val;
  }
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!index) {
        Serial.println((String)"UploadStart: " + filename);
        // open the file on first call and store the file handle in the request object
        request->_tempFile = SPIFFS.open("/" + filename, "w");
    }
    if (len) {
        // stream the incoming chunk to the opened file
        request->_tempFile.write(data, len);
    }
    if (final) {
        Serial.println((String)"UploadEnd: " + filename + "," + index+len);
        // close the file handle as the upload is now done
        request->_tempFile.close();
        request->send(200, "text/plain", "File Uploaded !");
    }
}

void setup() {
  Serial.begin(9600);
  Serial.setRxBufferSize(SERIAL_SIZE_RX);

//rs232setting
  /*pinMode(rx,INPUT);
  pinMode(tx,OUTPUT);
  digitalWrite(tx,HIGH);
  delay(2);
  digitalWrite(13,HIGH); //turn on debugging LED
  SWprint('h');  //debugging hello
  SWprint('i');
  SWprint(10); //carriage return*/
  
  run_wasm(NULL);

  // https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/
  if(!SPIFFS.begin(true)){
  Serial.println("An Error has occurred while mounting SPIFFS");
  return;
 }
  
  setupWifi();

  printCurrentNet();
  printWifiData();

  printCurrentNet();
  printWifiData();
  //arguments: server(IPAddress, const char[ ]), port(int)
  //const char* mqtt_server = confJson["mqtt_server"];
  const char* mqtt_server = "192.168.178.34";
  //int port = confJson["mqtt_port"];
  int port = 8883;

  //setinterval
  prev = 0; 
  interval = INTERVAL;

  wifiClient.setCACert(root_ca);
  wifiClient.setInsecure();
  
  client.setServer(mqtt_server,port);
  client.setCallback(callback);

  if ( !MDNS.begin("esp32") ) {
    Serial.println( "Error setting up MDNS responder!" );
    while(1) {
        delay(1000);
    }
  }
  Serial.println( "mDNS responder started" );

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/ide.html");
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200);
      }, handleUpload);

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("ide.html");

  server.onNotFound([](AsyncWebServerRequest *request) {
      if (request->method() == HTTP_OPTIONS) {
          request->send(200);
      } else {
          Serial.println("Not found");
          request->send(404, "Not found");
      }
  });

  server.begin();

}


//i_argv is a const char* array. I have to check how one can insert a binary value in the variable.
void loop() {
  
  if (!client.connected()) {
    reconnect();
  }

  client.loop();//This should be called regularly to allow the client to process incoming messages and maintain its connection to the server.
  
  M3Result result = m3Err_none;

  unsigned long current = millis(); 
  byte inputBytes[INPUT_BYTE_LENGTH];

//read from rs232 (max3323 MC)
  SWval = SWread();

  if(Serial.available() && (current-prev) >= interval){
    // read the incoming bytes:
    int bufferLength = Serial.readBytes(inputBytes, INPUT_BYTE_LENGTH);

    //resetCounter and JSONString
    if(inputBytes[0]==1){
      mergedJsonString = "{}";
      JSONCounter = 0;
      }

    // prints the received data
    //Serial.println("reading data...");
    //pointers of arguments for dataProcessWasm
    const void *i_argptrs[INPUT_BYTE_LENGTH];

    for(int i=0; i<INPUT_BYTE_LENGTH ;i++){
      i_argptrs[i] = &inputBytes[i];
    }

    
    result = m3_Call(dataProcessWasm,INPUT_BYTE_LENGTH,i_argptrs);
    if(result){
      FATAL("m3_Call(dataProcessWasm):", result);
    }
    

    JSONCounter++;

    if(JSONCounter == 7){
      if(isJsonStrValid(mergedJsonString)){
        unsigned long rtt_end = millis();
        char cstr[16];
        itoa(rtt_end, cstr, 10);
        client.publish("RTTStart", cstr);
        client.publish("KUKA", mergedJsonString.c_str());
       
      }
      mergedJsonString = "{}";
      JSONCounter = 0;
    }

    /*
    unsigned long start = micros();
    
    result = m3_Call(setAxisData,INPUT_BYTE_LENGTH,i_argptrs);                       
    if(result){
      FATAL("m3_Call(setAxisData):", result);
    }

    result = m3_GetResultsV(setAxisData, &axisNumber);
      if(result){
      FATAL("m3_GetResultsV(setAxisData):", result);
      }

    unsigned long calc_time = micros()-start;
    char cstr[16];
    itoa(calc_time, cstr, 10);
    client.publish("CALC",cstr);
  
    //Serial.println(axisNumber);
    if(axisNumber == 7){    
      result = m3_Call(getDistance,0,NULL);                       
        if(result){
        FATAL("m3_CallV(getMinDistance):", result);
        }
       

      result = m3_GetResultsV(getDistance, &distance);
      if(result){
      FATAL("m3_GetResultsV(getDistance):", result);
      }

      //Serial.println(distance);
      char cstr[16];
      itoa(distance, cstr, 10);
      client.publish("KUKA", cstr);
    }*/

    //set time 

    /*if(SWval){
      char res[1];
      res[0] = (char)SWval;
      client.publish("KUKA", res);
      }*/
    prev = current; 
  
  }
   
}