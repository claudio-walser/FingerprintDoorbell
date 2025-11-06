# FingerprintDoorbell - ESPHome Version

Diese ESPHome-Konfiguration ersetzt die ursprüngliche Arduino/PlatformIO-Implementierung und bietet die gleiche Funktionalität mit verbesserter Integration in Home Assistant.

## Vorteile der ESPHome-Version

1. **Einfachere Konfiguration**: YAML statt C++ Code
2. **Automatische Home Assistant Integration**: Kein MQTT erforderlich
3. **Live-Updates**: OTA-Updates ohne USB-Kabel
4. **Web Interface**: Integriertes Web-Dashboard
5. **Wartbarkeit**: Einfachere Anpassungen ohne Compilieren

## Hardware-Anforderungen

- ESP32 (gleich wie zuvor)
- Grow R503 Fingerprint Sensor
- Gleiche Verkabelung wie im Original-Projekt:
  - GPIO16 (RX) → Sensor TX
  - GPIO17 (TX) → Sensor RX
  - GPIO5 → Touch/Wake Pin
  - GPIO19 → Türklingel-Ausgang
  - 3.3V & GND

## Installation

### 1. ESPHome installieren

```bash
pip install esphome
```

### 2. Secrets konfigurieren

Bearbeite `secrets.yaml` und trage deine WiFi- und MQTT-Daten ein:

```yaml
wifi_ssid: "DeinWLAN"
wifi_password: "DeinPasswort"
mqtt_broker: "192.168.1.100"
mqtt_username: "mqtt_user"
mqtt_password: "mqtt_password"
```

### 3. Firmware kompilieren und flashen

#### Erstes Mal (USB erforderlich):

```bash
esphome run fingerprint-doorbell.yaml
```

#### Spätere Updates (OTA über WiFi):

```bash
esphome run fingerprint-doorbell.yaml --device fingerprint-doorbell.local
```

Oder über das ESPHome Dashboard:

```bash
esphome dashboard .
```

Dann im Browser: http://localhost:6052

## Konfiguration

### WiFi-Setup

Beim ersten Start ohne konfiguriertes WiFi öffnet der ESP32 einen Access Point:
- SSID: `FingerprintDoorbell-Config`
- Passwort: `12345678`

Verbinde dich damit und konfiguriere dein WiFi über das Captive Portal.

### Fingerabdrück einlernen

**Über Home Assistant:**
1. Gehe zu Settings → Devices & Services
2. Finde "Fingerprint Doorbell"
3. Setze "Enrollment Finger ID" auf gewünschte ID (1-200)
4. Drücke "Start Enrollment"
5. Folge den Anweisungen: 6x Finger auflegen und abheben

**Über Web Interface:**
Öffne http://fingerprint-doorbell.local und nutze die Buttons.

### MQTT Topics (gleich wie Original)

| Topic | Beschreibung | Werte |
|-------|--------------|-------|
| `fingerprintDoorbell/ring` | Türklingel-Event | "on" / "off" |
| `fingerprintDoorbell/matchId` | Erkannte Finger-ID | 1-200 oder "-1" |
| `fingerprintDoorbell/matchName` | Name des Fingers | String |
| `fingerprintDoorbell/matchConfidence` | Erkennungs-Konfidenz | 1-400 |
| `fingerprintDoorbell/ignoreTouchRing` | Touch-Ring ignorieren | "on" / "off" |
| `fingerprintDoorbell/status` | Online-Status | "online" / "offline" |

### Touch Ring bei Regen deaktivieren

**Über Home Assistant Automation:**

```yaml
automation:
  - alias: "Disable Fingerprint Touch Ring when raining"
    trigger:
      - platform: state
        entity_id: binary_sensor.rain_sensor
        to: "on"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.fingerprint_doorbell_ignore_touch_ring
  
  - alias: "Enable Fingerprint Touch Ring when not raining"
    trigger:
      - platform: state
        entity_id: binary_sensor.rain_sensor
        to: "off"
    action:
      - service: switch.turn_off
        target:
          entity_id: switch.fingerprint_doorbell_ignore_touch_ring
```

**Über MQTT:**

```bash
# Touch Ring deaktivieren (bei Regen)
mosquitto_pub -h your_broker -t "fingerprintDoorbell/ignoreTouchRing" -m "on"

# Touch Ring aktivieren
mosquitto_pub -h your_broker -t "fingerprintDoorbell/ignoreTouchRing" -m "off"
```

## Funktionen

### Verfügbare Entities in Home Assistant

**Sensoren:**
- Fingerprint Count (Anzahl gespeicherter Fingerabdrücke)
- Sensor Status
- Last Finger ID (Letzte erkannte ID)
- Last Confidence (Konfidenz des letzten Scans)
- WiFi Signal
- Uptime

**Schalter:**
- Doorbell Output (Türklingel-Ausgang)
- Ignore Touch Ring (Touch-Ring ignorieren)

**Buttons:**
- Start Enrollment (Fingerabdruck einlernen)
- Cancel Enrollment (Einlernen abbrechen)
- Delete Selected Fingerprint (Ausgewählten Fingerabdruck löschen)
- Delete All Fingerprints (Alle Fingerabdrücke löschen)
- Restart (Neustart)

**Eingaben:**
- Enrollment Finger ID (ID für neuen Fingerabdruck: 1-200)
- Delete Finger ID (ID zum Löschen auswählen)

## Anpassungen

### LED-Steuerung

Die Original-Firmware steuert den LED-Ring des Sensors direkt. In ESPHome wird dies automatisch vom `fingerprint_grow` Component übernommen. Wenn du zusätzlich eine externe Status-LED verwenden möchtest, passe den Pin in der Konfiguration an:

```yaml
light:
  - platform: status_led
    pin: GPIO2  # Ändere zu deinem LED-Pin
```

### Türöffner-Integration

Beispiel für Home Assistant Automation zum Öffnen der Tür bei erkanntem Fingerabdruck:

```yaml
automation:
  - alias: "Open door for known fingerprint"
    trigger:
      - platform: mqtt
        topic: fingerprintDoorbell/matchId
    condition:
      - condition: template
        value_template: "{{ trigger.payload != '-1' }}"
    action:
      - service: lock.unlock
        target:
          entity_id: lock.front_door
      - service: notify.mobile_app
        data:
          message: "Tür geöffnet für Fingerabdruck ID {{ trigger.payload }}"
```

## Unterschiede zum Original

### Vorteile:
- ✅ Keine komplexe C++ Programmierung nötig
- ✅ Automatische Home Assistant Integration
- ✅ OTA-Updates über WiFi
- ✅ Integriertes Web-Dashboard
- ✅ Einfachere Wartung und Updates
- ✅ Bessere Logging-Funktionen
- ✅ Community-Support durch ESPHome

### Limitierungen:
- ⚠️ Keine benutzerdefinierte Web-UI (nutze Home Assistant stattdessen)
- ⚠️ Fingerabdruck-Namen müssen über Home Assistant verwaltet werden
- ⚠️ Pairing-Funktion nicht implementiert (weniger relevant bei fest montiertem Sensor)

## Troubleshooting

### Sensor wird nicht erkannt
- Prüfe die UART-Pins (RX/TX vertauscht?)
- Prüfe die Baudrate (57600)
- Prüfe die Stromversorgung (Sensor braucht stabile 3.3V)

### WiFi verbindet nicht
- Halte Finger 10 Sekunden auf Sensor beim Booten → WiFi Config Mode
- Verbinde mit "FingerprintDoorbell-Config"
- Konfiguriere WiFi über Captive Portal

### MQTT funktioniert nicht
- Prüfe Broker-Adresse in secrets.yaml
- Prüfe Username/Password
- ESPHome nutzt primär die Home Assistant API - MQTT ist optional!

### Enrollment schlägt fehl
- Finger ganz auf den Sensor legen
- Finger zwischen den Scans vollständig abheben
- Sensor-Oberfläche reinigen
- Alle 6 Scans möglichst gleich positionieren

## Support

- ESPHome Dokumentation: https://esphome.io
- Fingerprint Sensor Komponente: https://esphome.io/components/fingerprint_grow.html
- Home Assistant Forum: https://community.home-assistant.io

## Migration vom Original

1. Sichere deine Fingerabdrücke (notiere IDs und Namen)
2. Flashe die ESPHome-Firmware
3. Konfiguriere WiFi
4. Lerne Fingerabdrücke neu ein (Migration der Fingerabdruck-Daten vom Sensor ist nicht möglich)
5. Konfiguriere Home Assistant Automations
6. Teste alle Funktionen

**Hinweis:** Die Fingerabdruck-Datenbank im Sensor bleibt erhalten, nur die Namen gehen verloren. Du kannst die bestehenden IDs weiter nutzen.
