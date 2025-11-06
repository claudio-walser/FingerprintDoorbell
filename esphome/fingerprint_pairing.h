#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include <Adafruit_Fingerprint.h>
#include <mbedtls/sha256.h>

// Fingerprint sensor commands for notepad access
#define FINGERPRINT_WRITENOTEPAD 0x18
#define FINGERPRINT_READNOTEPAD 0x19

class FingerprintPairing : public Component {
 public:
  FingerprintPairing(UARTComponent *uart) {
    finger_ = new Adafruit_Fingerprint(&Serial2);
  }

  void setup() override {
    // Initialize fingerprint sensor
    Serial2.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
    delay(100);
    
    if (finger_->verifyPassword()) {
      ESP_LOGI("pairing", "Fingerprint sensor connected");
    } else {
      ESP_LOGE("pairing", "Failed to connect to fingerprint sensor");
    }
    
    // Load stored pairing code from ESP32 preferences
    stored_pairing_code_ = id(global_prefs).get<std::string>("pairing_code").value_or("");
    pairing_valid_ = id(global_prefs).get<bool>("pairing_valid").value_or(false);
    
    if (stored_pairing_code_.empty()) {
      ESP_LOGW("pairing", "No pairing code stored - first boot. Will auto-pair.");
      do_pairing();
    }
  }

  void loop() override {
    // Nothing to do in loop
  }

  /**
   * Generate a new pairing code using SHA256 hash of unique system values
   */
  std::string generate_pairing_code() {
    mbedtls_sha256_context ctx;
    unsigned char hash[32];
    
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA256 (not SHA224)
    
    // Add unique values to hash
    uint32_t random_val = esp_random();
    mbedtls_sha256_update(&ctx, (unsigned char*)&random_val, sizeof(random_val));
    
    uint32_t uptime = millis();
    mbedtls_sha256_update(&ctx, (unsigned char*)&uptime, sizeof(uptime));
    
    // Add WiFi SSID and other config (if available)
    auto wifi_ssid = WiFi.SSID();
    mbedtls_sha256_update(&ctx, (unsigned char*)wifi_ssid.c_str(), wifi_ssid.length());
    
    // Add MAC address for uniqueness
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    mbedtls_sha256_update(&ctx, mac, 6);
    
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    // Convert first 16 bytes of hash to 32-char hex string
    char hex_string[33];
    hex_string[32] = 0; // null termination
    
    for (int i = 0; i < 16; i++) {
      sprintf(&hex_string[i * 2], "%02x", hash[i]);
    }
    
    return std::string(hex_string);
  }

  /**
   * Write data to sensor's notepad memory
   */
  uint8_t write_notepad(uint8_t page_number, const char* text, uint8_t length) {
    uint8_t data[34];
    
    if (length > 32) length = 32;
    
    data[0] = FINGERPRINT_WRITENOTEPAD;
    data[1] = page_number;
    for (int i = 0; i < length; i++) {
      data[i + 2] = text[i];
    }
    
    Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, length + 2, data);
    finger_->writeStructuredPacket(packet);
    
    if (finger_->getStructuredPacket(&packet) != FINGERPRINT_OK) {
      return FINGERPRINT_PACKETRECIEVEERR;
    }
    if (packet.type != FINGERPRINT_ACKPACKET) {
      return FINGERPRINT_PACKETRECIEVEERR;
    }
    
    return packet.data[0];
  }

  /**
   * Read data from sensor's notepad memory
   */
  uint8_t read_notepad(uint8_t page_number, char* text, uint8_t length) {
    uint8_t data[2];
    
    data[0] = FINGERPRINT_READNOTEPAD;
    data[1] = page_number;
    
    Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, 2, data);
    finger_->writeStructuredPacket(packet);
    
    if (finger_->getStructuredPacket(&packet) != FINGERPRINT_OK) {
      return FINGERPRINT_PACKETRECIEVEERR;
    }
    if (packet.type != FINGERPRINT_ACKPACKET) {
      return FINGERPRINT_PACKETRECIEVEERR;
    }
    
    if (packet.data[0] == FINGERPRINT_OK) {
      // Read data payload (skip first byte which is status)
      for (uint8_t i = 0; i < length && i < 32; i++) {
        text[i] = packet.data[i + 1];
      }
    }
    
    return packet.data[0];
  }

  /**
   * Get pairing code from sensor
   */
  std::string get_sensor_pairing_code() {
    char buffer[33];
    memset(buffer, 0, 33);
    
    uint8_t result = read_notepad(0, buffer, 32);
    
    if (result == FINGERPRINT_OK) {
      buffer[32] = 0; // ensure null termination
      return std::string(buffer);
    } else {
      ESP_LOGW("pairing", "Failed to read pairing code from sensor (error: %d)", result);
      return "";
    }
  }

  /**
   * Perform pairing with sensor
   */
  bool do_pairing() {
    ESP_LOGI("pairing", "Starting pairing process...");
    
    // Generate new pairing code
    std::string new_code = generate_pairing_code();
    ESP_LOGD("pairing", "Generated pairing code: %s", new_code.c_str());
    
    // Write to sensor notepad
    uint8_t result = write_notepad(0, new_code.c_str(), 32);
    
    if (result == FINGERPRINT_OK) {
      // Store in ESP32 preferences
      id(global_prefs).put("pairing_code", new_code);
      id(global_prefs).put("pairing_valid", true);
      
      stored_pairing_code_ = new_code;
      pairing_valid_ = true;
      
      // Update binary sensor
      id(pairing_valid).publish_state(true);
      
      ESP_LOGI("pairing", "Pairing successful!");
      id(pairing_warning).publish_state("Pairing successful");
      
      return true;
    } else {
      ESP_LOGE("pairing", "Pairing failed - could not write to sensor (error: %d)", result);
      id(pairing_warning).publish_state("Pairing failed - check sensor connection");
      return false;
    }
  }

  /**
   * Check if current pairing is valid
   */
  bool check_pairing() {
    ESP_LOGI("pairing", "Checking pairing status...");
    
    // If never paired, do automatic pairing
    if (stored_pairing_code_.empty()) {
      ESP_LOGW("pairing", "No stored pairing code - performing initial pairing");
      return do_pairing();
    }
    
    // If previously invalidated, don't auto-pair
    if (!pairing_valid_) {
      ESP_LOGW("pairing", "Pairing was invalidated previously");
      id(pairing_valid).publish_state(false);
      id(pairing_warning).publish_state("SECURITY: Pairing invalid! Sensor may have been replaced. Do re-pairing.");
      return false;
    }
    
    // Read actual pairing code from sensor
    std::string sensor_code = get_sensor_pairing_code();
    
    if (sensor_code.empty()) {
      ESP_LOGW("pairing", "Could not read pairing code from sensor - communication error");
      // Don't invalidate on communication error - might be temporary
      return false;
    }
    
    // Compare codes
    if (sensor_code == stored_pairing_code_) {
      ESP_LOGI("pairing", "Pairing valid - codes match");
      id(pairing_valid).publish_state(true);
      id(pairing_warning).publish_state("");
      return true;
    } else {
      // SECURITY ISSUE: Codes don't match!
      ESP_LOGE("pairing", "SECURITY WARNING: Pairing codes don't match!");
      ESP_LOGD("pairing", "Expected: %s", stored_pairing_code_.c_str());
      ESP_LOGD("pairing", "Got:      %s", sensor_code.c_str());
      
      // Invalidate pairing
      pairing_valid_ = false;
      id(global_prefs).put("pairing_valid", false);
      id(pairing_valid).publish_state(false);
      
      id(pairing_warning).publish_state(
        "SECURITY ALERT: Sensor pairing mismatch! Possible attack or sensor replacement detected. "
        "Fingerprint matches will be blocked. If you replaced the sensor, do re-pairing."
      );
      
      return false;
    }
  }

 protected:
  Adafruit_Fingerprint *finger_;
  std::string stored_pairing_code_;
  bool pairing_valid_ = false;
};
