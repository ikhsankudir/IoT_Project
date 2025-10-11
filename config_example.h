#ifndef CONFIG_H
#define CONFIG_H

// Configuration file for IoT Energy Meter
// Edit these values according to your setup

// WiFi Configuration - EDIT THESE VALUES
#ifndef WIFI_SSID
#define WIFI_SSID "your_Wifi_ssid"  // Change to your WiFi SSID
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your_Wifi_pass"  // Change to your WiFi password
#endif

// API Configuration - EDIT THESE VALUES
// Production API (commented out)
// #ifndef API_ENDPOINT
// #define API_ENDPOINT "your_production_endpoint"
// #endif
// #ifndef API_KEY
// #define API_KEY "your_production_api_key"
// #endif

// Development API (no security)
#ifndef API_ENDPOINT
#define API_ENDPOINT "your_DEV_endpoint"
#endif
#ifndef DEVICE_ID
#define DEVICE_ID "Your_Device_ID"
#endif

// PIR Sensor Pin (HC-SR501) - GPIO 25
#ifndef PIR_PIN
#define PIR_PIN 25
#endif

// DHT22 Sensor Pin - Default GPIO 4
#ifndef DHT22_PIN
#define DHT22_PIN 4
#endif

// IR Sensor removed as requested

// RCWL-0516 Microwave Radar Sensor Pin - Default GPIO 26
#ifndef RCWL0516_PIN
#define RCWL0516_PIN 26
#endif

// RCWL-0516 LED Pin - GPIO 27
#ifndef RCWL_LED_PIN
#define RCWL_LED_PIN 27
#endif

// PZEM-004T Serial Pins - Default UART2 (GPIO 16/17)
#ifndef PZEM_RX_PIN
#define PZEM_RX_PIN 16
#endif
#ifndef PZEM_TX_PIN
#define PZEM_TX_PIN 17
#endif

// SD Card Configuration - Alternative SPI pins to avoid conflicts
#ifndef SD_CS_PIN
#define SD_CS_PIN 5   // SPI CS
#endif

#ifndef SD_SCK_PIN
#define SD_SCK_PIN 18  // SPI SCK
#endif

#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN 23  // SPI MOSI
#endif

#ifndef SD_MISO_PIN
#define SD_MISO_PIN 19  // SPI MISO
#endif

// Calibration Defaults
#ifndef DEFAULT_VOLT_CAL
#define DEFAULT_VOLT_CAL 1.0
#endif

#ifndef DEFAULT_CURR_CAL
#define DEFAULT_CURR_CAL 111.1
#endif

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Timing Configuration
#define SAMPLE_INTERVAL 1000    // Sample every 1 second
#define SEND_INTERVAL 5000      // Send every 5 seconds
#define BUFFER_SIZE 5           // 5 samples to average
#define WIFI_CHECK_INTERVAL 30000  // Check WiFi every 30 seconds

// =========================
// Threshold Configuration
// =========================
// Tegangan (Volt) - batas aman jaringan PLN (sesuaikan)
#ifndef VOLT_MIN
#define VOLT_MIN 180.0
#endif
#ifndef VOLT_MAX
#define VOLT_MAX 250.0
#endif

// Arus (Ampere) - arus maksimum beban (sesuaikan)
#ifndef CURRENT_MAX
#define CURRENT_MAX 25.0
#endif

// Suhu & Kelembapan (DHT22)
#ifndef TEMP_LOW
#define TEMP_LOW 15.0
#endif
#ifndef TEMP_HIGH
#define TEMP_HIGH 35.0
#endif
#ifndef HUM_LOW
#define HUM_LOW 20
#endif
#ifndef HUM_HIGH
#define HUM_HIGH 80
#endif

// PIR active state (HIGH untuk HC-SR501)
#ifndef PIR_ACTIVE_STATE
#define PIR_ACTIVE_STATE HIGH
#endif

// Tambahkan definisi LED indikator PIR
#ifndef LED_PIN
#define LED_PIN 14 // GPIO14 for onboard LED indicator
#endif
#ifndef LED_ACTIVE_STATE
#define LED_ACTIVE_STATE HIGH // LED aktif HIGH
#endif

// OLED Configuration
#ifndef OLED_ADDRESS
#define OLED_ADDRESS 0x3C
#endif

// SERIAL & DEBUG Configuration
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED true
#endif

// I2C pins for display
#ifndef SDA_PIN
#define SDA_PIN 21
#endif
#ifndef SCL_PIN
#define SCL_PIN 22
#endif

// TIMING Configuration
#ifndef DISPLAY_UPDATE
#define DISPLAY_UPDATE 1000  // How often to update display (milliseconds)
#endif
#ifndef SAMPLES
#define SAMPLES 100          // Number of samples for sensor reading
#endif



#endif //
