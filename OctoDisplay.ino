/**************************CrowPanel ESP32 HMI Display Example Code************************
Version     :	1.1
Suitable for:	CrowPanel ESP32 HMI Display
Product link:	https://www.elecrow.com/esp32-display-series-hmi-touch-screen.html
Code	  link:	https://github.com/Elecrow-RD/CrowPanel-ESP32-Display-Course-File
Lesson	link:	https://www.youtube.com/watch?v=WHfPH-Kr9XU
Description	:	The code is currently available based on the course on YouTube, 
				        if you have any questions, please refer to the course video: Introduction 
				        to ask questions or feedback.
**************************************************************/


#include <Wire.h>
#include <SPI.h>
#include <lvgl.h>
#include "ui.h"
#include "gfx_conf.h"
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <WiFi.h>          // For WiFi functionality
#include <WebServer.h>     // For WebServer class
#include <Preferences.h>   // For Preferences (already present but ensure it's included)
#include <PubSubClient.h>  // MQTT Client

// Add these near the top
bool checkPreferences();
void startAPMode();
void connectToWiFi();
void updateConnectionStatus();
void handleSubmit();
void handleRoot();

// Add with other global variables
#define FACTORY_RESET_PIN 0  // GPIO0 = BOOT button
unsigned long buttonHoldStart = 0;
bool resetFlag = false;

WebServer server(80);
Preferences preferences;
bool setupMode = false;

// HTML configuration form
const char* configPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>OctoDisplay Setup</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #f4f4f4;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }
    .container {
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1);
      width: 300px;
      text-align: center;
    }
    input {
      width: 100%;
      padding: 10px;
      margin: 5px 0;
      border: 1px solid #ccc;
      border-radius: 5px;
    }
    input[type="submit"] {
      background-color: #007bff;
      color: white;
      cursor: pointer;
      border: none;
    }
    input[type="submit"]:hover {
      background-color: #0056b3;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>OctoDisplay Configuration</h2>
    <form action="/submit" method="post">
      <input type="text" name="ssid" placeholder="WiFi SSID" required><br>
      <input type="password" name="pass" placeholder="WiFi Password" required><br>
      <input type="text" name="ha_api" placeholder="Home Assistant API Key" required><br>
      <input type="text" name="ha_ip" placeholder="Home Assistant IP" required><br>
      <input type="text" name="mqtt_host" placeholder="MQTT Host" required><br>
      <input type="text" name="mqtt_user" placeholder="MQTT User" required><br>
      <input type="password" name="mqtt_pass" placeholder="MQTT Password" required><br>
      <input type="text" name="mqtt_port" placeholder="MQTT Port" required><br>
      <input type="submit" value="Save and Reboot">
    </form>
  </div>
</body>
</html>

)rawliteral";

void handleRoot() {
  server.send(200, "text/html", configPage);
}

static lv_disp_draw_buf_t draw_buf;
// Change from:
//static lv_color_t disp_draw_buf1[screenWidth * screenHeight / 10];
//static lv_color_t disp_draw_buf2[screenWidth * screenHeight / 10];

// To (1/4 screen buffer):
//static lv_color_t disp_draw_buf1[screenWidth * screenHeight / 4];
//static lv_color_t disp_draw_buf2[screenWidth * screenHeight / 4];

static lv_color_t disp_draw_buf1[screenWidth * 40]; // 40 rows
static lv_color_t disp_draw_buf2[screenWidth * 40];
static lv_disp_drv_t disp_drv;


/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
   uint32_t w = ( area->x2 - area->x1 + 1 );
   uint32_t h = ( area->y2 - area->y1 + 1 );

   tft.pushImageDMA(area->x1, area->y1, w, h,(lgfx::rgb565_t*)&color_p->full);

   lv_disp_flush_ready( disp );

}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
   uint16_t touchX, touchY;
   bool touched = tft.getTouch( &touchX, &touchY);
   if( !touched )
   {
      data->state = LV_INDEV_STATE_REL;
   }
   else
   {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touchX;
      data->point.y = touchY;

      Serial.print( "Data x " );
      Serial.println( touchX );

      Serial.print( "Data y " );
      Serial.println( touchY );
   }
}

bool checkPreferences() {
  return 
    preferences.isKey("ssid") &&       // WiFi SSID
    preferences.isKey("pass") &&       // WiFi Password
    preferences.isKey("ha_api") &&     // Home Assistant API
    preferences.isKey("ha_ip") &&      // Home Assistant IP
    preferences.isKey("mqtt_host") &&  // MQTT Host
    preferences.isKey("mqtt_user") &&  // MQTT User
    preferences.isKey("mqtt_pass") &&  // MQTT Password
    preferences.isKey("mqtt_port");    // MQTT Port (even with default)
}

// Add after Preferences declaration
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqtt_server;
uint16_t mqtt_port;
char mqtt_user[40];
char mqtt_pass[40];
void startAPMode() {
  WiFi.softAP("OctoDisplay", "");
  IPAddress IP = WiFi.softAPIP();
}

void connectToWiFi() {
  WiFi.begin(preferences.getString("ssid", "").c_str(), 
             preferences.getString("pass", "").c_str());
}

void handleSubmit() {
  preferences.putString("ssid", server.arg("ssid"));
  preferences.putString("pass", server.arg("pass"));
  preferences.putString("ha_api", server.arg("ha_api"));
  preferences.putString("ha_ip", server.arg("ha_ip"));
  preferences.putString("mqtt_host", server.arg("mqtt_host"));
  preferences.putString("mqtt_user", server.arg("mqtt_user"));
  preferences.putString("mqtt_pass", server.arg("mqtt_pass"));
  preferences.putUInt("mqtt_port", server.arg("mqtt_port").toInt());
  
  server.send(200, "text/plain", "Settings saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void setup()
{
  Serial.begin(115200);
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap()); // Add this line
  Serial.println("\nBooting...");
  WiFi.enableSTA(true);
  Serial.println("LVGL OctoDisplay");

  //GPIO init
#if defined (CrowPanel_50) || defined (CrowPanel_70)
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  pinMode(17, OUTPUT);
  digitalWrite(17, LOW);
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);
  pinMode(42, OUTPUT);
  digitalWrite(42, LOW);
#elif defined (CrowPanel_43)
  pinMode(20, OUTPUT);
  digitalWrite(20, LOW);
  pinMode(19, OUTPUT);
  digitalWrite(19, LOW);
  pinMode(35, OUTPUT);
  digitalWrite(35, LOW);
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  //pinMode(0, OUTPUT);//TOUCH-CS
#endif

  //Display Prepare
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  delay(200);

  lv_disp_drv_register(&disp_drv);
  lv_init();

  delay(100);

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, screenWidth * screenHeight/10);
  /* Initialize the display */
  lv_disp_drv_init(&disp_drv);
  /* Change the following line to your display resolution */
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.full_refresh = 1;
  disp_drv.draw_buf = &draw_buf;
 

  /* Initialize the (dummy) input device driver */
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  tft.fillScreen(TFT_BLACK);

  //please do not use LVGL Demo and UI export from Squareline Studio in the same time.
  // lv_demo_widgets();    // LVGL demo
  ui_init();
  
  preferences.begin("octodisplay", false);

  // Check if preferences exist
  if (!checkPreferences()) {
    setupMode = true;
    startAPMode();
    server.on("/", handleRoot);
    server.on("/submit", HTTP_POST, handleSubmit);
    server.begin();
    lv_label_set_text(ui_LabelURL, "http://192.168.4.1");
  } else {
    connectToWiFi();
    lv_disp_load_scr(ui_ScreenIHD);
  }

  Serial.println( "Setup done" );

}

void loop() {
  lv_timer_handler();

  // --- Factory Reset Check ---
  // Temporarily reconfigure GPIO0 for input
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
  bool buttonState = digitalRead(FACTORY_RESET_PIN);
  pinMode(FACTORY_RESET_PIN, OUTPUT);  // Restore original mode
  digitalWrite(FACTORY_RESET_PIN, LOW);// Restore original state

  if(buttonState == LOW) { // Button pressed (LOW)
    if(buttonHoldStart == 0) {
      buttonHoldStart = millis();
    } 
    else if(millis() - buttonHoldStart > 20000 && !resetFlag) {
      preferences.clear();
      delay(500);
      ESP.restart();
      resetFlag = true;
    }
  } 
  else {
    buttonHoldStart = 0;
    resetFlag = false;
  }
  // ---------------------------

  // --- Factory Reset Check ---
  // Only check if not in setup mode
  if(!setupMode) {
    static unsigned long lastCheck = 0;
    if(millis() - lastCheck > 100) { // Check every 100ms
      pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
      bool buttonState = digitalRead(FACTORY_RESET_PIN);
      pinMode(FACTORY_RESET_PIN, OUTPUT);
      digitalWrite(FACTORY_RESET_PIN, LOW);

      if(buttonState == LOW) {
        if(buttonHoldStart == 0) buttonHoldStart = millis();
        else if(millis() - buttonHoldStart > 20000 && !resetFlag) {
          preferences.clear();
          ESP.restart();
        }
      } else {
        buttonHoldStart = 0;
      }
      lastCheck = millis();
    }
  }
  delay(5);
}

// Add after setupMode declaration
bool mqttConnected = false;

// Add these topic definitions (modify as needed)
struct TopicMapping {
  const char* topic;
  lv_obj_t** label;
};

// Map topics to UI labels
TopicMapping topicMap[] = {
  {"elecrow/grid/power/state", &ui_GridPower},
  {"elecrow/grid/energy_out/state", &ui_GridEnergyOut},
  {"elecrow/grid/energy_in/state", &ui_GridEnergyIn}
};

void updateConnectionStatus() {
  static int retryCount = 0;
  const char* dots[] = {".", "..", "...", "...."};
  static bool wifiConnected = false;

  if(WiFi.status() == WL_CONNECTED) {
    if(!wifiConnected) {
      Serial.println("WiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      wifiConnected = true;
    }
    
    if(!mqttConnected) {
      Serial.println("Attempting MQTT connection...");
      connectToMQTT();
    }
    
    if(mqttConnected) {
      mqttClient.loop();
      lv_label_set_text_fmt(ui_Label6, "Connected\nMQTT: %s:%d", mqtt_server, mqtt_port);
      retryCount = 0;
    }
  } 
  else {
    wifiConnected = false;
    mqttConnected = false;
    lv_label_set_text_fmt(ui_Label6, "Connecting%s", dots[retryCount % 4]);
    
    if(retryCount % 5 == 0) {
      Serial.printf("WiFi connection attempt %d\n", retryCount/5 + 1);
    }
    
    if(retryCount++ > 50) { // ~25 second timeout
      Serial.println("Connection timeout! Rebooting...");
      ESP.restart();
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[20];
  snprintf(message, sizeof(message), "%.*s", length, payload);

  // Find matching label
  for(auto &mapping : topicMap) {
    if(strcmp(topic, mapping.topic) == 0) {
      lv_label_set_text(*mapping.label, message);
      break;
    }
  }
}

void connectToMQTT() {
  mqtt_server = preferences.getString("mqtt_host", "").c_str();
  mqtt_port = preferences.getUInt("mqtt_port", 1883);
  preferences.getString("mqtt_user", mqtt_user, 40);
  preferences.getString("mqtt_pass", mqtt_pass, 40);

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  
  Serial.print("Connecting to MQTT at ");
  Serial.print(mqtt_server);
  Serial.print(":");
  Serial.println(mqtt_port);

  if(mqttClient.connect("OctoDisplay", mqtt_user, mqtt_pass)) {
    Serial.println("MQTT connected!");
    for(auto &mapping : topicMap) {
      mqttClient.subscribe(mapping.topic);
      Serial.print("Subscribed to: ");
      Serial.println(mapping.topic);
    }
    mqttConnected = true;
  } else {
    Serial.print("MQTT connection failed, rc=");
    Serial.println(mqttClient.state());
  }
}


