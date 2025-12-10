#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "driver/i2s.h"

// -------------GLOBAL VARIABLE----------------
unsigned long lastTime = 0;  // typ zmiennych bo taki zwraca millis()

// -------------CONFIG WiFi----------------
const char* ssid = "Orange_Swiatlowod_7EE0";
const char* password = "tcN6HLNThXQ6nPtNR6";

void initWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("łączenie z siecią");
  while (WiFi.status() != WL_CONNECTED && !WiFi.localIP()){
    Serial.print(".");
    delay(1000);
  }
  Serial.println("====================");
  Serial.println("Połączono z siecią");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("====================");
}

// -------------CONFIG WEB_SOCKET----------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
  if (type == WS_EVT_CONNECT)
    Serial.printf("Klient podłączył się do webSocket: %u\n", client->id());
  else if (type == WS_EVT_DISCONNECT)
    Serial.printf("Klient rozłączył się do webSocket: %u\n", client->id());
}

void initWebSocket(){
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();
}

// -------------FUNCTION SEND DATA----------------
void sendDataByWebSocked(){
  static uint16_t counter = 1;
  // better delay - nie robi freeza 
  if(millis() - lastTime > 1000){
    lastTime = millis();
    
    Serial.printf("Przesłano wiadomość %d\n", counter);
    String messagesText = "Wiadomość" + String(counter);
    counter++;

    ws.textAll(messagesText);

  }
}

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    delay(10); // Wait for serial monitor, Only for test!!!!!!!!!
  }

  // Setup for WiFi
  initWiFi();
  // Setup for webSocket
  initWebSocket();

}

void loop() {
  ws.cleanupClients();


  sendDataByWebSocked();

  delay(10); // for RTOS to do tasks in background
}