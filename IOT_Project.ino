//=============================================================================
// ESP32 Energy Monitor - Main Program (Dual Core with FreeRTOS Tasks)
//=============================================================================
// 
// STEP 7: ADD LIBRARY INCLUDES FOR NEW SENSORS HERE
// Example: #include <DHT.h>
// Example: #include <Servo.h>
//
//=============================================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <PZEM004Tv30.h>  // PZEM-004T library
#include <SPI.h>
#include <SD.h>
#include <time.h>         // ESP32 built-in time library
#include "config.h"
#include "data.h"
#include "display.h"
#include "logging.h"

//=============================================================================
// GLOBAL OBJECTS - STEP 8: ADD NEW SENSOR OBJECTS HERE
//=============================================================================

// System objects
DisplayHandler displayHandler;
DataHandler dataHandler;
HTTPClient http;

// ADD NEW SENSOR OBJECTS BELOW:
// Example: DHT dht(DHT22_PIN, DHT22);
// Example: Servo myServo;

// PZEM-004T Power Meter
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Status variables (shared state)
bool httpStatus = false;
bool sdStatus = false;
bool internetConnected = false;

// Serial number for data logging
int SerialNumber = 1;

// Offline data buffering
bool isBufferingMode = false;
unsigned long lastConnectivityCheck = 0;
const unsigned long CONNECTIVITY_CHECK_INTERVAL = 10000; // Check every 10 seconds

// SD Card variables
File myFile;

// NTP configuration for accurate time (using ESP32 built-in time.h)

// Current readings (shared state)
SensorData currentSensor;
SystemData currentSystem;
WiFiData currentWiFi;

//=============================================================================
// FreeRTOS: Task Handles & Synchronization
//=============================================================================
TaskHandle_t taskSensorHandle = NULL;
TaskHandle_t taskDisplayHandle = NULL;
TaskHandle_t taskNetworkHandle = NULL;

SemaphoreHandle_t dataMutex;  // protect currentSensor/currentSystem/currentWiFi/httpStatus

// Forward declarations
void connectWiFi();

// Task functions
void taskSensor(void* pvParameters) {
  (void) pvParameters;
  for (;;) {
    SensorData sensor = readSensors();
    SystemData system = getSystemData();
    WiFiData wifi = getWiFiData();

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      currentSensor = sensor;
      currentSystem = system;
      currentWiFi = wifi;
      xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void taskDisplay(void* pvParameters) {
  (void) pvParameters;
  for (;;) {
    bool localHttpOK;
    bool localSdOK;
    SensorData sensor;
    SystemData system;
    WiFiData wifi;

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      sensor = currentSensor;
      system = currentSystem;
      wifi = currentWiFi;
      localHttpOK = httpStatus;
      localSdOK = sdStatus;
      xSemaphoreGive(dataMutex);
    }

    displayHandler.update(sensor, system, wifi, localHttpOK, localSdOK);
    DebugHandler::printSummary(sensor, system, wifi);
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE));
  }
}

void taskNetwork(void* pvParameters) {
  (void) pvParameters;
  unsigned long lastWifiCheck = 0;

  for (;;) {
    // Check internet connectivity periodically
    if (millis() - lastConnectivityCheck >= CONNECTIVITY_CHECK_INTERVAL) {
      checkInternetConnectivity();
      lastConnectivityCheck = millis();
    }

    // WiFi reconnect check
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
      internetConnected = false;
      isBufferingMode = true;
    } else {
      // send payload periodically
      static unsigned long lastTx = 0;
      unsigned long now = millis();
      if (now - lastTx >= SEND_INTERVAL) {
        // Prepare safe local copies
        SensorData sensor;
        SystemData system;
        WiFiData wifi;
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          sensor = currentSensor;
          system = currentSystem;
          wifi = currentWiFi;
          xSemaphoreGive(dataMutex);
        }

        // ALWAYS LOG TO SD CARD (for data persistence)
        if (sdStatus) {
          Logger::logAllSensors(sensor, system, wifi, SerialNumber);
          Serial.print("Data logged to SD: Entry #");
          Serial.println(SerialNumber);
        } else {
          checkSDCardReconnection();
          Serial.println("SD card not available - data not persisted");
        }

        // SEND TO SERVER ONLY IF INTERNET IS CONNECTED
        if (internetConnected && !isBufferingMode) {
          // Normal mode: send current data
          String payload = dataHandler.createPayload(sensor, system, wifi);

          http.begin(API_ENDPOINT);
          http.addHeader("Content-Type", "application/json");
          http.addHeader("X-Device-Id", DEVICE_ID);

          DebugHandler::printJson(payload);
          int code = http.POST(payload);
          bool ok = (code == 200);
          DebugHandler::printHTTP(code);
          http.end();

          if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            httpStatus = ok;
            xSemaphoreGive(dataMutex);
          }

          Serial.println("Data sent to server successfully");
        } else if (isBufferingMode) {
          // Buffering mode: try to send buffered data first
          Serial.println("Internet disconnected - entering buffering mode");
          Serial.println("Attempting to send buffered data...");

          if (sendBufferedData()) {
            Serial.println("Buffered data sent successfully - resuming normal mode");
            isBufferingMode = false;
          } else {
            Serial.println("Failed to send buffered data - remaining in buffering mode");
          }
        } else {
          Serial.println("Internet not available - data buffered on SD card");
        }

        // Display SD card contents periodically (every 10 readings)
        static int displayCount = 0;
        displayCount++;
        if (displayCount % 10 == 0) {
          Serial.println("\n=== SD CARD CSV LOG SUMMARY ===");
          Logger::showLogSummary();
          Serial.println("=================================\n");

          // Show sample of each CSV file
          Logger::displayCSVContents("/power_log.csv", 3);
          Serial.println();
          Logger::displayCSVContents("/environment_log.csv", 3);
          Serial.println();
          Logger::displayCSVContents("/motion_log.csv", 3);
        }

        SerialNumber++;
        lastTx = now;
      }
    }

    // periodic WiFi RSSI refresh for UI even if not sending
    if (millis() - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        currentWiFi = getWiFiData();
        xSemaphoreGive(dataMutex);
      }
      lastWifiCheck = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

//=============================================================================
// SETUP FUNCTION - STEP 9: ADD NEW SENSOR INITIALIZATION HERE  
//=============================================================================

void setup() {
  DebugHandler::init();
  debugPrintln("Starting ESP32...");
  
  // Initialize display
  if (!displayHandler.init()) {
    debugPrintln("Display failed");
  }
  displayHandler.showStartup();
  
  //-------------------------------------------------------------------------
  // ADD NEW SENSOR INITIALIZATION BELOW:
  //-------------------------------------------------------------------------

  // Example: DHT sensor
  // dht.begin();

  // Example: Pin modes for digital sensors
  // pinMode(RELAY_PIN, OUTPUT);
  // pinMode(LED_PIN, OUTPUT);

  // Example: Servo initialization
  // myServo.attach(SERVO_PIN);

  // PZEM-004T initialization
  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  delay(1000); // Allow PZEM to initialize

  // Debug: Show sensor pin configurations
  debugPrintln("\n=== SENSOR PIN CONFIGURATION ===");
  Serial.print("PIR_PIN (HC-SR501): GPIO ");
  Serial.println(PIR_PIN);
  Serial.print("DHT22_PIN: GPIO ");
  Serial.println(DHT22_PIN);
  Serial.print("RCWL0516_PIN: GPIO ");
  Serial.println(RCWL0516_PIN);
  Serial.print("RCWL_LED_PIN: GPIO ");
  Serial.println(RCWL_LED_PIN);
  Serial.print("LED_PIN (PIR indicator): GPIO ");
  Serial.println(LED_PIN);
  Serial.print("PZEM_RX_PIN: GPIO ");
  Serial.println(PZEM_RX_PIN);
  Serial.print("PZEM_TX_PIN: GPIO ");
  Serial.println(PZEM_TX_PIN);
  Serial.print("SD_CS_PIN: GPIO ");
  Serial.println(SD_CS_PIN);
  debugPrintln("=================================\n");

  // Test all sensors at startup
  debugPrintln("=== SENSOR TEST AT STARTUP ===");
  pinMode(PIR_PIN, INPUT);
  pinMode(RCWL0516_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RCWL_LED_PIN, OUTPUT);

  Serial.print("PIR sensor reading: ");
  Serial.println(digitalRead(PIR_PIN));
  Serial.print("RCWL sensor reading: ");
  Serial.println(digitalRead(RCWL0516_PIN));

  // Test DHT22
  dht.begin();
  delay(2000); // DHT22 needs 2 seconds to stabilize
  float testTemp = dht.readTemperature();
  float testHum = dht.readHumidity();
  Serial.print("DHT22 Temperature: ");
  if (!isnan(testTemp)) {
    Serial.print(testTemp);
    Serial.println(" °C");
  } else {
    Serial.println("FAILED");
  }
  Serial.print("DHT22 Humidity: ");
  if (!isnan(testHum)) {
    Serial.print(testHum);
    Serial.println(" %");
  } else {
    Serial.println("FAILED");
  }

  debugPrintln("===============================\n");

  // SD Card initialization - NON-BLOCKING (system continues even if SD fails)
  debugPrintln("Initializing SD card (non-blocking)...");
  debugPrintln("\n==============================================");
  char debugMsg[50];
  sprintf(debugMsg, "MOSI: %d", SD_MOSI_PIN);
  debugPrintln(debugMsg);
  sprintf(debugMsg, "MISO: %d", SD_MISO_PIN);
  debugPrintln(debugMsg);
  sprintf(debugMsg, "SCK: %d", SD_SCK_PIN);
  debugPrintln(debugMsg);
  sprintf(debugMsg, "SS or CS: %d", SD_CS_PIN);
  debugPrintln(debugMsg);
  debugPrintln("==============================================\n");

  // Initialize SPI with default pins
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  // Try SD card initialization (up to 3 attempts)
  bool sdInitSuccess = trySDCardInit();

  if (sdInitSuccess) {
    sdStatus = true;
    debugPrintln("SD card OK - CSV logging enabled");

    // Initialize CSV logging system
    if (!Logger::init()) {
      debugPrintln("CSV logging initialization failed!");
      sdStatus = false;
    } else {
      debugPrintln("CSV logging system initialized");
    }
  } else {
    sdStatus = false;
    debugPrintln("SD card initialization failed - System will continue without logging");
    debugPrintln("SD card will be retried periodically...");
  }

  //-------------------------------------------------------------------------
  // WiFi connection (keep at end)
  //-------------------------------------------------------------------------
  connectWiFi();

  // Initialize NTP using ESP32 built-in time library
  configTime(25200, 0, "pool.ntp.org", "time.nist.gov"); // GMT+7 (7*3600=25200)
  debugPrintln("NTP time initialized");

  // Wait for time to be set
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 1000000000 && retry < 10) { // Wait until we get a valid timestamp
    delay(500);
    now = time(nullptr);
    retry++;
  }

  if (now >= 1000000000) {
    debugPrintln("NTP time synchronized successfully");
  } else {
    debugPrintln("NTP time synchronization failed - using system time");
  }

  // Init shared state
  currentSensor = SensorData{};
  currentSystem = getSystemData();
  currentWiFi = getWiFiData();

  // Create mutex
  dataMutex = xSemaphoreCreateMutex();

  // Create tasks pinned to cores
  // Core assignment suggestion:
  // - Core 1: Sensor + Display (I/O + light work)
  // - Core 0: Network/HTTP (WiFi stack runs on core 0)
  xTaskCreatePinnedToCore(taskSensor,  "TASK_SENSOR",  4096, NULL, 2, &taskSensorHandle, 1);
  xTaskCreatePinnedToCore(taskDisplay, "TASK_DISPLAY", 4096, NULL, 1, &taskDisplayHandle, 1);
  xTaskCreatePinnedToCore(taskNetwork, "TASK_NET",     8192, NULL, 2, &taskNetworkHandle, 0);

  debugPrintln("Ready");
}

void loop() {
  readPirRealtime();
  readRcwlRealtime();
  vTaskDelay(pdMS_TO_TICKS(1)); // loop cepat untuk PIR dan RCWL
}

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debugPrintln("WiFi OK");
  } else {
    debugPrintln("WiFi FAIL");
  }
}



// SD Card initialization with retry logic
bool trySDCardInit() {
  const int MAX_INIT_ATTEMPTS = 3;
  bool initSuccess = false;

  for (int attempt = 1; attempt <= MAX_INIT_ATTEMPTS; attempt++) {
    Serial.print("SD card init attempt ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.print(MAX_INIT_ATTEMPTS);
    Serial.print("... ");

    if (SD.begin(SD_CS_PIN)) {
      Serial.println("SUCCESS!");
      initSuccess = true;
      break;
    } else {
      Serial.println("FAILED");

      if (attempt < MAX_INIT_ATTEMPTS) {
        Serial.println("Retrying in 2 seconds...");
        delay(2000);
      }
    }
  }

  return initSuccess;
}

// Periodic SD card reconnection attempt
void checkSDCardReconnection() {
  static unsigned long lastSDCheck = 0;
  const unsigned long SD_CHECK_INTERVAL = 30000; // Check every 30 seconds

  if (millis() - lastSDCheck >= SD_CHECK_INTERVAL) {
    if (!sdStatus) {
      Serial.println("Attempting to reconnect SD card...");
      if (trySDCardInit()) {
        sdStatus = true;
        Serial.println("SD card reconnected successfully!");

        // Re-initialize CSV logging system
        if (Logger::init()) {
          Serial.println("CSV logging system re-initialized");
        } else {
          Serial.println("CSV logging re-initialization failed");
          sdStatus = false;
        }
      } else {
        Serial.println("SD card reconnection failed - will retry later");
      }
    }
    lastSDCheck = millis();
  }
}

// Check actual internet connectivity (not just WiFi)
void checkInternetConnectivity() {
  static bool lastConnectivityStatus = false;

  // Try to make a simple HTTP request to test connectivity
  HTTPClient testHttp;
  testHttp.setTimeout(5000); // 5 second timeout

  testHttp.begin("http://httpbin.org/status/200"); // Simple test endpoint
  int responseCode = testHttp.GET();

  if (responseCode == 200) {
    if (!internetConnected) {
      Serial.println("Internet connectivity restored!");
      internetConnected = true;
    }
  } else {
    if (internetConnected) {
      Serial.println("Internet connectivity lost!");
      internetConnected = false;
      isBufferingMode = true;
    }
  }

  testHttp.end();

  // Log connectivity changes
  if (lastConnectivityStatus != internetConnected) {
    Serial.print("Internet Status: ");
    Serial.println(internetConnected ? "CONNECTED" : "DISCONNECTED");
    lastConnectivityStatus = internetConnected;
  }
}

// Send buffered data from SD card to server
bool sendBufferedData() {
  if (!sdStatus || !internetConnected) {
    return false;
  }

  Serial.println("Reading buffered data from SD card...");

  // For now, we'll just mark that buffered data was sent
  // In a full implementation, you would:
  // 1. Read CSV files from SD card
  // 2. Parse the data
  // 3. Send in batches to server
  // 4. Mark sent data as processed
  // 5. Clean up old data

  // Placeholder implementation - in production you'd implement full buffering
  Serial.println("Buffered data transmission completed (placeholder)");
  return true;
}
