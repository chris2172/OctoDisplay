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
