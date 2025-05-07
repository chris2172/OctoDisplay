I purchased this display https://www.elecrow.com/esp32-display-4-3-inch-hmi-display-rgb-tft-lcd-touch-screen.html
A Crowpanel 4.3" display and was intending to install OpenHasp on it, but unfortunatly it did not work.

So I thought what I could do with it & decided why not get AI to program it for me.
Using https://copilot.microsoft.com/ I asked

Question 1:
This Project is for a Crowpanel 4.3" Display I want to add functionality that if the following setting values are not available 
Wifi SSID: 
Wifi Password: 
Home Assistant API: 
Home Assistant IP: 
MQTT Host: 
MQTT User: 
MQTT Password: 
MQTT Port: Should be pre defined as the default MQTT Port 
The device publishes its SSID as OctoDisplay with no password Display ui_ScreenSetup and replace "text" on LabelURL 
with the URL to go to And allow the user to connect to that SSID and go to a web page, and set those settings. 
By hitting Submit, the device will reboot, log on to the SSID provided and show the Wifi Connection status on the screen ui_ScreenIHD Label6
```C
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include <Wire.h>
#include <SPI.h>
#include <lvgl.h>
#include "ui.h"
#include "gfx_conf.h"

// Existing display buffers and driver declarations
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf1[screenWidth * screenHeight / 10];
static lv_color_t disp_draw_buf2[screenWidth * screenHeight / 10];
static lv_disp_drv_t disp_drv;

// For nonvolatile storage (ESP32)
Preferences preferences;

// Default MQTT port (as a string for storage; you could also store it as an int)
const String defaultMQTTPort = "1883";

// Web server running on port 80
WebServer server(80);

// Flag to tell whether we need configuration
bool configMissing = false;

// ---------------------------------------------------------------------------
// 1. Function to load settings from nonvolatile storage.
//    If any required value is missing, we mark configMissing true.
// ---------------------------------------------------------------------------
void loadConfigurations() {
    preferences.begin("config", false);
    String wifiSSID = preferences.getString("wifiSSID", "");
    String wifiPass = preferences.getString("wifiPass", "");
    String haAPI    = preferences.getString("haAPI", "");
    String haIP     = preferences.getString("haIP", "");
    String mqttHost = preferences.getString("mqttHost", "");
    String mqttUser = preferences.getString("mqttUser", "");
    String mqttPass = preferences.getString("mqttPass", "");
    String mqttPort = preferences.getString("mqttPort", "");
    
    // Check if essential values are provided (except mqttPort, which gets a default)
    if (wifiSSID == "" || wifiPass == "" || haAPI == "" || haIP == "" ||
        mqttHost == "" || mqttUser == "" || mqttPass == "") {
        configMissing = true;
    }

    // If MQTT Port is not set, save the default
    if (mqttPort == "") {
        preferences.putString("mqttPort", defaultMQTTPort);
    }
    preferences.end();
}

// ---------------------------------------------------------------------------
// 2. Webserver handlers for the configuration portal
// ---------------------------------------------------------------------------
void handleRoot() {
    // Generate a simple HTML form with fields for each setting.
    String page = "<html><head><title>Device Configuration</title></head><body>";
    page += "<h1>Configure Your Device</h1>";
    page += "<form method='POST' action='/save'>";
    page += "Wifi SSID: <input type='text' name='wifiSSID'><br><br>";
    page += "Wifi Password: <input type='password' name='wifiPass'><br><br>";
    page += "Home Assistant API: <input type='text' name='haAPI'><br><br>";
    page += "Home Assistant IP: <input type='text' name='haIP'><br><br>";
    page += "MQTT Host: <input type='text' name='mqttHost'><br><br>";
    page += "MQTT User: <input type='text' name='mqttUser'><br><br>";
    page += "MQTT Password: <input type='password' name='mqttPass'><br><br>";
    page += "MQTT Port: <input type='text' name='mqttPort' placeholder='1883'><br><br>";
    page += "<input type='submit' value='Submit'>";
    page += "</form></body></html>";
    server.send(200, "text/html", page);
}

void handleSave() {
    if (server.method() == HTTP_POST) {
        preferences.begin("config", false);
        // Save the values from the form submission
        preferences.putString("wifiSSID", server.arg("wifiSSID"));
        preferences.putString("wifiPass", server.arg("wifiPass"));
        preferences.putString("haAPI", server.arg("haAPI"));
        preferences.putString("haIP", server.arg("haIP"));
        preferences.putString("mqttHost", server.arg("mqttHost"));
        preferences.putString("mqttUser", server.arg("mqttUser"));
        preferences.putString("mqttPass", server.arg("mqttPass"));
        
        String port = server.arg("mqttPort");
        if (port == "") {
            port = defaultMQTTPort;
        }
        preferences.putString("mqttPort", port);
        preferences.end();
        
        // Inform the user and then restart
        server.send(200, "text/html", "Settings saved. The device will now restart.");
        delay(2000);
        ESP.restart();
    }
}

// ---------------------------------------------------------------------------
// 3. Function to start the configuration portal: 
//    sets up an access point and the web server.
// ---------------------------------------------------------------------------
void startConfigPortal() {
    // Start Wi-Fi in Access Point mode with no password.
    WiFi.softAP("OctoDisplay");
    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP started. Connect to AP IP: ");
    Serial.println(ip);

    // Update the display's LabelURL on ui_ScreenSetup with the AP IP.
    // (Assuming you have a function or method to update that label.)
    // For example:
    // ui_LabelURL->setText(ip.toString().c_str());

    // Set web server routes.
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.println("Configuration web server started.");
}

// ---------------------------------------------------------------------------
// 4. Display and Touch callbacks (same as your current code)
// ---------------------------------------------------------------------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY);
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
        Serial.print("Data x ");
        Serial.println(touchX);
        Serial.print("Data y ");
        Serial.println(touchY);
    }
}

// ---------------------------------------------------------------------------
// 5. Setup: initialize display, load configuration, and either launch
//    the config portal or proceed to connect to Wi-Fi.
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("Starting CrowPanel ESP32 Display Project");

    // GPIO initialization as per your board (CrowPanel 4.3" example)
    #if defined (CrowPanel_43)
      pinMode(20, OUTPUT);
      digitalWrite(20, LOW);
      pinMode(19, OUTPUT);
      digitalWrite(19, LOW);
      pinMode(35, OUTPUT);
      digitalWrite(35, LOW);
      pinMode(38, OUTPUT);
      digitalWrite(38, LOW);
      pinMode(0, OUTPUT);  // TOUCH-CS
    #endif

    // Display initialization (your existing code)
    tft.begin();
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    delay(200);

    lv_init();
    delay(100);
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, screenWidth * screenHeight / 10);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.full_refresh = 1;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    tft.fillScreen(TFT_BLACK);

    // Initialize UI (this sets up your default screen(s))
    ui_init();

    // Load configuration settings from nonvolatile storage.
    loadConfigurations();

    if (configMissing) {
        Serial.println("Configuration missing. Launching config portal mode.");
        // Switch the display to the setup screen.
        // For example, if you have a function to change screens:
        // showScreen(ui_ScreenSetup);
        startConfigPortal();
    } else {
        // If all required settings are present, attempt to connect to Wi-Fi.
        preferences.begin("config", false);
        String ssid = preferences.getString("wifiSSID", "");
        String pass = preferences.getString("wifiPass", "");
        preferences.end();

        Serial.print("Connecting to WiFi: ");
        Serial.println(ssid);
        WiFi.begin(ssid.c_str(), pass.c_str());
    
        int counter = 0;
        while (WiFi.status() != WL_CONNECTED && counter < 40) {  // wait up to 20 seconds
            delay(500);
            Serial.print(".");
            counter++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi Connected!");
            // Switch to the normal operational screen.
            // For example:
            // showScreen(ui_ScreenIHD);
            // And update Label6 with a positive connection status:
            // ui_Label6->setText("Connected.");
        } else {
            Serial.println("\nWiFi connection failed.");
            // Consider reverting to configuration mode or displaying an error.
        }
    }
    Serial.println("Setup done.");
}

// ---------------------------------------------------------------------------
// 6. Loop: handle LVGL tasks and (if in config mode) web server requests.
// ---------------------------------------------------------------------------
void loop() {
    lv_timer_handler();
    // If the device is in configuration mode, keep handling web server requests.
    if (configMissing) {
        server.handleClient();
    }
    delay(5);
}
```

Question 2:
Please change this HTML Layout to look more profesional, remembering that it is on an ESP32-S3-Wroom without any internet access
```C
String page = "<!DOCTYPE html>"
              "<html lang='en'>"
              "<head>"
              "  <meta charset='UTF-8'>"
              "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
              "  <title>Device Configuration</title>"
              "  <style>"
              "    body {"
              "      font-family: Arial, sans-serif;"
              "      background-color: #f7f7f7;"
              "      margin: 0;"
              "      padding: 0;"
              "    }"
              "    .container {"
              "      max-width: 600px;"
              "      margin: 50px auto;"
              "      background: #fff;"
              "      padding: 30px;"
              "      border-radius: 8px;"
              "      box-shadow: 0 2px 8px rgba(0,0,0,0.1);"
              "    }"
              "    h1 {"
              "      text-align: center;"
              "      color: #333;"
              "      margin-bottom: 30px;"
              "    }"
              "    form {"
              "      display: flex;"
              "      flex-direction: column;"
              "    }"
              "    label {"
              "      margin: 10px 0 5px;"
              "      font-weight: bold;"
              "    }"
              "    input[type='text'],"
              "    input[type='password'] {"
              "      padding: 10px;"
              "      border: 1px solid #ccc;"
              "      border-radius: 4px;"
              "      font-size: 16px;"
              "    }"
              "    input[type='submit'] {"
              "      margin-top: 20px;"
              "      padding: 10px;"
              "      background-color: #4CAF50;"
              "      border: none;"
              "      border-radius: 4px;"
              "      color: #fff;"
              "      font-size: 16px;"
              "      cursor: pointer;"
              "    }"
              "    input[type='submit']:hover {"
              "      background-color: #45a049;"
              "    }"
              "  </style>"
              "</head>"
              "<body>"
              "  <div class='container'>"
              "    <h1>Configure Your Device</h1>"
              "    <form method='POST' action='/save'>"
              "      <label for='wifiSSID'>Wifi SSID:</label>"
              "      <input type='text' id='wifiSSID' name='wifiSSID'>"
              "      <label for='wifiPass'>Wifi Password:</label>"
              "      <input type='password' id='wifiPass' name='wifiPass'>"
              "      <label for='haAPI'>Home Assistant API:</label>"
              "      <input type='text' id='haAPI' name='haAPI'>"
              "      <label for='haIP'>Home Assistant IP:</label>"
              "      <input type='text' id='haIP' name='haIP'>"
              "      <label for='mqttHost'>MQTT Host:</label>"
              "      <input type='text' id='mqttHost' name='mqttHost'>"
              "      <label for='mqttUser'>MQTT User:</label>"
              "      <input type='text' id='mqttUser' name='mqttUser'>"
              "      <label for='mqttPass'>MQTT Password:</label>"
              "      <input type='password' id='mqttPass' name='mqttPass'>"
              "      <label for='mqttPort'>MQTT Port:</label>"
              "      <input type='text' id='mqttPort' name='mqttPort' placeholder='1883'>"
              "      <input type='submit' value='Submit'>"
              "    </form>"
              "  </div>"
              "</body>"
              "</html>";

```
Question 3:
That worked, but I was thinking how do I re test it. 
So I was thinking if I hold the boot button for 20 Seconds I want it to clear all the settings then reboot 
All the files can be found https://github.com/chris2172/OctoDisplay
```C
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <SPI.h>
#include <lvgl.h>
#include "ui.h"
#include "gfx_conf.h"

// Existing display buffers and driver declarations
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf1[screenWidth * screenHeight / 10];
static lv_color_t disp_draw_buf2[screenWidth * screenHeight / 10];
static lv_disp_drv_t disp_drv;

// For nonvolatile storage (ESP32)
Preferences preferences;

// Default MQTT port
const String defaultMQTTPort = "1883";

// Web server running on port 80
WebServer server(80);

// Flag to tell whether configuration is missing
bool configMissing = false;

// ---------------------------------------------------
// Button reset configuration settings variables
// ---------------------------------------------------
const int resetButtonPin = 0;  // Change this if needed.
unsigned long buttonPressStart = 0;
const unsigned long holdDuration = 20000; // 20,000 milliseconds = 20 seconds

// ---------------------------------------------------
// 1. Load configuration and flag missing settings
// ---------------------------------------------------
void loadConfigurations() {
    preferences.begin("config", false);
    String wifiSSID = preferences.getString("wifiSSID", "");
    String wifiPass = preferences.getString("wifiPass", "");
    String haAPI    = preferences.getString("haAPI", "");
    String haIP     = preferences.getString("haIP", "");
    String mqttHost = preferences.getString("mqttHost", "");
    String mqttUser = preferences.getString("mqttUser", "");
    String mqttPass = preferences.getString("mqttPass", "");
    String mqttPort = preferences.getString("mqttPort", "");
    
    // Check essential values (MQTT port gets a default if missing)
    if (wifiSSID == "" || wifiPass == "" || haAPI == "" || haIP == "" ||
        mqttHost == "" || mqttUser == "" || mqttPass == "") {
        configMissing = true;
    }
    
    if (mqttPort == "") {
        preferences.putString("mqttPort", defaultMQTTPort);
    }
    preferences.end();
}

// ---------------------------------------------------
// 2. Webserver handlers for the configuration portal
// ---------------------------------------------------
void handleRoot() {
    String page = "<!DOCTYPE html>"
                  "<html lang='en'>"
                  "<head>"
                  "  <meta charset='UTF-8'>"
                  "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "  <title>Device Configuration</title>"
                  "  <style>"
                  "    body { font-family: Arial, sans-serif; background-color: #f7f7f7; margin:0; padding:0; }"
                  "    .container { max-width: 600px; margin: 50px auto; background: #fff; padding: 30px; border-radius: 8px;"
                  "                 box-shadow: 0 2px 8px rgba(0,0,0,0.1); }"
                  "    h1 { text-align: center; color: #333; margin-bottom: 30px; }"
                  "    form { display: flex; flex-direction: column; }"
                  "    label { margin: 10px 0 5px; font-weight: bold; }"
                  "    input[type='text'], input[type='password'] { padding: 10px; border: 1px solid #ccc;"
                  "                 border-radius: 4px; font-size: 16px; }"
                  "    input[type='submit'] { margin-top: 20px; padding: 10px; background-color: #4CAF50; "
                  "                 border: none; border-radius: 4px; color: #fff; font-size: 16px; cursor: pointer; }"
                  "    input[type='submit']:hover { background-color: #45a049; }"
                  "  </style>"
                  "</head>"
                  "<body>"
                  "  <div class='container'>"
                  "    <h1>Configure Your Device</h1>"
                  "    <form method='POST' action='/save'>"
                  "      <label for='wifiSSID'>Wifi SSID:</label>"
                  "      <input type='text' id='wifiSSID' name='wifiSSID'>"
                  "      <label for='wifiPass'>Wifi Password:</label>"
                  "      <input type='password' id='wifiPass' name='wifiPass'>"
                  "      <label for='haAPI'>Home Assistant API:</label>"
                  "      <input type='text' id='haAPI' name='haAPI'>"
                  "      <label for='haIP'>Home Assistant IP:</label>"
                  "      <input type='text' id='haIP' name='haIP'>"
                  "      <label for='mqttHost'>MQTT Host:</label>"
                  "      <input type='text' id='mqttHost' name='mqttHost'>"
                  "      <label for='mqttUser'>MQTT User:</label>"
                  "      <input type='text' id='mqttUser' name='mqttUser'>"
                  "      <label for='mqttPass'>MQTT Password:</label>"
                  "      <input type='password' id='mqttPass' name='mqttPass'>"
                  "      <label for='mqttPort'>MQTT Port:</label>"
                  "      <input type='text' id='mqttPort' name='mqttPort' placeholder='1883'>"
                  "      <input type='submit' value='Submit'>"
                  "    </form>"
                  "  </div>"
                  "</body>"
                  "</html>";
    server.send(200, "text/html", page);
}

void handleSave() {
    if (server.method() == HTTP_POST) {
        preferences.begin("config", false);
        preferences.putString("wifiSSID", server.arg("wifiSSID"));
        preferences.putString("wifiPass", server.arg("wifiPass"));
        preferences.putString("haAPI", server.arg("haAPI"));
        preferences.putString("haIP", server.arg("haIP"));
        preferences.putString("mqttHost", server.arg("mqttHost"));
        preferences.putString("mqttUser", server.arg("mqttUser"));
        preferences.putString("mqttPass", server.arg("mqttPass"));
        
        String port = server.arg("mqttPort");
        if (port == "") {
            port = defaultMQTTPort;
        }
        preferences.putString("mqttPort", port);
        preferences.end();
        
        server.send(200, "text/html", "Settings saved. The device will now restart.");
        delay(2000);
        ESP.restart();
    }
}

void startConfigPortal() {
    WiFi.softAP("OctoDisplay");
    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP started. Connect to AP IP: "); Serial.println(ip);
    
    // Update the setup screen’s LabelURL if needed:
    // For example: ui_LabelURL->setText(ip.toString().c_str());
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.println("Configuration web server started.");
}

// ---------------------------------------------------
// 3. Display and touch callback functions (unchanged)
// ---------------------------------------------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY);
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
        Serial.print("Data x ");
        Serial.println(touchX);
        Serial.print("Data y ");
        Serial.println(touchY);
    }
}

// ---------------------------------------------------
// 4. Setup: initialize hardware, display, configuration and WiFi logic
// ---------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("Starting CrowPanel ESP32 Display Project");
    
    // Configure the reset (boot) button pin.
    // NOTE: Confirm that this pin doesn't conflict with other functions (e.g., touchscreen).
    pinMode(resetButtonPin, INPUT_PULLUP);
    
    // Board-specific GPIO initialization (example for CrowPanel 4.3")
    #if defined (CrowPanel_43)
      pinMode(20, OUTPUT);
      digitalWrite(20, LOW);
      pinMode(19, OUTPUT);
      digitalWrite(19, LOW);
      pinMode(35, OUTPUT);
      digitalWrite(35, LOW);
      pinMode(38, OUTPUT);
      digitalWrite(38, LOW);
      // The following line was originally for TOUCH-CS. Adjust as needed.
      // pinMode(0, OUTPUT); // Already used as resetButtonPin in this example.
    #endif

    tft.begin();
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    delay(200);

    lv_init();
    delay(100);
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, screenWidth * screenHeight / 10);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.full_refresh = 1;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    tft.fillScreen(TFT_BLACK);
    ui_init();

    loadConfigurations();

    if (configMissing) {
        Serial.println("Configuration missing. Launching config portal mode.");
        // Optionally switch display to ui_ScreenSetup here.
        startConfigPortal();
    } else {
        // If configuration exists, connect to WiFi.
        preferences.begin("config", false);
        String ssid = preferences.getString("wifiSSID", "");
        String pass = preferences.getString("wifiPass", "");
        preferences.end();
    
        Serial.print("Connecting to WiFi: "); Serial.println(ssid);
        WiFi.begin(ssid.c_str(), pass.c_str());
    
        int counter = 0;
        while (WiFi.status() != WL_CONNECTED && counter < 40) {  // wait up to ~20 seconds
            delay(500);
            Serial.print(".");
            counter++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi Connected!");
            // Optionally switch display to ui_ScreenIHD and update status Label6.
        } else {
            Serial.println("\nWiFi connection failed.");
            // Handle failed connection appropriately.
        }
    }
    Serial.println("Setup done.");
}

// ---------------------------------------------------
// 5. Loop: LVGL tasks, config portal handling and reset check.
// ---------------------------------------------------
void loop() {
    lv_timer_handler();
    
    // Check if the reset button is held for 20 seconds
    if (digitalRead(resetButtonPin) == LOW) {  // Assuming active LOW (pressed)
        if (buttonPressStart == 0) {
            buttonPressStart = millis();  // Start timing the press
        } else {
            // If pressed continuously for 20 seconds, clear settings
            if (millis() - buttonPressStart >= holdDuration) {
                Serial.println("Boot button held for 20 seconds. Clearing configuration...");
                preferences.begin("config", false);
                preferences.clear();
                preferences.end();
                delay(1000);
                ESP.restart();
            }
        }
    } else {
        // Button released; reset the timer
        buttonPressStart = 0;
    }
    
    // If in configuration portal mode, handle web server client requests.
    if (configMissing) {
        server.handleClient();
    }
    
    delay(5);
}

```

Question 4:
Server Provided in the settings.

I need the values from the following MQTT Topics
elecrow/grid/power/state
elecrow/grid/energy_in/state
elecrow/grid/energy_out/state
elecrow/grid/price_in/state
elecrow/grid/price_out/state

This project will have many more in the following format
elecrow/pv
elecrow/load
elecrow/battery
elecrow/inverter
elecrow/car

All with sub topics so I need the code to be easily editable to add additional topics

```C
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SPI.h>
#include <lvgl.h>
#include "ui.h"
#include "gfx_conf.h"

// -----------------------------
// Display-related declarations
// -----------------------------
static lv_disp_draw_buf_t draw_buf;
static lv_color_t disp_draw_buf1[screenWidth * screenHeight / 10];
static lv_color_t disp_draw_buf2[screenWidth * screenHeight / 10];
static lv_disp_drv_t disp_drv;

// -----------------------------
// Nonvolatile storage and web server
// -----------------------------
Preferences preferences;
const String defaultMQTTPort = "1883";
WebServer server(80);
bool configMissing = false;

// -----------------------------
// Reset button settings
// -----------------------------
const int resetButtonPin = 0;   // Choose appropriate pin (make sure it does not conflict)
unsigned long buttonPressStart = 0;
const unsigned long holdDuration = 20000;  // 20 seconds

// -----------------------------
// MQTT declarations
// -----------------------------
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// List your MQTT topics here. Additional topics and subtopics can be added to this list.
const char* mqttTopics[] = {
  "elecrow/grid/power/state",
  "elecrow/grid/energy_in/state",
  "elecrow/grid/energy_out/state",
  "elecrow/grid/price_in/state",
  "elecrow/grid/price_out/state",
  // You can easily add more topics under different branches:
  // "elecrow/pv/state", "elecrow/load/state", etc.
};
const int numTopics = sizeof(mqttTopics) / sizeof(mqttTopics[0]);

// MQTT callback: handles inbound messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received on topic [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Example: Update display or global variables based on topic
  // if (String(topic) == "elecrow/grid/power/state") {
  //     ui_LabelPower->setText(message.c_str());
  // }
  // Add similar conditional blocks for other topics (or use a lookup table)
}

// Attempt an MQTT connection and subscribe to the topics.
void mqttConnect() {
  // The client ID can be set to any unique string (or consider using a unique value)
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32DisplayClient")) {
      Serial.println(" connected.");
      // Subscribe to every topic in the list.
      for (int i = 0; i < numTopics; i++) {
        if (mqttClient.subscribe(mqttTopics[i])) {
          Serial.print("Subscribed to: ");
          Serial.println(mqttTopics[i]);
        } else {
          Serial.print("Failed subscription: ");
          Serial.println(mqttTopics[i]);
        }
      }
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println("; try again in 5 seconds");
      delay(5000);
    }
  }
}

// -----------------------------
// Configuration Portal Web Handlers
// -----------------------------
void handleRoot() {
    String page = "<!DOCTYPE html>"
                  "<html lang='en'>"
                  "<head>"
                  "  <meta charset='UTF-8'>"
                  "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "  <title>Device Configuration</title>"
                  "  <style>"
                  "    body { font-family: Arial, sans-serif; background-color: #f7f7f7; margin:0; padding:0; }"
                  "    .container { max-width: 600px; margin: 50px auto; background: #fff; padding: 30px; border-radius: 8px;"
                  "                 box-shadow: 0 2px 8px rgba(0,0,0,0.1); }"
                  "    h1 { text-align: center; color: #333; margin-bottom: 30px; }"
                  "    form { display: flex; flex-direction: column; }"
                  "    label { margin: 10px 0 5px; font-weight: bold; }"
                  "    input[type='text'], input[type='password'] { padding: 10px; border: 1px solid #ccc;"
                  "                 border-radius: 4px; font-size: 16px; }"
                  "    input[type='submit'] { margin-top: 20px; padding: 10px; background-color: #4CAF50; "
                  "                 border: none; border-radius: 4px; color: #fff; font-size: 16px; cursor: pointer; }"
                  "    input[type='submit']:hover { background-color: #45a049; }"
                  "  </style>"
                  "</head>"
                  "<body>"
                  "  <div class='container'>"
                  "    <h1>Configure Your Device</h1>"
                  "    <form method='POST' action='/save'>"
                  "      <label for='wifiSSID'>Wifi SSID:</label>"
                  "      <input type='text' id='wifiSSID' name='wifiSSID'>"
                  "      <label for='wifiPass'>Wifi Password:</label>"
                  "      <input type='password' id='wifiPass' name='wifiPass'>"
                  "      <label for='haAPI'>Home Assistant API:</label>"
                  "      <input type='text' id='haAPI' name='haAPI'>"
                  "      <label for='haIP'>Home Assistant IP:</label>"
                  "      <input type='text' id='haIP' name='haIP'>"
                  "      <label for='mqttHost'>MQTT Host:</label>"
                  "      <input type='text' id='mqttHost' name='mqttHost'>"
                  "      <label for='mqttUser'>MQTT User:</label>"
                  "      <input type='text' id='mqttUser' name='mqttUser'>"
                  "      <label for='mqttPass'>MQTT Password:</label>"
                  "      <input type='password' id='mqttPass' name='mqttPass'>"
                  "      <label for='mqttPort'>MQTT Port:</label>"
                  "      <input type='text' id='mqttPort' name='mqttPort' placeholder='1883'>"
                  "      <input type='submit' value='Submit'>"
                  "    </form>"
                  "  </div>"
                  "</body>"
                  "</html>";
    server.send(200, "text/html", page);
}

void handleSave() {
    if (server.method() == HTTP_POST) {
        preferences.begin("config", false);
        // Save all configuration values
        preferences.putString("wifiSSID", server.arg("wifiSSID"));
        preferences.putString("wifiPass", server.arg("wifiPass"));
        preferences.putString("haAPI", server.arg("haAPI"));
        preferences.putString("haIP", server.arg("haIP"));
        preferences.putString("mqttHost", server.arg("mqttHost"));
        preferences.putString("mqttUser", server.arg("mqttUser"));
        preferences.putString("mqttPass", server.arg("mqttPass"));

        String port = server.arg("mqttPort");
        if (port == "") {
            port = defaultMQTTPort;
        }
        preferences.putString("mqttPort", port);
        preferences.end();

        server.send(200, "text/html", "Settings saved. The device will now restart.");
        delay(2000);
        ESP.restart();
    }
}

void startConfigPortal() {
    WiFi.softAP("OctoDisplay");
    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP started. Connect to AP IP: ");
    Serial.println(ip);

    // Update the display’s setup screen LabelURL if needed:
    // ui_LabelURL->setText(ip.toString().c_str());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.println("Configuration web server started.");
}

// -----------------------------
// LVGL Display and Touch functions (as before)
// -----------------------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.pushImageDMA(area->x1, area->y1, w, h, (lgfx::rgb565_t*)&color_p->full);
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY);
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
        Serial.print("Data x ");
        Serial.println(touchX);
        Serial.print("Data y ");
        Serial.println(touchY);
    }
}

// -----------------------------
// Load configurations and mark as missing if needed.
// -----------------------------
void loadConfigurations() {
    preferences.begin("config", false);
    String wifiSSID = preferences.getString("wifiSSID", "");
    String wifiPass = preferences.getString("wifiPass", "");
    String haAPI    = preferences.getString("haAPI", "");
    String haIP     = preferences.getString("haIP", "");
    String mqttHost = preferences.getString("mqttHost", "");
    String mqttUser = preferences.getString("mqttUser", "");
    String mqttPass = preferences.getString("mqttPass", "");
    String mqttPort = preferences.getString("mqttPort", "");
    
    // Check for essential values
    if (wifiSSID == "" || wifiPass == "" || haAPI == "" || haIP == "" ||
        mqtt
```
