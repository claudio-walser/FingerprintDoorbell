#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>

namespace esphome {
namespace fingerprint_sensor {

class FingerprintSensor : public Component, public uart::UARTDevice {
 public:
  FingerprintSensor() = default;
  
  void set_match_id_sensor(sensor::Sensor *sensor) { match_id_sensor_ = sensor; }
  void set_match_name_sensor(text_sensor::TextSensor *sensor) { match_name_sensor_ = sensor; }
  void set_confidence_sensor(sensor::Sensor *sensor) { confidence_sensor_ = sensor; }
  void set_enrolled_count_sensor(sensor::Sensor *sensor) { enrolled_count_sensor_ = sensor; }
  void set_status_sensor(text_sensor::TextSensor *sensor) { status_sensor_ = sensor; }
  void set_ring_sensor(binary_sensor::BinarySensor *sensor) { ring_sensor_ = sensor; }
  
  void setup() override {
    // Initialize preferences for storing fingerprint names
    preferences_.begin("fingerprints", false);
    
    // Initialize the fingerprint sensor
    finger_.begin(57600);
    
    // Try to connect to sensor
    delay(50);
    if (finger_.verifyPassword()) {
      ESP_LOGI(TAG, "Fingerprint sensor found!");
      finger_.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 0);
      
      // Get sensor parameters
      finger_.getParameters();
      ESP_LOGI(TAG, "Capacity: %d", finger_.capacity);
      ESP_LOGI(TAG, "Security level: %d", finger_.security_level);
      
      // Get template count
      finger_.getTemplateCount();
      ESP_LOGI(TAG, "Sensor contains %d templates", finger_.templateCount);
      
      if (enrolled_count_sensor_ != nullptr) {
        enrolled_count_sensor_->publish_state(finger_.templateCount);
      }
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Ready");
      }
      connected_ = true;
      
      // Load fingerprint names from preferences
      load_fingerprint_names();
      
      // Set LED to ready state
      finger_.LEDcontrol(FINGERPRINT_LED_BREATHING, 250, FINGERPRINT_LED_BLUE);
    } else {
      ESP_LOGE(TAG, "Fingerprint sensor not found!");
      delay(5000);
      // Try again
      if (finger_.verifyPassword()) {
        ESP_LOGI(TAG, "Fingerprint sensor found on second try!");
        connected_ = true;
        finger_.LEDcontrol(FINGERPRINT_LED_BREATHING, 250, FINGERPRINT_LED_BLUE);
      } else {
        if (status_sensor_ != nullptr) {
          status_sensor_->publish_state("Sensor not found!");
        }
        connected_ = false;
        return;
      }
    }
  }
  
  void loop() override {
    if (!connected_) return;
    
    // Check if currently enrolling
    if (enrolling_) {
      // Enrollment is handled by the service call
      return;
    }
    
    // Normal scanning mode
    scan_fingerprint();
  }
  
  // Service: Enroll fingerprint
  void enroll_fingerprint(int id, const std::string &name) {
    if (!connected_) {
      ESP_LOGE(TAG, "Sensor not connected!");
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Error: Sensor not connected");
      }
      return;
    }
    
    if (id < 1 || id > 200) {
      ESP_LOGE(TAG, "Invalid ID: %d (must be 1-200)", id);
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Error: Invalid ID");
      }
      return;
    }
    
    enrolling_ = true;
    enroll_id_ = id;
    enroll_name_ = name;
    
    ESP_LOGI(TAG, "Starting enrollment for ID %d with name '%s'", id, name.c_str());
    if (status_sensor_ != nullptr) {
      status_sensor_->publish_state("Enrollment started. Place finger on sensor 5 times...");
    }
    
    // Perform enrollment
    int result = perform_enrollment(id);
    
    if (result == 0) {
      // Success
      ESP_LOGI(TAG, "Enrollment successful!");
      
      // Save name to preferences
      String key = String(id);
      preferences_.putString(key.c_str(), name.c_str());
      fingerprint_names_[id] = name;
      
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Enrollment successful!");
      }
      
      // Update count
      finger_.getTemplateCount();
      if (enrolled_count_sensor_ != nullptr) {
        enrolled_count_sensor_->publish_state(finger_.templateCount);
      }
    } else {
      ESP_LOGE(TAG, "Enrollment failed with code: %d", result);
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Enrollment failed!");
      }
    }
    
    enrolling_ = false;
    
    // Return LED to ready state
    finger_.LEDcontrol(FINGERPRINT_LED_BREATHING, 250, FINGERPRINT_LED_BLUE);
  }
  
  // Service: Delete fingerprint
  void delete_fingerprint(int id) {
    if (!connected_) {
      ESP_LOGE(TAG, "Sensor not connected!");
      return;
    }
    
    if (id < 1 || id > 200) {
      ESP_LOGE(TAG, "Invalid ID: %d", id);
      return;
    }
    
    ESP_LOGI(TAG, "Deleting fingerprint ID %d", id);
    
    uint8_t result = finger_.deleteModel(id);
    if (result == FINGERPRINT_OK) {
      ESP_LOGI(TAG, "Fingerprint deleted successfully");
      
      // Remove from preferences
      String key = String(id);
      preferences_.remove(key.c_str());
      fingerprint_names_.erase(id);
      
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Fingerprint deleted");
      }
      
      // Update count
      finger_.getTemplateCount();
      if (enrolled_count_sensor_ != nullptr) {
        enrolled_count_sensor_->publish_state(finger_.templateCount);
      }
    } else {
      ESP_LOGE(TAG, "Delete failed with code: %d", result);
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Delete failed!");
      }
    }
  }
  
  // Service: Clear all fingerprints
  void clear_all() {
    if (!connected_) {
      ESP_LOGE(TAG, "Sensor not connected!");
      return;
    }
    
    ESP_LOGI(TAG, "Clearing all fingerprints");
    
    uint8_t result = finger_.emptyDatabase();
    if (result == FINGERPRINT_OK) {
      ESP_LOGI(TAG, "Database cleared successfully");
      
      // Clear preferences
      preferences_.clear();
      fingerprint_names_.clear();
      
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("All fingerprints cleared");
      }
      if (enrolled_count_sensor_ != nullptr) {
        enrolled_count_sensor_->publish_state(0);
      }
    } else {
      ESP_LOGE(TAG, "Clear database failed with code: %d", result);
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Clear failed!");
      }
    }
  }
  
 protected:
  static constexpr const char *TAG = "fingerprint_sensor";
  
  Adafruit_Fingerprint finger_ = Adafruit_Fingerprint(&Serial2);
  Preferences preferences_;
  std::map<int, std::string> fingerprint_names_;
  bool connected_ = false;
  bool enrolling_ = false;
  int enroll_id_ = 0;
  std::string enroll_name_;
  unsigned long last_scan_time_ = 0;
  bool last_ring_state_ = false;
  
  sensor::Sensor *match_id_sensor_{nullptr};
  text_sensor::TextSensor *match_name_sensor_{nullptr};
  sensor::Sensor *confidence_sensor_{nullptr};
  sensor::Sensor *enrolled_count_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr};
  binary_sensor::BinarySensor *ring_sensor_{nullptr};
  
  void load_fingerprint_names() {
    // Load all stored fingerprint names from preferences
    for (int i = 1; i <= 200; i++) {
      String key = String(i);
      if (preferences_.isKey(key.c_str())) {
        String name = preferences_.getString(key.c_str(), "");
        if (name.length() > 0) {
          fingerprint_names_[i] = name.c_str();
          ESP_LOGD(TAG, "Loaded ID %d: %s", i, name.c_str());
        }
      }
    }
    ESP_LOGI(TAG, "Loaded %d fingerprint names from memory", fingerprint_names_.size());
  }
  
  void scan_fingerprint() {
    // Don't scan too frequently
    unsigned long current_time = millis();
    if (current_time - last_scan_time_ < 100) {
      return;
    }
    last_scan_time_ = current_time;
    
    // Check for finger on sensor
    uint8_t result = finger_.getImage();
    
    if (result == FINGERPRINT_NOFINGER) {
      // No finger detected
      if (last_ring_state_) {
        // Reset ring state
        last_ring_state_ = false;
        if (ring_sensor_ != nullptr) {
          ring_sensor_->publish_state(false);
        }
        if (match_id_sensor_ != nullptr) {
          match_id_sensor_->publish_state(-1);
        }
        if (match_name_sensor_ != nullptr) {
          match_name_sensor_->publish_state("");
        }
        if (confidence_sensor_ != nullptr) {
          confidence_sensor_->publish_state(0);
        }
        
        // Return LED to ready
        finger_.LEDcontrol(FINGERPRINT_LED_BREATHING, 250, FINGERPRINT_LED_BLUE);
      }
      return;
    }
    
    if (result != FINGERPRINT_OK) {
      // Error getting image
      return;
    }
    
    // Image captured, show LED feedback
    finger_.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 0);
    
    // Convert image to template
    result = finger_.image2Tz();
    if (result != FINGERPRINT_OK) {
      if (result == FINGERPRINT_IMAGEMESS) {
        ESP_LOGW(TAG, "Image too messy");
      } else if (result == FINGERPRINT_FEATUREFAIL || result == FINGERPRINT_INVALIDIMAGE) {
        ESP_LOGW(TAG, "Could not find fingerprint features");
      }
      return;
    }
    
    // Search for matching fingerprint
    result = finger_.fingerSearch();
    
    if (result == FINGERPRINT_OK) {
      // Match found!
      int id = finger_.fingerID;
      int confidence = finger_.confidence;
      
      ESP_LOGI(TAG, "Match found! ID: %d, Confidence: %d", id, confidence);
      
      // Get name from stored names
      std::string name = "Unknown";
      if (fingerprint_names_.find(id) != fingerprint_names_.end()) {
        name = fingerprint_names_[id];
      }
      
      // Publish to Home Assistant
      if (match_id_sensor_ != nullptr) {
        match_id_sensor_->publish_state(id);
      }
      if (match_name_sensor_ != nullptr) {
        match_name_sensor_->publish_state(name);
      }
      if (confidence_sensor_ != nullptr) {
        confidence_sensor_->publish_state(confidence);
      }
      if (ring_sensor_ != nullptr) {
        ring_sensor_->publish_state(false); // Not a ring event
      }
      
      // Purple LED for match
      finger_.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
      
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Match: " + name);
      }
      last_ring_state_ = true;
      
      // Wait a bit before next scan
      delay(3000);
      
    } else if (result == FINGERPRINT_NOTFOUND) {
      // No match found - ring doorbell!
      ESP_LOGI(TAG, "No match found - ring doorbell!");
      
      // Publish ring event to Home Assistant
      if (ring_sensor_ != nullptr) {
        ring_sensor_->publish_state(true);
      }
      if (match_id_sensor_ != nullptr) {
        match_id_sensor_->publish_state(-1);
      }
      if (match_name_sensor_ != nullptr) {
        match_name_sensor_->publish_state("");
      }
      if (confidence_sensor_ != nullptr) {
        confidence_sensor_->publish_state(0);
      }
      
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Doorbell ring!");
      }
      
      // Trigger doorbell output (will be handled by automation in Home Assistant)
      last_ring_state_ = true;
      
      delay(1000);
    }
  }
  
  int perform_enrollment(int id) {
    ESP_LOGI(TAG, "Starting enrollment for ID %d", id);
    
    // Enroll in 5 passes
    for (int pass = 1; pass <= 5; pass++) {
      ESP_LOGI(TAG, "Enrollment pass %d/5", pass);
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state("Enrollment pass " + std::to_string(pass) + "/5: Place finger");
      }
      
      // Wait for no finger on sensor (except first pass)
      if (pass > 1) {
        finger_.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_PURPLE);
        while (finger_.getImage() != FINGERPRINT_NOFINGER) {
          delay(50);
        }
        delay(500);
      }
      
      // Flash LED to indicate ready for finger
      finger_.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_PURPLE, 0);
      
      // Wait for finger on sensor
      uint8_t result = FINGERPRINT_NOFINGER;
      while (result != FINGERPRINT_OK) {
        result = finger_.getImage();
        if (result == FINGERPRINT_PACKETRECIEVEERR || result == FINGERPRINT_IMAGEFAIL) {
          ESP_LOGE(TAG, "Error capturing image");
          return result;
        }
        delay(50);
      }
      
      ESP_LOGI(TAG, "Image captured");
      
      // Convert image to template
      result = finger_.image2Tz(pass);
      if (result != FINGERPRINT_OK) {
        ESP_LOGE(TAG, "Error converting image: %d", result);
        return result;
      }
      
      // Solid LED to indicate success
      finger_.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
      delay(1000);
      
      ESP_LOGI(TAG, "Pass %d complete", pass);
    }
    
    // Create model from the 5 images
    ESP_LOGI(TAG, "Creating fingerprint model");
    if (status_sensor_ != nullptr) {
      status_sensor_->publish_state("Creating fingerprint model...");
    }
    
    uint8_t result = finger_.createModel();
    if (result != FINGERPRINT_OK) {
      ESP_LOGE(TAG, "Error creating model: %d", result);
      if (result == FINGERPRINT_ENROLLMISMATCH) {
        ESP_LOGE(TAG, "Fingerprints did not match");
      }
      return result;
    }
    
    // Store model
    ESP_LOGI(TAG, "Storing fingerprint model at ID %d", id);
    if (status_sensor_ != nullptr) {
      status_sensor_->publish_state("Storing fingerprint...");
    }
    
    result = finger_.storeModel(id);
    if (result != FINGERPRINT_OK) {
      ESP_LOGE(TAG, "Error storing model: %d", result);
      return result;
    }
    
    ESP_LOGI(TAG, "Enrollment complete!");
    return 0;
  }
};

}  // namespace fingerprint_sensor
}  // namespace esphome
