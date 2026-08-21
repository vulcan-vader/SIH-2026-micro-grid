#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_INA3221.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- NETWORK CONFIGURATION ---
const char* ssid = "Airtel_juli_0293";
const char* password = "Air@89883";
const char* serverName = "http://192.168.1.5:8000/sensor"; // Replace X with server IP

const unsigned long sendInterval = 3000; // Send telemetry every 3 seconds
unsigned long lastSendTime = 0;
unsigned long lastDisplayTime = 0;
bool forceDisplayUpdate = true; // Forces immediate update on button press


// --- PIN DEFINITIONS ---
#define I2C_SDA        21
#define I2C_SCL        22

#define Mains_Relay    32 // Controls the Main input line [HIGH- Mains Disconnected | LOW- Connected]
#define Backup_Relay   33 // Controls the Backup battery pwr line [HIGH- Battery connected | LOW - Isolated]
#define Motor_Relay    26 // Represents high variable loads like EV charger [HIGH- ON | LOW- OFF]
#define Lights_Relay   25 // Represents normal contant loads [HIGH- Normal Lights | LOW- Emergency Lights]
#define USB_Relay      27 // Represents low occasional loads [HIGH- Off | LOW- ON]
#define Charge_Relay   17 // Represents backup battery charging [HIGH- ON | LOW- OFF]

#define LM35_TEMP      34 // Senses temperature of Battery
#define RAIN           35 // Senses Rain

#define SCROLL_SW1        18 // Next button for display
#define SELECT_SW2       19 // Previous button for display

// --- I2C ADDRESSES ---
#define INA1_ADDR      0x40 // Monitors Mains, Battery Charger, Motor [CH1,CH2,CH3]
#define INA2_ADDR      0x41 // Monitors Lights, USB Charger, Backup Battery [CH!,CH2,CH3]
#define OLED_ADDR      0x3C

// --- MENU STATE VARIABLES ---
int lastState1 = HIGH;    // Default HIGH because of INPUT_PULLUP
int currentState1 = HIGH;
int lastState2 = HIGH;
int currentState2 = HIGH;
int currentPage = 0;      // Keeps track of the current OLED page (0 to 6)

// --- OBJECT INSTANTIATIONS ---
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
Adafruit_INA3221 ina1;
Adafruit_INA3221 ina2;

// --- FUNCTION PROTOTYPES ---
float readTemperatureLM35();

// ==================== DATA STRUCTURE ====================
struct SystemData {
  // Relay States
  bool mains_relay;
  bool backup_relay;
  bool motor_relay;
  bool lights_relay;
  bool usb_relay;
  bool charge_relay;
  
  // Sensors
  float temperature;
  bool raining;
  
  // INA1 (0x40) - Mains, Charger, Motor
  float mains_v, mains_i, mains_p;
  float charge_v, charge_i, charge_p;
  float motor_v, motor_i, motor_p;
  
  // INA2 (0x41) - Lights, USB, Backup
  float lights_v, lights_i, lights_p;
  float usb_v, usb_i, usb_p;
  float backup_v, backup_i, backup_p;
};

// ==================== WEATHER DATA (as strings for OLED) ====================
String weatherUV = "--";
String weatherRain = "--";
String weatherClouds = "--";

SystemData systemData = {0};

static unsigned long lastWeatherFetch = 0;
const unsigned long weatherInterval = 10000;  // 10 sec

// ==================== FUNCTION TO READ ALL DATA ====================
void readAllSystemData() {
  // Read Relay States
  systemData.mains_relay = (digitalRead(Mains_Relay) == LOW);    // LOW = Connected
  systemData.backup_relay = (digitalRead(Backup_Relay) == HIGH);  // HIGH = Connected
  systemData.motor_relay = (digitalRead(Motor_Relay) == HIGH);    // HIGH = ON
  systemData.lights_relay = (digitalRead(Lights_Relay) == HIGH);  // HIGH = Normal Lights ON
  systemData.usb_relay = (digitalRead(USB_Relay) == LOW);         // LOW = ON
  systemData.charge_relay = (digitalRead(Charge_Relay) == HIGH);  // HIGH = ON
  
  // Read Sensors
  systemData.temperature = readTemperatureLM35();
  systemData.raining = (digitalRead(RAIN) == LOW);  // LOW = Raining
  
  // Read INA1 (0x40)
  systemData.mains_v = ina1.getBusVoltage(0);
  systemData.mains_i = ina1.getCurrentAmps(0) * 1000;  // Convert to mA
  systemData.mains_p = systemData.mains_v * systemData.mains_i / 1000;
  
  systemData.charge_v = ina1.getBusVoltage(1);
  systemData.charge_i = ina1.getCurrentAmps(1) * 1000;
  systemData.charge_p = systemData.charge_v * systemData.charge_i / 1000;
  
  systemData.motor_v = ina1.getBusVoltage(2);
  systemData.motor_i = ina1.getCurrentAmps(2) * 1000;
  systemData.motor_p = systemData.motor_v * systemData.motor_i / 1000;
  
  // Read INA2 (0x41)
  systemData.lights_v = ina2.getBusVoltage(0);
  systemData.lights_i = ina2.getCurrentAmps(0) * 1000;
  systemData.lights_p = systemData.lights_v * systemData.lights_i / 1000;
  
  systemData.usb_v = ina2.getBusVoltage(1);
  systemData.usb_i = ina2.getCurrentAmps(1) * 1000;
  systemData.usb_p = systemData.usb_v * systemData.usb_i / 1000;
  
  systemData.backup_v = ina2.getBusVoltage(2);
  systemData.backup_i = ina2.getCurrentAmps(2) * 1000;
  systemData.backup_p = systemData.backup_v * systemData.backup_i / 1000;
}

// ==================== FUNCTION TO BUILD JSON PAYLOAD ====================
String buildSystemJSON() {
  String json = "{";
  
  // Relay States (as L1, L2, L3, L4 for your dashboard)
  json += "\"l1\":" + String(systemData.motor_relay ? "true" : "false") + ",";      // L1: Variable High (Motor)
  json += "\"l2\":" + String(systemData.lights_relay ? "true" : "false") + ",";    // L2: Normal Constant (Lights)
  json += "\"l3\":" + String(systemData.usb_relay ? "false" : "true") + ",";       // L3: Occasional (USB)
  json += "\"l4\":" + String(systemData.charge_relay ? "true" : "false") + ",";    // L4: Charger
  
  // Sensors
  json += "\"temp\":" + String(systemData.temperature, 2) + ",";
  json += "\"raining\":" + String(systemData.raining ? "true" : "false") + ",";
  
  // Mains
  json += "\"mains\":{";
  json += "\"v\":" + String(systemData.mains_v, 2) + ",";
  json += "\"i\":" + String(systemData.mains_i, 2) + ",";
  json += "\"p\":" + String(systemData.mains_p, 2);
  json += "},";
  
  // Charger
  json += "\"charger\":{";
  json += "\"v\":" + String(systemData.charge_v, 2) + ",";
  json += "\"i\":" + String(systemData.charge_i, 2) + ",";
  json += "\"p\":" + String(systemData.charge_p, 2);
  json += "},";
  
  // Motor
  json += "\"motor\":{";
  json += "\"v\":" + String(systemData.motor_v, 2) + ",";
  json += "\"i\":" + String(systemData.motor_i, 2) + ",";
  json += "\"p\":" + String(systemData.motor_p, 2);
  json += "},";
  
  // Lights
  json += "\"lights\":{";
  json += "\"v\":" + String(systemData.lights_v, 2) + ",";
  json += "\"i\":" + String(systemData.lights_i, 2) + ",";
  json += "\"p\":" + String(systemData.lights_p, 2);
  json += "},";
  
  // USB
  json += "\"usb\":{";
  json += "\"v\":" + String(systemData.usb_v, 2) + ",";
  json += "\"i\":" + String(systemData.usb_i, 2) + ",";
  json += "\"p\":" + String(systemData.usb_p, 2);
  json += "},";
  
  // Backup
  json += "\"backup\":{";
  json += "\"v\":" + String(systemData.backup_v, 2) + ",";
  json += "\"i\":" + String(systemData.backup_i, 2) + ",";
  json += "\"p\":" + String(systemData.backup_p, 2);
  json += "}";
  
  json += "}";
  return json;
}

// ==================== HTTP SENDING FUNCTION ====================
void sendTelemetryHTTP(String jsonPayload) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Connect to your laptop server
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    
    // Send POST request with JSON payload
    int httpResponseCode = http.POST(jsonPayload);
    
    // Debug output
    if (httpResponseCode > 0) {
      Serial.printf("[HTTP] POST Success! Response Code: %d\n", httpResponseCode);
      
      // Optional: Read response from server
      String response = http.getString();
      Serial.printf("[HTTP] Response: %s\n", response.c_str());
      
    } else {
      Serial.printf("[HTTP] POST Failed! Error Code: %d\n", httpResponseCode);
    }
    
    http.end();
    
  } else {
    Serial.println("[HTTP] WiFi Disconnected. Cannot send data.");
  }
}


void setup() {
  Serial.begin(115200);
  
  // 1. Configure Relay Outputs (Ensuring Safe LOW Startup)
  pinMode(Mains_Relay, OUTPUT);
  pinMode(Backup_Relay, OUTPUT);
  pinMode(Motor_Relay, OUTPUT); 
  pinMode(Lights_Relay, OUTPUT);
  pinMode(USB_Relay, OUTPUT);   
  pinMode(Charge_Relay, OUTPUT);

  digitalWrite(Mains_Relay, LOW);
  digitalWrite(Backup_Relay, LOW);
  digitalWrite(Motor_Relay, LOW); 
  digitalWrite(Lights_Relay, LOW);
  digitalWrite(USB_Relay, LOW);   
  digitalWrite(Charge_Relay, LOW);

  // 2. Configure Inputs
  pinMode(SCROLL_SW1, INPUT_PULLUP);
  pinMode(SELECT_SW2, INPUT_PULLUP);
  pinMode(LM35_TEMP, INPUT);
  pinMode(RAIN, INPUT);

  // 3. Initialize I2C Bus
  Wire.begin(I2C_SDA, I2C_SCL);

  // 4. Initialize OLED
  if (!display.begin(OLED_ADDR, true)) {
    Serial.println(F("OLED Initialization Failed!"));
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("System Initializing..."));

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(40, 31);
    display.println(F("POWER GRID"));
    display.display();

    
  }

  // 5. Initialize INA3221 Sensors
  if (!ina1.begin(INA1_ADDR, &Wire)) {
    Serial.println(F("INA3221 #1 (0x40) Not Found!"));
  }
  if (!ina2.begin(INA2_ADDR, &Wire)) {
    Serial.println(F("INA3221 #2 (0x41) Not Found!"));
  }

  // 6. Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) { 
    delay(500); 
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  }
  else {
    Serial.println("\nWi-Fi Connection Failed! Continuing offline...");
  }

  delay(1000);
}

void loop() {

  // --- BUTTON STATE & PAGE SWITCHING ---
  currentState1 = digitalRead(SCROLL_SW1);
  currentState2 = digitalRead(SELECT_SW2);

  // Check if SCROLL_SW1 was just pressed
  if (lastState1 == HIGH && currentState1 == LOW) {
    currentPage++;
    if (currentPage > 7) { // Expanded from 6 to 7 for 8 total pages
      currentPage = 0;
    }
    forceDisplayUpdate = true;
    Serial.printf("Scrolled to Page: %d\n", currentPage);
  }

  // Check if SELECT_SW2 was just pressed
  if (lastState2 == HIGH && currentState2 == LOW) {
    Serial.println("Select Button Pressed! Toggling Relay...");
    forceDisplayUpdate = true; 
    
    switch (currentPage) {
      case 0: // Dashboard
      case 1: // Weather API (Info-only page)
        break; 
      case 2: // Mains Input 
      case 7: // Backup Battery 
        if (ina2.getBusVoltage(2) > 10){
        digitalWrite(Mains_Relay, !digitalRead(Mains_Relay));
        digitalWrite(Backup_Relay, !digitalRead(Backup_Relay));
        }
        break;
      case 3: digitalWrite(Charge_Relay, !digitalRead(Charge_Relay)); break;
      case 4: digitalWrite(Motor_Relay, !digitalRead(Motor_Relay)); break;
      case 5: digitalWrite(Lights_Relay, !digitalRead(Lights_Relay)); break;
      case 6: digitalWrite(USB_Relay, !digitalRead(USB_Relay)); break;
    }
  }

  // Save states for next loop iteration
  lastState1 = currentState1;
  lastState2 = currentState2;

  // Read Temperature Sensor
  float temperature = readTemperatureLM35();

   // Fetch weather every 10 sec
  if (millis() - lastWeatherFetch >= weatherInterval) {
    lastWeatherFetch = millis();
    fetchWeatherData();
  }

  // ==================== INA1 MODULE (0x40) ====================
  // CH1: Mains
  float Mains_V = ina1.getBusVoltage(0);
  float Mains_I = ina1.getCurrentAmps(0) * 1000; // Converted to mA
  float Mains_P = Mains_V * Mains_I / 1000;      // Power in Watts

  // CH2: Battery Charger
  float Charge_V = ina1.getBusVoltage(1);
  float Charge_I = ina1.getCurrentAmps(1) * 1000;
  float Charge_P = Charge_V * Charge_I / 1000;

  // CH3: Motor
  float Motor_V = ina1.getBusVoltage(2);
  float Motor_I = ina1.getCurrentAmps(2) * 1000;
  float Motor_P = Motor_V * Motor_I / 1000;

  // ==================== INA2 MODULE (0x41) ====================
  // CH1: Lights
  float Lights_V = ina2.getBusVoltage(0);
  float Lights_I = ina2.getCurrentAmps(0) * 1000;
  float Lights_P = Lights_V * Lights_I / 1000;

  // CH2: USB Charger
  float USB_V = ina2.getBusVoltage(1);
  float USB_I = ina2.getCurrentAmps(1) * 1000;
  float USB_P = USB_V * USB_I / 1000;

  // CH3: Backup Battery
  float Backup_V = ina2.getBusVoltage(2);
  float Backup_I = ina2.getCurrentAmps(2) * 1000;
  float Backup_P = Backup_V * Backup_I / 1000;

 // 3. --- UPDATE OLED DISPLAY MULTI-PAGE MENU ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);

  if (millis() - lastDisplayTime >= 1000 || forceDisplayUpdate) {
    lastDisplayTime = millis();
    forceDisplayUpdate = false;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(1); // Using 1 instead of SH110X_WHITE is safer for some libraries
    display.setCursor(0, 0);
    switch (currentPage) {
      case 0:
        display.println(F("--- DASHBOARD ---"));
        display.printf("Temp : %.1f C\n", temperature);
        display.printf("Rain : %s\n", digitalRead(RAIN) ? "NO" : "YES");
        display.println(F("-----------------"));
        display.println(F("Press NEXT for Weather"));
        break;

      case 1: // NEW WEATHER PAGE PLACEHOLDER
        display.println(F("--- WEATHER API ---"));
        display.printf("UV_INDEX: %s\n", weatherUV);
        display.printf("Rain    : %s mm\n", weatherRain);
        display.printf("clouds  : %s\n", weatherClouds);
        break;

      case 2:
        display.println(F("--- MAINS INPUT ---"));
        display.printf("Volt : %.2f V\n", Mains_V);
        display.printf("Curr : %.0f mA\n", Mains_I);
        display.printf("Pwr  : %.2f W\n", Mains_P);
        display.printf("Stat : %s\n", digitalRead(Mains_Relay) ? "DISCONN" : "CONN");
        break;

      case 3:
        display.println(F("--- BATT CHARGER ---"));
        display.printf("Volt : %.2f V\n", Charge_V);
        display.printf("Curr : %.0f mA\n", Charge_I);
        display.printf("Pwr  : %.2f W\n", Charge_P);
        display.printf("Stat : %s\n", digitalRead(Charge_Relay) ? "ON" : "OFF");
        break;

      case 4:
        display.println(F("--- MOTOR LOAD ---"));
        display.printf("Volt : %.2f V\n", Motor_V);
        display.printf("Curr : %.0f mA\n", Motor_I);
        display.printf("Pwr  : %.2f W\n", Motor_P);
        display.printf("Stat : %s\n", digitalRead(Motor_Relay) ? "ON" : "OFF");
        break;

      case 5:
        display.println(F("--- LIGHTS LOAD ---"));
        display.printf("Volt : %.2f V\n", Lights_V);
        display.printf("Curr : %.0f mA\n", Lights_I);
        display.printf("Pwr  : %.2f W\n", Lights_P);
        display.printf("Stat : %s\n", digitalRead(Lights_Relay) ? "NORMAL" : "EMERG");
        break;

      case 6:
        display.println(F("--- USB LOAD ---"));
        display.printf("Volt : %.2f V\n", USB_V);
        display.printf("Curr : %.0f mA\n", USB_I);
        display.printf("Pwr  : %.2f W\n", USB_P);
        display.printf("Stat : %s\n", digitalRead(USB_Relay) ? "OFF" : "ON");
        break;

      case 7:
        display.println(F("--- BACKUP BATT ---"));
        display.printf("Volt : %.2f V\n", Backup_V);
        display.printf("Curr : %.0f mA\n", Backup_I);
        display.printf("Pwr  : %.2f W\n", Backup_P);
        display.printf("Stat : %s\n", digitalRead(Backup_Relay) ? "CONN" : "ISOLATED");
        break;
    }
    display.display();
  }

  // ==================== SERIAL DEBUG OUTPUT ====================
  Serial.println("================ SENSOR DATA ================");
  Serial.printf("LM35 Temp   : %.2f C\n", temperature);
  Serial.printf("IS RAINING? : %s\n", digitalRead(RAIN) ? "NO" : "YES");
  Serial.println("---------------------------------------------");

  // INA1 Outputs
  Serial.printf("INA1 Mains  : %.2f V | %.2f mA | %.2f W\n", Mains_V, Mains_I, Mains_P);
  Serial.printf("INA1 Charge : %.2f V | %.2f mA | %.2f W\n", Charge_V, Charge_I, Charge_P);
  Serial.printf("INA1 Motor  : %.2f V | %.2f mA | %.2f W\n", Motor_V, Motor_I, Motor_P);
  Serial.println("---------------------------------------------");

  // INA2 Outputs
  Serial.printf("INA2 Lights : %.2f V | %.2f mA | %.2f W\n", Lights_V, Lights_I, Lights_P);
  Serial.printf("INA2 USB    : %.2f V | %.2f mA | %.2f W\n", USB_V, USB_I, USB_P);
  Serial.printf("INA2 Backup : %.2f V | %.2f mA | %.2f W\n", Backup_V, Backup_I, Backup_P);
  Serial.println("=============================================");

  // Read all system data
  readAllSystemData();
  
  // Build JSON
  String jsonPayload = buildSystemJSON();
  
  // Serial debug output
  Serial.println("=== SYSTEM DATA ===");
  Serial.println(jsonPayload);
  Serial.println("===================\n");
  
  // HTTP POST every 3 seconds
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    Serial.println("[SENDING] Posting to laptop...");
    sendTelemetryHTTP(jsonPayload);
  }
  
  delay(300);
}

// Helper: Read Analog LM35 Sensor
float readTemperatureLM35() {
  int rawADC = analogRead(LM35_TEMP);
  float millivolts = (rawADC / 4095.0) * 3300.0;
  return millivolts / 10.0; // LM35 output is 10mV per °C
}

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.1.5:8000/weather");
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String response = http.getString();
      
      // Parse JSON
      DynamicJsonDocument doc(512);
      deserializeJson(doc, response);
      
      weatherUV = String(doc["uv_index"].as<float>(), 1);
      weatherRain = String(doc["rain_forecast"].as<float>(), 1);
      weatherClouds = String(doc["cloud_coverage"].as<float>(), 0);
      
      Serial.printf("✓ Weather: UV=%s, Rain=%s, Clouds=%s\n", 
        weatherUV.c_str(), weatherRain.c_str(), weatherClouds.c_str());
      
    } else {
      Serial.printf("❌ Weather fetch failed: %d\n", httpResponseCode);
    }
    
    http.end();
  }
}