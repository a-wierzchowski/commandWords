#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include "driver/i2s.h"

// RGB
#define LED_PIN_BOARD 48
#define LED_COUNT 1

// MICROPHONE INMP441
#define I2S_PORT_NUM I2S_NUM_1
#define I2S_WS_PIN GPIO_NUM_6
#define I2S_SCK_PIN GPIO_NUM_5
#define I2S_SD_PIN GPIO_NUM_7

#define I2S_BUFFER_SIZE 256

// -------------GLOBAL VARIABLE----------------
unsigned long lastTime = 0;  // typ zmiennych bo taki zwraca millis(), timer jako delay ale bez freeza

// For RGB LED
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN_BOARD, NEO_BGR + NEO_KHZ800);
volatile int ledMode = 1; // 0 - constant, 1 - Blink 
volatile uint32_t ledColor = 0x0000FFFF;; // Yellow



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

// -------------CONFIG MICROPHONE----------------
void setupI2S(){
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),  // steruje zetarem + RX czyli receive / odbieranie
    .sample_rate = 16000,  // próbkowanie / 16kHz czyli 16 000 odczytów na sekundę
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
  uint8_t *i2s_buffer = (uint8_t*)malloc(I2S_BUFFER_SIZE);
  if(i2s_buffer == NULL){
    Serial.println("Error memory allocation bufor I2S");
    vTaskDelete(NULL);
    return;
  }

  size_t bytes_read;

  for(;;){
    esp_err_t result = i2s_read(I2S_PORT_NUM, i2s_buffer, I2S_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

    if(result == ESP_OK && bytes_read > 0){
      ws.binaryAll(i2s_buffer, bytes_read);
    }
    else{
      Serial.printf("Error read I2S: %d\n", result);
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // access cpu for another tasks like wifi
  }

}

// -------------FUNCTION SEND DATA----------------
//                ONLY FOR TEST

// void sendDataByWebSocked(){
//   static uint16_t counter = 1;
//   // better delay - nie robi freeza 
//   if(millis() - lastTime > 1000){
//     lastTime = millis();
    
//     Serial.printf("Przesłano wiadomość %d\n", counter);
//     String messagesText = "Wiadomość" + String(counter);
//     counter++;

//     ws.textAll(messagesText);

//   }
// }


// --------------TASKS----------------------------
void taskBlink(void *){ // this task no need parametr

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
  }
}

void setup() {
  Serial.begin(115200);

  xTaskCreatePinnedToCore(taskBlink, "TaskBlink", 3000, NULL, 1, NULL, 0);

  while (!Serial) {
    delay(10); // Wait for serial monitor, Only for test!!!!!!!!!
  }

  // Setup for WiFi
  initWiFi();
  ledMode = 0;
  ledColor = 0x000000FF;

  // Setup for webSocket
  initWebSocket();

  // Setup MIC
  setupI2S();
  xTaskCreatePinnedToCore(taskI2S, "TaskI2S", 4096, NULL, 1, NULL, 0);

}

void loop() {
  ws.cleanupClients();

  //sendDataByWebSocked();

  delay(10); // for RTOS to do tasks in background
}