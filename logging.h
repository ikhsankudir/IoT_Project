//=============================================================================
// ESP32 Energy Monitor - Logging System (CSV Format)
//=============================================================================

#ifndef LOGGING_H
#define LOGGING_H

#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "data.h"

// SD Card file handles
extern File myFile;

//=============================================================================
// CSV LOGGING SYSTEM
//=============================================================================

// CSV Headers for different log types (static constants)
static const char* POWER_CSV_HEADER = "Timestamp,Sequence,Voltage_V,Current_A,Power_W,Energy_kWh,Frequency_Hz,Power_Factor,Voltage_Status,Current_Status\n";
static const char* ENVIRONMENT_CSV_HEADER = "Timestamp,Sequence,Temperature_C,Humidity_Pct,Temp_Status,Humidity_Status\n";
static const char* MOTION_CSV_HEADER = "Timestamp,Sequence,PIR_Motion,RCWL_Motion\n";
static const char* SYSTEM_CSV_HEADER = "Timestamp,Sequence,Uptime_S,Free_Heap_KB,CPU_MHz,SD_Present,SD_Type,SD_Size_MB,SD_Used_MB,SD_Free_MB,SD_Usage_Pct\n";
static const char* NETWORK_CSV_HEADER = "Timestamp,Sequence,WiFi_Status,IP_Address,MAC_Address,RSSI_dBm\n";

class Logger {
private:

public:
    // Initialize logging system
    static bool init() {
        // Create CSV files with headers if they don't exist
        if (!createCSVFile("/power_log.csv", POWER_CSV_HEADER)) return false;
        if (!createCSVFile("/environment_log.csv", ENVIRONMENT_CSV_HEADER)) return false;
        if (!createCSVFile("/motion_log.csv", MOTION_CSV_HEADER)) return false;
        if (!createCSVFile("/system_log.csv", SYSTEM_CSV_HEADER)) return false;
        if (!createCSVFile("/network_log.csv", NETWORK_CSV_HEADER)) return false;

        return true;
    }

    // Log all sensor data to separate CSV files
    static void logAllSensors(SensorData sensor, SystemData system, WiFiData wifi, int sequenceNumber) {
        unsigned long timestamp = millis();

        // Log power data
        logPowerData(sensor, timestamp, sequenceNumber);

        // Log environmental data
        logEnvironmentData(sensor, timestamp, sequenceNumber);

        // Log motion data
        logMotionData(sensor, timestamp, sequenceNumber);

        // Log system data
        logSystemData(system, timestamp, sequenceNumber);

        // Log network data
        logNetworkData(wifi, timestamp, sequenceNumber);
    }

    // Individual logging functions
    static void logPowerData(SensorData sensor, unsigned long timestamp, int sequence) {
        String csvLine = String(timestamp) + "," + String(sequence) + ",";

        if (sensor.pzemActive) {
            csvLine += String(sensor.pzemVoltage, 1) + ",";
            csvLine += String(sensor.pzemCurrent, 2) + ",";
            csvLine += String(sensor.pzemPower, 1) + ",";
            csvLine += String(sensor.pzemEnergy, 3) + ",";
            csvLine += String(sensor.pzemFrequency, 1) + ",";
            csvLine += String(sensor.pzemPowerFactor, 2) + ",";
        } else {
            csvLine += "0.0,0.00,0.0,0.000,0.0,0.00,";
        }

        csvLine += String(sensor.voltageOutOfRange ? "OUT_OF_RANGE" : "NORMAL") + ",";
        csvLine += String(sensor.currentOverlimit ? "OVER_LIMIT" : "NORMAL");

        writeToFile("/power_log.csv", csvLine.c_str());
    }

    static void logEnvironmentData(SensorData sensor, unsigned long timestamp, int sequence) {
        String csvLine = String(timestamp) + "," + String(sequence) + ",";

        if (!isnan(sensor.dhtTemperature) && !isnan(sensor.dhtHumidity)) {
            csvLine += String(sensor.dhtTemperature, 1) + ",";
            csvLine += String(sensor.dhtHumidity, 1) + ",";
            csvLine += String(sensor.tempOutOfRange ? "OUT_OF_RANGE" : "NORMAL") + ",";
            csvLine += String(sensor.humOutOfRange ? "OUT_OF_RANGE" : "NORMAL");
        } else {
            csvLine += "0.0,0.0,ERROR,ERROR";
        }

        writeToFile("/environment_log.csv", csvLine.c_str());
    }

    static void logMotionData(SensorData sensor, unsigned long timestamp, int sequence) {
        String csvLine = String(timestamp) + "," + String(sequence) + ",";
        csvLine += String(sensor.pirMotion ? "DETECTED" : "CLEAR") + ",";
        csvLine += String(sensor.rcwlMotion ? "DETECTED" : "CLEAR");

        writeToFile("/motion_log.csv", csvLine.c_str());
    }

    static void logSystemData(SystemData system, unsigned long timestamp, int sequence) {
        String csvLine = String(timestamp) + "," + String(sequence) + ",";
        csvLine += String(system.uptime) + ",";
        csvLine += String(system.freeHeap / 1024) + ",";
        csvLine += String(system.cpuFreq) + ",";
        csvLine += String(system.sdCardPresent ? "YES" : "NO") + ",";
        csvLine += system.sdCardType + ",";
        csvLine += String(system.sdCardSize) + ",";
        csvLine += String(system.sdCardUsed) + ",";
        csvLine += String(system.sdCardFree) + ",";
        csvLine += String(system.sdCardUsagePercent, 1);

        writeToFile("/system_log.csv", csvLine.c_str());
    }

    static void logNetworkData(WiFiData wifi, unsigned long timestamp, int sequence) {
        String csvLine = String(timestamp) + "," + String(sequence) + ",";
        csvLine += wifi.status + ",";
        csvLine += wifi.ip + ",";
        csvLine += wifi.mac + ",";
        csvLine += String(wifi.rssi);

        writeToFile("/network_log.csv", csvLine.c_str());
    }

    // Display CSV file contents
    static void displayCSVContents(const char* filename, int maxLines = 10) {
        myFile = SD.open(filename);
        if (myFile) {
            Serial.print("=== ");
            Serial.print(filename);
            Serial.println(" (CSV Format) ===");

            int lineCount = 0;
            while (myFile.available() && lineCount < maxLines) {
                String line = myFile.readStringUntil('\n');
                line.trim();
                if (line.length() > 0) {
                    Serial.print("[");
                    Serial.print(lineCount + 1);
                    Serial.print("] ");
                    Serial.println(line);
                    lineCount++;
                }
            }

            if (myFile.available()) {
                Serial.println("... (truncated, showing first ");
                Serial.print(maxLines);
                Serial.println(" lines)");
            }

            myFile.close();

            // Show file size
            myFile = SD.open(filename);
            if (myFile) {
                Serial.print("File size: ");
                Serial.print(myFile.size());
                Serial.println(" bytes");
                myFile.close();
            }
        } else {
            Serial.print("Error opening ");
            Serial.println(filename);
        }
    }

    // Show summary of all log files
    static void showLogSummary() {
        Serial.println("\n=== SD CARD LOG SUMMARY ===");

        const char* files[] = {"/power_log.csv", "/environment_log.csv", "/motion_log.csv", "/system_log.csv", "/network_log.csv"};
        const char* names[] = {"Power Data", "Environment Data", "Motion Data", "System Data", "Network Data"};

        for (int i = 0; i < 5; i++) {
            myFile = SD.open(files[i]);
            if (myFile) {
                Serial.print(names[i]);
                Serial.print(": ");
                Serial.print(myFile.size());
                Serial.println(" bytes");

                // Count lines (entries)
                int lines = 0;
                while (myFile.available()) {
                    if (myFile.read() == '\n') lines++;
                }
                Serial.print("  Entries: ");
                Serial.println(lines);

                myFile.close();
            } else {
                Serial.print(names[i]);
                Serial.println(": NOT FOUND");
            }
        }

        Serial.println("===========================\n");
    }

private:
    // Create CSV file with header if it doesn't exist
    static bool createCSVFile(const char* filename, const char* header) {
        if (!SD.exists(filename)) {
            myFile = SD.open(filename, FILE_WRITE);
            if (myFile) {
                myFile.println(header);
                myFile.close();
                Serial.print("Created ");
                Serial.println(filename);
                return true;
            } else {
                Serial.print("Failed to create ");
                Serial.println(filename);
                return false;
            }
        }
        return true;
    }

    // Write data to file
    static void writeToFile(const char* filename, const char* data) {
        myFile = SD.open(filename, FILE_APPEND);
        if (myFile) {
            myFile.println(data);
            myFile.close();
        } else {
            Serial.print("Error writing to ");
            Serial.println(filename);
        }
    }
};

// Global debug functions
void debugPrintln(const char* message);
void debugPrint(const char* message);

#endif // LOGGING_H
