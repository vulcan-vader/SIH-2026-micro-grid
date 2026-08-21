#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_INA3221.h>
#include <WiFi.h>
#include <HTTPClient.h>

// --- NETWORK CONFIGURATION ---
const char* ssid = "Airtel_juli_0293";
const char* password = "Air@89883";
const char* serverName = "http://192.168.1.5:8000/sensor"; // Replace X with server IP

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 3000; // Send telemetry every 3 seconds

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

#define NXT_SW1        18 // Next button for display
#define PREV_SW2       19 // Previous button for display

// --- I2C ADDRESSES ---
#define INA1_ADDR      0x40 // Monitors Mains, Battery Charger, Motor [CH1,CH2,CH3]
#define INA2_ADDR      0x41 // Monitors Lights, USB Charger, Backup Battery [CH!,CH2,CH3]
#define OLED_ADDR      0x3C

// --- OBJECT INSTANTIATIONS ---
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
Adafruit_INA3221 ina1;
Adafruit_INA3221 ina2;

// --- FUNCTION PROTOTYPES ---
float readTemperatureLM35();

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
  pinMode(NXT_SW1, INPUT_PULLUP);
  pinMode(PREV_SW2, INPUT_PULLUP);
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
    display.setCursor(51, 31);
    display.println(F("TESTING"));
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
    Serial.println("\nWi-Fi Connected!");
  } else {
    Serial.println("\nWi-Fi Connection Failed! Continuing offline...");
  }

  delay(1000);
}

void loop() {
  // Read Temperature Sensor
  float temperature = readTemperatureLM35();

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

  // ==================== PERIODIC HTTP POST ====================
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    // Build complete JSON string payload
    String jsonPayload = "{";
    jsonPayload += "\"temp\":" + String(temperature, 2) + ",";
    jsonPayload += "\"raining\":" + String(digitalRead(RAIN) ? "false" : "true") + ",";
    jsonPayload += "\"mains\":{\"v\":" + String(Mains_V, 2) + ",\"i\":" + String(Mains_I, 2) + ",\"p\":" + String(Mains_P, 2) + "},";
    jsonPayload += "\"charge\":{\"v\":" + String(Charge_V, 2) + ",\"i\":" + String(Charge_I, 2) + ",\"p\":" + String(Charge_P, 2) + "},";
    jsonPayload += "\"motor\":{\"v\":" + String(Motor_V, 2) + ",\"i\":" + String(Motor_I, 2) + ",\"p\":" + String(Motor_P, 2) + "},";
    jsonPayload += "\"lights\":{\"v\":" + String(Lights_V, 2) + ",\"i\":" + String(Lights_I, 2) + ",\"p\":" + String(Lights_P, 2) + "},";
    jsonPayload += "\"usb\":{\"v\":" + String(USB_V, 2) + ",\"i\":" + String(USB_I, 2) + ",\"p\":" + String(USB_P, 2) + "},";
    jsonPayload += "\"backup\":{\"v\":" + String(Backup_V, 2) + ",\"i\":" + String(Backup_I, 2) + ",\"p\":" + String(Backup_P, 2) + "}";
    jsonPayload += "}";

    sendTelemetryHTTP(jsonPayload);
  }

  delay(300); // Smooth loop pacing
}

// Helper: Post JSON Payload over HTTP
void sendTelemetryHTTP(String payload) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    int responseCode = http.POST(payload);
    Serial.printf("[HTTP] POST Result Code: %d\n", responseCode);
    
    http.end();
  } else {
    Serial.println("[HTTP] Wi-Fi Disconnected. Skipping POST.");
  }
}

// Helper: Read Analog LM35 Sensor
float readTemperatureLM35() {
  int rawADC = analogRead(LM35_TEMP);
  float millivolts = (rawADC / 4095.0) * 3300.0;
  return millivolts / 10.0; // LM35 output is 10mV per °C
}
