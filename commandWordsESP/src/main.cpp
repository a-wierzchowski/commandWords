#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include "driver/i2s.h"
#include "LittleFS.h"
#include "fvad.h"

// RGB
#define LED_PIN_BOARD 48
#define LED_COUNT 1

// PINS CONTROLS FROM COMMANDS
#define LIGHT_1 GPIO_NUM_2

// MICROPHONE INMP441
#define I2S_PORT_NUM I2S_NUM_1
#define I2S_WS_PIN GPIO_NUM_6
#define I2S_SCK_PIN GPIO_NUM_5
#define I2S_SD_PIN GPIO_NUM_7

#define I2S_BUFFER_SIZE 1920  // 480 (1 sample) * 4 (byte)
#define I2S_BUFFER_SIZE_SEND 5760 // 480(one sample) * 2 (byte) * 6 (frames)
#define SAMPLE_RATE 16000

// -------------GLOBAL VARIABLE----------------
unsigned long lastTime = 0;  // typ zmiennych bo taki zwraca millis(), timer jako delay ale bez freeza
bool switchWiFi = false;

// For RGB LED
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN_BOARD, NEO_BGR + NEO_KHZ800);
volatile int ledMode = 1; // 0 - constant, 1 - Blink, 2 - off
volatile uint32_t ledColor = 0x0000FFFF;; // Yellow

Fvad *vad = NULL; // Voice Activity detecton
// --------------PINS CONTROL----------------------------

void commandsPinout(int8_t arg){
  switch (arg)
  {
  case 1:
    Serial.print("Zapal diode\n");
    digitalWrite(LIGHT_1, HIGH);
    ledMode = 0;
    break;
  case 2:
    Serial.print("Zgas diode\n");
    digitalWrite(LIGHT_1, LOW);
    ledMode = 2;
    break;
  }
}

// -------------CONFIG WiFi----------------
String ssid = "Orange_Swiatlowod_7EE0";
String password = "tcN6HLNThXQ6nPtNR6";

void initWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("łączenie z siecią");
  while (WiFi.status() != WL_CONNECTED && !WiFi.localIP()){
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n====================");
  Serial.println("Połączono z siecią");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("====================");
}

// -------------CONFIG WEB_SOCKET----------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
  if(client != NULL){

    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("Klient podłączył się do webSocket: %u\n", client->id());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("Klient rozłączył się z webSocket: %u\n", client->id());
        break;
      case WS_EVT_DATA:{
        AwsFrameInfo *info = (AwsFrameInfo*) arg;
        if(info->final && info->index == 0 && info->len == len){ // whole message is in a single frame
          Serial.printf("Klient przesłał: %d o dlugosci: %zu\n", data[0], len);
          commandsPinout(data[0]);
        }
      }
      break;
    }
  }
}

void initWebSocket(){
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();
}


// -------------WEB PANEL----------------

void setupWebRequests(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/config/wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("SSID", true) && request->hasParam("PASS", true)){
      ssid = request->getParam("SSID", true)->value();
      password = request->getParam("PASS", true)->value();

      Serial.println("SSID: " + ssid);
      Serial.println("PASS: " + password);

      String json = "{\"status\":\"passed\"}";
      request->send(200, "application/json", json);
      
      lastTime = millis();
      switchWiFi = true;
      
    }
    else{
      Serial.println("Error przeslania ssid i pass: ");
      request->send(400, "text/plain", "Brak zmiennych");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Strona nie istnieje.");
  });

  
}

// -------------CONFIG MICROPHONE----------------
void setupI2S(){
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),  // steruje zetarem + RX czyli receive / odbieranie
    .sample_rate = SAMPLE_RATE,  // próbkowanie / 16kHz czyli 16 000 odczytów na sekundę
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 wysyła dane po 24 bity ale I2S czyta w blokach 32-bitowych
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Lewy kanał / L/R do GND
    .communication_format = I2S_COMM_FORMAT_STAND_I2S, // standard of Philips
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,  // ustawienie priorytetu dla przerwań / ważne ale np WiFi będzie ważniejsze
    .dma_buf_count = 8,  // 8 buforów  * 64 = 512 bajtów bezpośrednio do pamięci (dane) (DMA | Direct Memory Access)
    .dma_buf_len = 64,   // po 64 bajty
    .use_apll = false,   // APLL | Audio Phase-Locked Loop (precyzyjny zegar audio)
    .tx_desc_auto_clear = false, 
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE, // to jest do głośnika
    .data_in_num = I2S_SD_PIN
  };

  esp_err_t err;
  err = i2s_driver_install(I2S_PORT_NUM, &i2s_config, 0, NULL);
  if(err != ESP_OK){
    Serial.printf("Error driver install I2S: %d\n", err);
    for(;;);
  }
  err = i2s_set_pin(I2S_PORT_NUM, &pin_config);
  if (err != ESP_OK){
    Serial.printf("Check I2S pins setup: %d\n", err);
    for(;;);
  }
}
// read data from mic and send
void taskI2S(void *){
  //int32_t *i2s_32_buffer = (int32_t*)malloc(I2S_BUFFER_SIZE);    // for inmp441, 24 bit but we get 32 bit
  //int16_t *i2s_16_buffer = (int16_t*)malloc(I2S_BUFFER_SIZE / 2); // for vosk, only 16bit per sample


  int32_t *i2s_32_buffer = (int32_t*)ps_malloc(I2S_BUFFER_SIZE);    // switch to PSRAM
  int16_t *i2s_16_buffer = (int16_t*)ps_malloc(I2S_BUFFER_SIZE / 2); // switch to PSRAM
  int16_t *i2s_16_buffer_to_send = (int16_t*)ps_malloc(I2S_BUFFER_SIZE_SEND); 
  if(i2s_32_buffer == NULL || i2s_16_buffer == NULL || i2s_16_buffer_to_send == NULL){
    Serial.println("Error memory allocation bufor I2S");
    vTaskDelete(NULL);
    return;
  }

  size_t bytes_read;
  int test_counter = 0;
  int frame_counter = 0;
  int send_size=0;

  for(;;){
    esp_err_t result = i2s_read(I2S_PORT_NUM, i2s_32_buffer, I2S_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

    if(result == ESP_OK && bytes_read > 0){

      int sample_count = bytes_read / 4; // single sample from inmp441 = 4B (32 bit)

      for(int i=0; i<sample_count; i++){
        i2s_16_buffer[i] = (int16_t)(i2s_32_buffer[i] >> 16);  // 'convert' from 32 bit to 16bit by lose last 16 bit
      }

      int check_voice = fvad_process(vad, i2s_16_buffer, sample_count);
      if (check_voice == 1){
        //Serial.printf("Wykryto głos %d\n", test_counter++);
        frame_counter = 50;
      }
      else if(check_voice == -1)
        Serial.println("Invalid frame length");
      
      if (ws.count() > 0){
        if(frame_counter > 0){

          if (send_size + sample_count * 2 <= I2S_BUFFER_SIZE_SEND){ // *2 bcs we mind about size, where one sample = 2 bytes
            memcpy(&((uint8_t*)i2s_16_buffer_to_send)[send_size], i2s_16_buffer, sample_count * 2);
            send_size += sample_count * 2;
          }

          if(send_size >= I2S_BUFFER_SIZE_SEND){
            ws.binaryAll((uint8_t*)i2s_16_buffer_to_send, send_size); // accepts only char*, uint8_t*, String.  |  now, every sample have 2B (16 bit)
            send_size = 0;
            //Serial.printf("Przesłano %d\n", test_counter);
          }
            
          frame_counter--;
        }
        else if(send_size > 0){
          ws.binaryAll((uint8_t*)i2s_16_buffer_to_send, send_size); 
          send_size = 0;
          //Serial.printf("Przesłano koniec %d\n", test_counter);
        }
      }
      else{
          send_size = 0;
      }
    }
    else{
      Serial.printf("Error read I2S: %d\n", result);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1)); // access cpu for another tasks like wifi
  }

}

// ------CONFIG VAD(speach recognizer)--------
void initVAD(){
  vad = fvad_new();
  if(!vad){
    Serial.printf("Error VAD: out of memory\nRESTERT IN 5 SEC...");
    delay(5000);
    ESP.restart();
  }
  if(fvad_set_mode(vad, 3) < 0){
    Serial.printf("Error VAD: cannot set mode\nRESTERT IN 5 SEC...");
    delay(5000);
    ESP.restart();
  }
  if(fvad_set_sample_rate(vad, SAMPLE_RATE) < 0){
    Serial.printf("Error VAD: cannot set sample rate\nRESTERT IN 5 SEC...");
    delay(5000);
    ESP.restart();
  }
}


// --------------TASKS----------------------------
void taskBlink(void *){ // this task no need parametr

  /*
  ledMode:
          0 - Const power on
          2 - Blink
          2 - Power off
  */

  strip.begin();
  strip.setBrightness(25);
  bool ledState = 0;

  for(;;){
    if(ledMode == 0){
      strip.setPixelColor(0, ledColor);
      strip.show();

      vTaskDelay(100 / portTICK_PERIOD_MS);

    }
    else if (ledMode == 1){
      if(ledState)
        strip.setPixelColor(0, ledColor);
      else
        strip.setPixelColor(0, 0);
      strip.show();
      ledState = ledState == true ? false : true ;

      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else if (ledMode == 2){
      strip.setPixelColor(0, 0);
      strip.show();
    }
  }
}

void setup() {
  Serial.begin(115200);

  xTaskCreatePinnedToCore(taskBlink, "TaskBlink", 4096, NULL, 1, NULL, 0);

  // while (!Serial) {
  //   delay(10); // Wait for serial monitor, Only for test!!!!!!!!!
  //   Serial.printf(".");
  // }
  delay(2000);  // Wait for serial monitor, Only for test!!!!!!!!!
  Serial.printf("\n");

  // Setup for WiFi
  initWiFi();
  ledMode = 0;
  ledColor = 0x000000FF;

  if(!LittleFS.begin(true)){
    Serial.println("ERROR MOUNT FILE SYSTEM LittleFS");
    return;
  }

  // Setup for Website
  setupWebRequests();

  // Setup for webSocket
  initWebSocket();

  // Setup MIC
  setupI2S();
  xTaskCreatePinnedToCore(taskI2S, "TaskI2S", 20480, NULL, 1, NULL, 1);

  // Setup Voice Activity Detecion
  initVAD();

  // Setup pinout
  pinMode(LIGHT_1, OUTPUT);
}



void loop() {
  ws.cleanupClients();

  if(switchWiFi && millis() - lastTime  > 1500){ // To make sure that  we sent respone to website client (POST, JSON)
    switchWiFi = false;
    lastTime = 0;
    ws.closeAll();
    WiFi.disconnect();
    delay(500);
    initWiFi();
  }
  //sendDataByWebSocked();

  delay(10); // for RTOS to do tasks in background
}