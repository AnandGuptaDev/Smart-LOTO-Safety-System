#include "esp_camera.h"
#include <WiFi.h>
#include "SD_MMC.h"
#include "FS.h"
#include <U8g2lib.h>

#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

#define LED_GPIO_NUM 3
#define RELAY1_PIN 14
#define OLED_SDA 41
#define OLED_SCL 42

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

const char* ssid     = "*********";
const char* password = "*********";

void startCameraServer();
void setupLedFlash(int pin);

void initOLED() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, "System Starting...");
  u8g2.sendBuffer();
  Serial.println("OLED init OK");
}

void showOLED(const char* line1, const char* line2 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, line1);
  if (strlen(line2) > 0) u8g2.drawStr(0, 28, line2);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);
digitalWrite(RELAY1_PIN, HIGH); // relay OFF at start
  delay(1000);

  // Step 1: Camera first
  Serial.println("PSRAM check...");
  if (!psramFound()) {
    Serial.println("ERROR: PSRAM not found!");
    while(true) delay(1000);
  }
  Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());

  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sccb_sda  = SIOD_GPIO_NUM;
  config.pin_sccb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 10000000;
  config.pixel_format  = PIXFORMAT_JPEG;
  config.frame_size    = FRAMESIZE_QVGA;
  config.jpeg_quality  = 15;
  config.fb_count      = 1;
  config.grab_mode     = CAMERA_GRAB_LATEST;
  config.fb_location   = CAMERA_FB_IN_PSRAM;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    while(true) delay(1000);
  }
  Serial.println("Camera init OK");

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_vflip(s, 1);

  // Step 2: OLED after camera
  initOLED();
  showOLED("Camera OK!", "Mounting SD...");

  // Step 3: SD card
  Serial.println("Initializing SD card...");
  SD_MMC.setPins(39, 38, 40);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed!");
    showOLED("SD Mount FAILED!");
  } else {
    Serial.println("SD Card Mounted OK!");
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    showOLED("SD Card OK!");
  }

  // Step 4: WiFi
  showOLED("Connecting WiFi...");
  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setSleep(false);

  Serial.print("Connecting to WiFi");
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    dots++;
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, "Connecting WiFi...");
    // draw dots
    char dotStr[11] = "";
    int numDots = dots % 10;
    for (int i = 0; i < numDots; i++) dotStr[i] = '.';
    dotStr[numDots] = '\0';
    u8g2.drawStr(0, 28, dotStr);
    u8g2.sendBuffer();
  }
  Serial.println("\nWiFi connected");

  // Show IP address
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, "WiFi Connected!");
  u8g2.drawStr(0, 28, WiFi.localIP().toString().c_str());
  u8g2.sendBuffer();

  // Step 5: Start server
  setupLedFlash(LED_GPIO_NUM);
  startCameraServer();

  showOLED("System Ready!", "Look at camera");

  Serial.print("Camera Ready! Open: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(10000);
}
