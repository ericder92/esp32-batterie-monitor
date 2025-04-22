#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_ST7796S_kbv.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

// WiFi Zugangsdaten
const char* wifi_ssid = "Wlan";       // Ihr WLAN Name
const char* wifi_password = "Passwort"; // Ihr WLAN Passwort

// Feste IP-Adresse und Hostname
IPAddress local_IP(192, 168, 1, 200);         // Feste IP-Adresse
IPAddress gateway(192, 168, 1, 1);            // Gateway (Router IP)
IPAddress subnet(255, 255, 255, 0);           // Subnetzmaske
const char* hostname = "esp.monitor";          // Hostname

// Alternative IP-Adresse für den Fall, dass die feste IP nicht funktioniert
bool useFixedIP = false;  // Auf true setzen, wenn feste IP verwendet werden soll

// Variable für den WLAN-Modus
bool isAPMode = false;  // true = Access Point Modus, false = Station Modus

// ADS1115 Objekte erstellen
Adafruit_ADS1115 ads_current;  // Für Shunt-Messung (Adresse 0x48)
Adafruit_ADS1115 ads_voltage;  // Für Akkuspannung (Adresse 0x4B)

// Display Pins definieren
#define TFT_CS    5
#define TFT_RST   10
#define TFT_DC    26

// TFT Display Objekt erstellen
Adafruit_ST7796S_kbv tft = Adafruit_ST7796S_kbv(TFT_CS, TFT_DC, TFT_RST);

// Webserver auf Port 80
WebServer server(80);

// EEPROM Adressen für die Einstellungen
#define EEPROM_SIZE 32
#define ADDR_BATTERY_CAPACITY 0
#define ADDR_CRITICAL_VOLTAGE 4
#define ADDR_SHUNT_RESISTOR 8
#define ADDR_VOLTAGE_DIVIDER 12

// Konstanten für Berechnungen
float SHUNT_RESISTOR = 0.00025;  // 75mV/300A = 0.00025 Ohm (Standardwert)
float VOLTAGE_DIVIDER = 6.0;     // Spannungsteiler für Akku-Messung (10k + 2k) / 2k = 6.0
float CURRENT_DEADBAND = 0.06;   // Strom-Deadband in Ampere (60mA)

// Spannungsteiler Widerstände
const float R1 = 10000.0;        // Vorwiderstand 10k Ohm
const float R2 = 2000.0;         // Spannungsteilerwiderstand 2k Ohm

// Variablen für Messwerte
float voltage = 0.0;
float current = 0.0;
float power = 0.0;
float ampereHours = 0.0;
unsigned long lastTime = 0;
float totalAh = 0.0;
float batteryCapacity = 180.0;  // Standard Akkukapazität in Ah
float stateOfCharge = 100.0;    // Ladezustand in Prozent
float remainingAh = 180.0;      // Restkapazität in Ah

// Statistik-Variablen
float maxVoltage = 0.0;
float minVoltage = 100.0;
float maxChargeCurrent = 0.0;
float maxDischargeCurrent = 0.0;
float maxPower = 0.0;
float maxConsumption = 0.0;
float totalConsumption = 0.0;
unsigned long startTime = 0;

// Konstanten für Warnungen
float CRITICAL_VOLTAGE = 11.0;  // Kritische Spannung in Volt
bool ads_voltage_ok = false;
bool ads_current_ok = false;

// Variable für WLAN-Status
bool wifiConnected = false;

// Funktionsdeklarationen
void loadSettings();
void saveSettings();
void updateDisplay();
void setupWiFi();
void setupWebserver();
void updateMeasurements();

// HTML für die Hauptseite
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Strommessung</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding: 20px; }
    .reading { font-size: 2.8rem; }
    .unit { font-size: 1.2rem; }
    .container { max-width: 600px; margin: 0 auto; }
    .value-box { background-color: #f0f0f0; border-radius: 10px; padding: 20px; margin: 10px 0; }
    .battery-status { font-size: 1.5rem; margin: 10px 0; }
    .warning { background-color: #ffeb3b; color: #000; padding: 10px; border-radius: 5px; margin: 10px 0; }
    .error { background-color: #f44336; color: white; padding: 10px; border-radius: 5px; margin: 10px 0; }
    .settings-button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin: 20px 0; }
    .settings-button:hover { background-color: #45a049; }
    .gear-icon { position: absolute; top: 20px; right: 20px; font-size: 24px; color: #333; text-decoration: none; }
    .gear-icon:hover { color: #000; }
    .header { position: relative; }
    .battery-info { display: flex; justify-content: space-between; margin-top: 10px; }
    .battery-info-item { flex: 1; text-align: center; }
    .button-container { display: flex; justify-content: space-between; margin-top: 20px; }
    .button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
    .button:hover { background-color: #45a049; }
    .stats-button { background-color: #2196F3; }
    .stats-button:hover { background-color: #0b7dda; }
    .mode-indicator { background-color: #2196F3; color: white; padding: 5px 10px; border-radius: 5px; display: inline-block; margin-bottom: 10px; }
    .mode-indicator.ap { background-color: #FF9800; }
  </style>
  <script>
    // Sofort beim Laden der Seite die Daten abrufen
    window.onload = function() {
      getData();
      // Dann alle 1 Sekunde aktualisieren
      setInterval(getData, 1000);
    };
    
    function getData() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          document.getElementById("voltage").innerHTML = data.voltage.toFixed(2);
          document.getElementById("current").innerHTML = data.current.toFixed(2);
          document.getElementById("power").innerHTML = data.power.toFixed(2);
          document.getElementById("ampereHours").innerHTML = data.ampereHours.toFixed(2);
          document.getElementById("stateOfCharge").innerHTML = data.stateOfCharge.toFixed(1);
          document.getElementById("remainingAh").innerHTML = data.remainingAh.toFixed(2);
          
          // Modus anzeigen
          var modeIndicator = document.getElementById("modeIndicator");
          if (data.isAPMode) {
            modeIndicator.innerHTML = "Access Point Modus";
            modeIndicator.className = "mode-indicator ap";
          } else {
            modeIndicator.innerHTML = "Station Modus";
            modeIndicator.className = "mode-indicator";
          }
          
          // Warnungen und Fehler anzeigen
          var warningsDiv = document.getElementById("warnings");
          warningsDiv.innerHTML = "";
          
          if (!data.ads_voltage_ok) {
            warningsDiv.innerHTML += '<div class="error">ADS1115 f&uuml;r Spannungsmessung nicht initialisiert!</div>';
          }
          if (!data.ads_current_ok) {
            warningsDiv.innerHTML += '<div class="error">ADS1115 f&uuml;r Strommessung nicht initialisiert!</div>';
          }
          if (data.voltage < data.criticalVoltage) {
            warningsDiv.innerHTML += '<div class="warning">Kritische Spannung: ' + data.voltage.toFixed(2) + 'V</div>';
          }
        }
      };
      xhttp.open("GET", "/data", true);
      xhttp.send();
    }
  </script>
</head>
<body>
  <div class="container">
    <div class="header">
      <h2>ESP32 Strommessung</h2>
      <a href="/settings" class="gear-icon">&#9881;</a>
    </div>
    <div id="modeIndicator" class="mode-indicator">Station Modus</div>
    <div id="warnings"></div>
    <div class="value-box">
      <span class="reading" id="voltage">0.00</span>
      <span class="unit">V</span>
    </div>
    <div class="value-box">
      <span class="reading" id="current">0.00</span>
      <span class="unit">A</span>
    </div>
    <div class="value-box">
      <span class="reading" id="power">0.00</span>
      <span class="unit">W</span>
    </div>
    <div class="value-box">
      <span class="reading" id="ampereHours">0.00</span>
      <span class="unit">Ah</span>
    </div>
    <div class="value-box">
      <span class="reading" id="stateOfCharge">100.0</span>
      <span class="unit">%</span>
      <div class="battery-info">
        <div class="battery-info-item">
          <span>Restkapaz&auml;t:</span>
          <span id="remainingAh">180.00</span>
          <span class="unit">Ah</span>
        </div>
      </div>
    </div>
    <div class="button-container">
      <a href="/settings" class="button">Einstellungen</a>
      <a href="/stats" class="button stats-button">Statistiken</a>
    </div>
  </div>
</body>
</html>
)rawliteral";

// HTML für die Einstellungsseite
const char settings_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Einstellungen</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; }
    .settings-box { background-color: #e3f2fd; padding: 20px; border-radius: 10px; margin: 20px 0; }
    .input-group { margin: 10px 0; }
    .input-group label { display: block; margin-bottom: 5px; }
    .input-group input { width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #ccc; }
    .button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 0; }
    .button:hover { background-color: #45a049; }
    .home-button { background-color: #2196F3; }
    .home-button:hover { background-color: #0b7dda; }
    .save-button { background-color: #4CAF50; }
    .save-button:hover { background-color: #45a049; }
    .gear-icon { position: absolute; top: 20px; right: 20px; font-size: 24px; color: #333; text-decoration: none; }
    .gear-icon:hover { color: #000; }
    .header { position: relative; }
    .tab-container { margin: 20px 0; }
    .tab-buttons { display: flex; margin-bottom: 10px; }
    .tab-button { flex: 1; padding: 10px; background-color: #f1f1f1; border: none; cursor: pointer; }
    .tab-button.active { background-color: #4CAF50; color: white; }
    .tab-content { display: none; }
    .tab-content.active { display: block; }
  </style>
  <script>
    function getSettings() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          document.getElementById("batteryCapacity").value = data.batteryCapacity;
          document.getElementById("capacityValue").innerHTML = data.batteryCapacity;
          document.getElementById("criticalVoltage").value = data.criticalVoltage;
          document.getElementById("voltageValue").innerHTML = data.criticalVoltage.toFixed(1);
          document.getElementById("shuntResistor").value = data.shuntResistor;
          document.getElementById("shuntValue").innerHTML = parseFloat(data.shuntResistor).toFixed(7);
          document.getElementById("voltageDivider").value = data.voltageDivider;
          document.getElementById("dividerValue").innerHTML = data.voltageDivider.toFixed(1);
        }
      };
      xhttp.open("GET", "/data", true);
      xhttp.send();
    }
    
    function updateCapacity() {
      var capacity = document.getElementById("batteryCapacity").value;
      document.getElementById("capacityValue").innerHTML = capacity;
    }
    
    function updateCriticalVoltage() {
      var voltage = document.getElementById("criticalVoltage").value;
      document.getElementById("voltageValue").innerHTML = voltage;
    }
    
    function updateShuntResistor() {
      var resistor = document.getElementById("shuntResistor").value;
      document.getElementById("shuntValue").innerHTML = parseFloat(resistor).toFixed(7);
    }
    
    function updateVoltageDivider() {
      var divider = document.getElementById("voltageDivider").value;
      document.getElementById("dividerValue").innerHTML = divider;
    }
    
    function calculateShuntResistor() {
      var current = parseFloat(document.getElementById("shuntCurrent").value);
      var voltage = parseFloat(document.getElementById("shuntVoltage").value);
      
      if (current > 0 && voltage > 0) {
        var resistor = (voltage / 1000) / current; // mV zu V umrechnen
        document.getElementById("shuntResistor").value = resistor;
        document.getElementById("shuntValue").innerHTML = resistor.toFixed(7);
      } else {
        alert("Bitte g&uuml;ltige Werte f&uuml;r Strom und Spannung eingeben!");
      }
    }
    
    function openTab(evt, tabName) {
      var i, tabcontent, tablinks;
      
      // Alle Tab-Inhalte ausblenden
      tabcontent = document.getElementsByClassName("tab-content");
      for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].classList.remove("active");
      }
      
      // Alle Tab-Buttons inaktiv machen
      tablinks = document.getElementsByClassName("tab-button");
      for (i = 0; i < tablinks.length; i++) {
        tablinks[i].classList.remove("active");
      }
      
      // Aktiven Tab anzeigen und Button aktivieren
      document.getElementById(tabName).classList.add("active");
      evt.currentTarget.classList.add("active");
    }
    
    function saveSettings() {
      var capacity = document.getElementById("batteryCapacity").value;
      var voltage = document.getElementById("criticalVoltage").value;
      var resistor = document.getElementById("shuntResistor").value;
      var divider = document.getElementById("voltageDivider").value;
      
      // Validierung der Eingabewerte
      if (isNaN(capacity) || capacity <= 0) {
        alert("Bitte geben Sie einen gültigen Wert für die Akkukapazität ein!");
        return;
      }
      if (isNaN(voltage) || voltage <= 0) {
        alert("Bitte geben Sie einen gültigen Wert für die kritische Spannung ein!");
        return;
      }
      if (isNaN(resistor) || resistor <= 0) {
        alert("Bitte geben Sie einen gültigen Wert für den Shunt-Widerstand ein!");
        return;
      }
      if (isNaN(divider) || divider <= 0) {
        alert("Bitte geben Sie einen gültigen Wert für den Spannungsteiler ein!");
        return;
      }
      
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4) {
          if (this.status == 200) {
            alert("Einstellungen wurden erfolgreich gespeichert!");
            // Aktualisiere die Anzeige der aktuellen Werte
            document.getElementById("capacityValue").innerHTML = capacity;
            document.getElementById("voltageValue").innerHTML = voltage;
            document.getElementById("shuntValue").innerHTML = parseFloat(resistor).toFixed(7);
            document.getElementById("dividerValue").innerHTML = divider;
          } else {
            alert("Fehler beim Speichern der Einstellungen! Status: " + this.status);
          }
        }
      };
      xhttp.open("GET", "/saveSettings?capacity=" + capacity + "&voltage=" + voltage + "&resistor=" + resistor + "&divider=" + divider, true);
      xhttp.send();
    }
    
    // Lade Einstellungen beim Start
    window.onload = function() {
      getSettings();
      // Standardmäßig den ersten Tab öffnen
      document.getElementById("directTab").classList.add("active");
      document.getElementsByClassName("tab-button")[0].classList.add("active");
    };
  </script>
</head>
<body>
  <div class="container">
    <div class="header">
      <h2>ESP32 Einstellungen</h2>
      <a href="/" class="gear-icon">&#8962;</a>
    </div>
    <div class="settings-box">
      <h3>Einstellungen</h3>
      <div class="input-group">
        <label for="batteryCapacity">Akkukapazit&auml;t (Ah):</label>
        <input type="number" min="1" max="1000" step="1" value="180" id="batteryCapacity" onchange="updateCapacity()">
        <span>Aktuell: <span id="capacityValue">180</span> Ah</span>
      </div>
      <div class="input-group">
        <label for="criticalVoltage">Kritische Spannung (V):</label>
        <input type="number" min="1" max="48" step="0.1" value="11.0" id="criticalVoltage" onchange="updateCriticalVoltage()">
        <span>Aktuell: <span id="voltageValue">11.0</span> V</span>
      </div>
      
      <div class="tab-container">
        <div class="tab-buttons">
          <button class="tab-button" onclick="openTab(event, 'directTab')">Direkt</button>
          <button class="tab-button" onclick="openTab(event, 'calculateTab')">Berechnen</button>
        </div>
        
        <div id="directTab" class="tab-content">
          <div class="input-group">
            <label for="shuntResistor">Shunt-Widerstand (Ohm):</label>
            <input type="number" min="0.000001" max="0.01" step="0.0000001" value="0.00025" id="shuntResistor" onchange="updateShuntResistor()">
            <span>Aktuell: <span id="shuntValue">0.0002500</span> Ohm</span>
          </div>
        </div>
        
        <div id="calculateTab" class="tab-content">
          <div class="input-group">
            <label for="shuntCurrent">Shunt-Strom (A):</label>
            <input type="number" min="0.1" max="1000" step="0.1" value="300" id="shuntCurrent">
          </div>
          <div class="input-group">
            <label for="shuntVoltage">Shunt-Spannung (mV):</label>
            <input type="number" min="1" max="1000" step="1" value="75" id="shuntVoltage">
          </div>
          <button class="button" onclick="calculateShuntResistor()">Widerstand berechnen</button>
        </div>
      </div>
      
      <div class="input-group">
        <label for="voltageDivider">Spannungsteiler-Verh&auml;ltnis:</label>
        <input type="number" min="1" max="100" step="0.1" value="6.0" id="voltageDivider" onchange="updateVoltageDivider()">
        <span>Aktuell: <span id="dividerValue">6.0</span></span>
      </div>
      <button class="button save-button" onclick="saveSettings()">Einstellungen speichern</button>
    </div>
  </div>
</body>
</html>
)rawliteral";

// HTML für die Statistikseite
const char stats_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 Statistiken</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; }
    .stats-box { background-color: #e3f2fd; padding: 20px; border-radius: 10px; margin: 20px 0; }
    .stat-item { display: flex; justify-content: space-between; margin: 10px 0; padding: 10px; background-color: #f5f5f5; border-radius: 5px; }
    .stat-label { font-weight: bold; }
    .stat-value { font-family: monospace; }
    .button { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin: 10px 0; }
    .button:hover { background-color: #45a049; }
    .home-button { background-color: #2196F3; }
    .home-button:hover { background-color: #0b7dda; }
    .gear-icon { position: absolute; top: 20px; right: 20px; font-size: 24px; color: #333; text-decoration: none; }
    .gear-icon:hover { color: #000; }
    .header { position: relative; }
  </style>
  <script>
    // Sofort beim Laden der Seite die Daten abrufen
    window.onload = function() {
      getStats();
      // Dann alle 1 Sekunde aktualisieren
      setInterval(getStats, 1000);
    };
    
    function getStats() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          document.getElementById("maxVoltage").innerHTML = data.maxVoltage.toFixed(2) + " V";
          document.getElementById("minVoltage").innerHTML = data.minVoltage.toFixed(2) + " V";
          document.getElementById("maxChargeCurrent").innerHTML = data.maxChargeCurrent.toFixed(2) + " A";
          document.getElementById("maxDischargeCurrent").innerHTML = data.maxDischargeCurrent.toFixed(2) + " A";
          document.getElementById("maxPower").innerHTML = data.maxPower.toFixed(2) + " W";
          document.getElementById("maxConsumption").innerHTML = data.maxConsumption.toFixed(2) + " Ah";
          document.getElementById("totalConsumption").innerHTML = data.totalConsumption.toFixed(2) + " Ah";
          document.getElementById("runtime").innerHTML = data.runtime;
        }
      };
      xhttp.open("GET", "/statsData", true);
      xhttp.send();
    }
  </script>
</head>
<body>
  <div class="container">
    <div class="header">
      <h2>ESP32 Statistiken</h2>
      <a href="/" class="gear-icon">&#8962;</a>
    </div>
    <div class="stats-box">
      <h3>Messwerte</h3>
      <div class="stat-item">
        <span class="stat-label">Max. Spannung:</span>
        <span class="stat-value" id="maxVoltage">0.00 V</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Min. Spannung:</span>
        <span class="stat-value" id="minVoltage">0.00 V</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Max. Ladestrom:</span>
        <span class="stat-value" id="maxChargeCurrent">0.00 A</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Max. Entladestrom:</span>
        <span class="stat-value" id="maxDischargeCurrent">0.00 A</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Max. Leistung:</span>
        <span class="stat-value" id="maxPower">0.00 W</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Max. Verbrauch:</span>
        <span class="stat-value" id="maxConsumption">0.00 Ah</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Gesamtverbrauch:</span>
        <span class="stat-value" id="totalConsumption">0.00 Ah</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">Laufzeit:</span>
        <span class="stat-value" id="runtime">0:00:00</span>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

// Funktion zum Laden der Einstellungen aus dem EEPROM
void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Debug-Ausgabe für Shunt-Widerstand
  Serial.println("Lade Shunt-Widerstand aus EEPROM...");
  
  // Lade Shunt-Widerstand
  EEPROM.get(ADDR_SHUNT_RESISTOR, SHUNT_RESISTOR);
  Serial.print("Geladener Shunt-Widerstand: ");
  Serial.print(SHUNT_RESISTOR * 1000, 7); // 7 Dezimalstellen
  Serial.println(" mOhm");
  
  if (isnan(SHUNT_RESISTOR) || SHUNT_RESISTOR <= 0) {
    Serial.println("Ungültiger Shunt-Widerstand gefunden, setze Standardwert");
    SHUNT_RESISTOR = 0.00025; // Standardwert, falls ungültig
    Serial.print("Standardwert gesetzt: ");
    Serial.print(SHUNT_RESISTOR * 1000, 7);
    Serial.println(" mOhm");
  }
  
  // Lade Akkukapazität
  EEPROM.get(ADDR_BATTERY_CAPACITY, batteryCapacity);
  if (isnan(batteryCapacity) || batteryCapacity <= 0) {
    batteryCapacity = 180.0; // Standardwert, falls ungültig
  }
  
  // Lade kritische Spannung
  EEPROM.get(ADDR_CRITICAL_VOLTAGE, CRITICAL_VOLTAGE);
  if (isnan(CRITICAL_VOLTAGE) || CRITICAL_VOLTAGE <= 0) {
    CRITICAL_VOLTAGE = 11.0; // Standardwert, falls ungültig
  }
  
  // Lade Spannungsteiler
  EEPROM.get(ADDR_VOLTAGE_DIVIDER, VOLTAGE_DIVIDER);
  if (isnan(VOLTAGE_DIVIDER) || VOLTAGE_DIVIDER <= 0) {
    VOLTAGE_DIVIDER = 6.0; // Standardwert, falls ungültig
  }
  
  Serial.println("Einstellungen geladen:");
  Serial.print("Akkukapazität: "); Serial.print(batteryCapacity); Serial.println(" Ah");
  Serial.print("Kritische Spannung: "); Serial.print(CRITICAL_VOLTAGE); Serial.println(" V");
  Serial.print("Shunt-Widerstand: "); Serial.print(SHUNT_RESISTOR, 7); Serial.println(" Ohm");
  Serial.print("Spannungsteiler: "); Serial.print(VOLTAGE_DIVIDER); Serial.println();
}

// Funktion zum Speichern der Einstellungen im EEPROM
void saveSettings() {
  Serial.println("Speichere Einstellungen...");
  Serial.print("Shunt-Widerstand vor dem Speichern: ");
  Serial.print(SHUNT_RESISTOR * 1000, 7);
  Serial.println(" mOhm");
  
  EEPROM.put(ADDR_SHUNT_RESISTOR, SHUNT_RESISTOR);
  EEPROM.put(ADDR_BATTERY_CAPACITY, batteryCapacity);
  EEPROM.put(ADDR_CRITICAL_VOLTAGE, CRITICAL_VOLTAGE);
  EEPROM.put(ADDR_VOLTAGE_DIVIDER, VOLTAGE_DIVIDER);
  EEPROM.commit();
  
  // Überprüfe nach dem Speichern
  float testValue;
  EEPROM.get(ADDR_SHUNT_RESISTOR, testValue);
  Serial.print("Shunt-Widerstand nach dem Speichern: ");
  Serial.print(testValue * 1000, 7);
  Serial.println(" mOhm");
  
  Serial.println("Einstellungen gespeichert:");
  Serial.print("Akkukapazität: "); Serial.print(batteryCapacity, 2); Serial.println(" Ah");
  Serial.print("Kritische Spannung: "); Serial.print(CRITICAL_VOLTAGE, 2); Serial.println(" V");
  Serial.print("Shunt-Widerstand: "); Serial.print(SHUNT_RESISTOR, 7); Serial.println(" Ohm");
  Serial.print("Spannungsteiler: "); Serial.print(VOLTAGE_DIVIDER, 2); Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Strommessung startet...");
  
  // Startzeit für Laufzeitberechnung
  startTime = millis();
  
  // Einstellungen laden
  loadSettings();
  
  // I2C initialisieren
  Wire.begin(21, 22);  // SDA, SCL für ESP32
  Serial.println("I2C initialisiert");
  
  // I2C-Scan durchführen
  Serial.println("I2C-Scan starten...");
  for (byte i = 1; i < 127; i++) {
    Wire.beginTransmission(i);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C-Gerät gefunden an Adresse 0x");
      if (i < 16) Serial.print("0");
      Serial.println(i, HEX);
    }
  }
  Serial.println("I2C-Scan abgeschlossen");
  
  // ADS1115 für Strommessung initialisieren (Adresse 0x48)
  if (!ads_current.begin(0x48)) {
    Serial.println("Fehler beim Starten von ADS1115 #1 (0x48)");
    ads_current_ok = false;
  } else {
    Serial.println("ADS1115 für Strommessung initialisiert");
    ads_current_ok = true;
    // Gain für Strommessung setzen (5V Betrieb)
    ads_current.setGain(GAIN_SIXTEEN);    // +/- 0.256V für Shunt-Spannung
  }
  
  // ADS1115 für Spannungsmessung initialisieren (Adresse 0x4B)
  if (!ads_voltage.begin(0x4B)) {
    Serial.println("Fehler beim Starten von ADS1115 #2 (0x4B)");
    ads_voltage_ok = false;
  } else {
    Serial.println("ADS1115 für Spannungsmessung initialisiert");
    ads_voltage_ok = true;
    // Gain für Spannungsmessung setzen (5V Betrieb)
    ads_voltage.setGain(GAIN_TWOTHIRDS);  // +/- 6.144V für Akkuspannung
  }
  
  // Display initialisieren
  tft.begin();  // Initialisiere das Display
  tft.setRotation(1); // Landscape-Modus
  tft.fillScreen(ST7796S_BLACK);
  tft.setTextColor(ST7796S_WHITE);
  tft.setTextSize(2);
  
  // Überschrift
  tft.setCursor(10, 10);
  tft.print("ESP32 Strommessung");
  tft.drawFastHLine(10, 40, 460, ST7796S_WHITE);
  
  Serial.println("Display initialisiert");
  
  // WLAN-Verbindung versuchen
  setupWiFi();
  
  lastTime = millis();
  Serial.println("Setup abgeschlossen");
}

// Neue Funktion für WLAN-Setup
void setupWiFi() {
  // WiFi im Station Modus starten
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Trenne eventuelle bestehende Verbindungen
  delay(1000);       // Warte einen Moment
  
  // Hostname setzen
  WiFi.setHostname(hostname);
  
  // Feste IP-Adresse konfigurieren, wenn gewünscht
  if (useFixedIP) {
    if (!WiFi.config(local_IP, gateway, subnet)) {
      Serial.println("Fehler bei der IP-Konfiguration!");
    } else {
      Serial.println("Feste IP-Adresse konfiguriert");
    }
  }
  
  // Mit WLAN verbinden
  Serial.print("Verbinde mit WLAN: ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {  // Mehr Versuche
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nMit WLAN verbunden!");
    Serial.print("IP Adresse: ");
    Serial.println(WiFi.localIP());
    Serial.print("Hostname: ");
    Serial.println(WiFi.getHostname());
    wifiConnected = true;
    
    // Webserver Routen
    setupWebserver();
    
    // Webserver starten
    server.begin();
    Serial.println("Webserver gestartet");
    Serial.print("Webseite erreichbar unter: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nKonnte sich nicht mit WLAN verbinden!");
    Serial.println("Fahre mit Datenaufzeichnung fort...");
    wifiConnected = false;
  }
}

// Neue Funktion für Webserver-Setup
void setupWebserver() {
  server.on("/", HTTP_GET, [](){
    Serial.println("Anfrage für Hauptseite erhalten");
    server.send_P(200, "text/html", index_html);
  });
  
  server.on("/settings", HTTP_GET, [](){
    Serial.println("Anfrage für Einstellungsseite erhalten");
    server.send_P(200, "text/html", settings_html);
  });
  
  server.on("/stats", HTTP_GET, [](){
    Serial.println("Anfrage für Statistikseite erhalten");
    server.send_P(200, "text/html", stats_html);
  });
  
  server.on("/data", HTTP_GET, [](){
    Serial.println("Anfrage für Daten erhalten");
    String json = "{\"voltage\":" + String(voltage) + 
                 ",\"current\":" + String(current) + 
                 ",\"power\":" + String(power) + 
                 ",\"ampereHours\":" + String(totalAh) + 
                 ",\"stateOfCharge\":" + String(stateOfCharge) +
                 ",\"remainingAh\":" + String(remainingAh) +
                 ",\"batteryCapacity\":" + String(batteryCapacity) +
                 ",\"criticalVoltage\":" + String(CRITICAL_VOLTAGE) +
                 ",\"ads_voltage_ok\":" + String(ads_voltage_ok ? "true" : "false") +
                 ",\"ads_current_ok\":" + String(ads_current_ok ? "true" : "false") +
                 ",\"shuntResistor\":" + String(SHUNT_RESISTOR, 7) +
                 ",\"voltageDivider\":" + String(VOLTAGE_DIVIDER) + "}";
    server.send(200, "application/json", json);
  });
  
  server.on("/statsData", HTTP_GET, [](){
    Serial.println("Anfrage für Statistiken erhalten");
    
    // Laufzeit berechnen
    unsigned long runtimeMillis = millis() - startTime;
    unsigned long hours = runtimeMillis / 3600000;
    unsigned long minutes = (runtimeMillis % 3600000) / 60000;
    unsigned long seconds = (runtimeMillis % 60000) / 1000;
    String runtimeStr = String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
    
    String json = "{\"maxVoltage\":" + String(maxVoltage) + 
                 ",\"minVoltage\":" + String(minVoltage) + 
                 ",\"maxChargeCurrent\":" + String(maxChargeCurrent) + 
                 ",\"maxDischargeCurrent\":" + String(maxDischargeCurrent) + 
                 ",\"maxPower\":" + String(maxPower) + 
                 ",\"maxConsumption\":" + String(maxConsumption) + 
                 ",\"totalConsumption\":" + String(totalAh) + 
                 ",\"runtime\":\"" + runtimeStr + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/saveSettings", HTTP_GET, [](){
    bool success = true;
    String errorMsg = "";
    
    if(server.hasArg("capacity")) {
      float newCapacity = server.arg("capacity").toFloat();
      if (newCapacity > 0) {
        batteryCapacity = newCapacity;
        stateOfCharge = 100.0 - (totalAh / batteryCapacity * 100.0);
        if(stateOfCharge < 0) stateOfCharge = 0;
        if(stateOfCharge > 100) stateOfCharge = 100;
      } else {
        success = false;
        errorMsg += "Ungültige Akkukapazität. ";
      }
    }
    
    if(server.hasArg("voltage")) {
      float newVoltage = server.arg("voltage").toFloat();
      if (newVoltage > 0) {
        CRITICAL_VOLTAGE = newVoltage;
      } else {
        success = false;
        errorMsg += "Ungültige kritische Spannung. ";
      }
    }
    
    if(server.hasArg("resistor")) {
      float newResistor = server.arg("resistor").toFloat();
      Serial.print("Neuer Shunt-Widerstand empfangen: ");
      Serial.print(newResistor * 1000, 7);
      Serial.println(" mOhm");
      
      if (newResistor > 0) {
        SHUNT_RESISTOR = newResistor;
        Serial.print("Shunt-Widerstand aktualisiert auf: ");
        Serial.print(SHUNT_RESISTOR * 1000, 7);
        Serial.println(" mOhm");
      } else {
        success = false;
        errorMsg += "Ungültiger Shunt-Widerstand. ";
        Serial.println("Ungültiger Shunt-Widerstand empfangen!");
      }
    } else {
      Serial.println("Kein Shunt-Widerstand in der Anfrage gefunden!");
    }
    
    if(server.hasArg("divider")) {
      float newDivider = server.arg("divider").toFloat();
      if (newDivider > 0) {
        VOLTAGE_DIVIDER = newDivider;
      } else {
        success = false;
        errorMsg += "Ungültiger Spannungsteiler. ";
      }
    }
    
    if (success) {
      // Einstellungen im EEPROM speichern
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", errorMsg);
    }
  });
}

void loop() {
  // Messwerte aktualisieren
  updateMeasurements();
  
  // Display aktualisieren
  updateDisplay();
  
  // Wenn WLAN verbunden ist, Webserver-Anfragen verarbeiten
  if (wifiConnected) {
    server.handleClient();
    
    // WLAN-Verbindung überprüfen
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi-Verbindung verloren!");
      wifiConnected = false;
    }
  } else {
    // Alle 30 Sekunden versuchen, WLAN-Verbindung wiederherzustellen
    static unsigned long lastWiFiAttempt = 0;
    if (millis() - lastWiFiAttempt > 30000) {
      lastWiFiAttempt = millis();
      setupWiFi();
    }
  }
  
  // Debug-Ausgabe für Status
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 5000) {  // Alle 5 Sekunden
    lastDebugTime = millis();
    Serial.println("\n=== Status Update ===");
    Serial.print("WLAN: ");
    Serial.println(wifiConnected ? "Verbunden" : "Nicht verbunden");
    if (wifiConnected) {
      Serial.print("IP-Adresse: ");
      Serial.println(WiFi.localIP());
    }
    Serial.print("Spannung: ");
    Serial.print(voltage);
    Serial.println(" V");
    Serial.print("Strom: ");
    Serial.print(current);
    Serial.println(" A");
    Serial.print("Leistung: ");
    Serial.print(power);
    Serial.println(" W");
    Serial.print("Verbrauch: ");
    Serial.print(totalAh);
    Serial.println(" Ah");
    Serial.println("==================\n");
  }
  
  delay(100); // Kurze Pause
}

// Neue Funktion für Messwert-Aktualisierung
void updateMeasurements() {
  // Spannung am Akku messen (Differenzmodus Kanal 0-1)
  if (ads_voltage_ok) {
    int16_t adc0 = ads_voltage.readADC_Differential_0_1();
    Serial.print("Spannungs-ADC Rohwert: ");
    Serial.println(adc0);
    
    float rawVoltage = adc0 * 6.144 / 32767.0;  // 6.144V bei GAIN_TWOTHIRDS
    Serial.print("Rohe Spannung vor Teiler: ");
    Serial.print(rawVoltage, 4);
    Serial.println(" V");
    
    voltage = rawVoltage * VOLTAGE_DIVIDER;
    Serial.print("Berechnete Spannung: ");
    Serial.print(voltage, 2);
    Serial.println(" V");
    
    // Statistik aktualisieren
    if (voltage > maxVoltage) maxVoltage = voltage;
    if (voltage < minVoltage) minVoltage = voltage;
  } else {
    Serial.println("Spannungs-ADS nicht initialisiert!");
  }
  
  // Strom über Shunt messen (Differenzmodus Kanal 0-1)
  if (ads_current_ok) {
    int16_t adc1 = ads_current.readADC_Differential_0_1();
    float shuntVoltage = (adc1 * 0.256 / 32767.0);  // 0.256V bei GAIN_SIXTEEN
    float rawCurrent = shuntVoltage / SHUNT_RESISTOR;
    
    // Deadband für Strom anwenden
    if (abs(rawCurrent) < CURRENT_DEADBAND) {
      current = 0.0;  // Strom unter Deadband wird auf 0 gesetzt
    } else {
      current = rawCurrent;  // Strom über Deadband wird verwendet
    }
    
    // Statistik aktualisieren
    if (current > 0 && current > maxChargeCurrent) maxChargeCurrent = current;
    if (current < 0 && abs(current) > maxDischargeCurrent) maxDischargeCurrent = abs(current);
  }
  
  // Leistung berechnen
  power = voltage * current;
  
  // Statistik aktualisieren
  if (abs(power) > maxPower) maxPower = abs(power);
  
  // Amperestunden berechnen
  unsigned long currentTime = millis();
  float timeDiff = (currentTime - lastTime) / 3600000.0; // in Stunden
  ampereHours = current * timeDiff;
  totalAh += ampereHours;
  lastTime = currentTime;
  
  // Statistik aktualisieren
  if (ampereHours < 0 && abs(ampereHours) > maxConsumption) maxConsumption = abs(ampereHours);
  
  // Ladezustand berechnen
  stateOfCharge = 100.0 - (totalAh / batteryCapacity * 100.0);
  if(stateOfCharge < 0) stateOfCharge = 0;
  if(stateOfCharge > 100) stateOfCharge = 100;
  
  // Restkapazität in Ah berechnen
  remainingAh = batteryCapacity * (stateOfCharge / 100.0);
}

// Funktion zum Aktualisieren des Displays
void updateDisplay() {
  // Debug-Ausgabe
  Serial.println("Aktualisiere Display...");
  
  // Spannung
  tft.fillRect(10, 60, 460, 30, ST7796S_BLACK);
  tft.setCursor(10, 60);
  tft.setTextColor(ST7796S_WHITE);
  tft.print("Spannung: ");
  tft.print(voltage, 2);
  tft.print(" V");
  
  // Strom
  tft.fillRect(10, 100, 460, 30, ST7796S_BLACK);
  tft.setCursor(10, 100);
  tft.setTextColor(ST7796S_WHITE);
  tft.print("Strom: ");
  tft.print(current, 2);
  tft.print(" A");
  
  // Leistung
  tft.fillRect(10, 140, 460, 30, ST7796S_BLACK);
  tft.setCursor(10, 140);
  tft.setTextColor(ST7796S_WHITE);
  tft.print("Leistung: ");
  tft.print(power, 2);
  tft.print(" W");
  
  // Verbrauch
  tft.fillRect(10, 180, 460, 30, ST7796S_BLACK);
  tft.setCursor(10, 180);
  tft.setTextColor(ST7796S_WHITE);
  tft.print("Verbrauch: ");
  tft.print(totalAh, 2);
  tft.print(" Ah");
  
  // Ladezustand mit Farbkodierung
  tft.fillRect(10, 220, 460, 30, ST7796S_BLACK);
  tft.setCursor(10, 220);
  if (stateOfCharge > 50) {
    tft.setTextColor(ST7796S_GREEN);
  } else if (stateOfCharge > 20) {
    tft.setTextColor(ST7796S_YELLOW);
  } else {
    tft.setTextColor(ST7796S_RED);
  }
  tft.print("Ladezustand: ");
  tft.print(stateOfCharge, 1);
  tft.print(" %");
  
  // Restkapazität
  tft.fillRect(10, 260, 460, 30, ST7796S_BLACK);
  tft.setCursor(10, 260);
  tft.setTextColor(ST7796S_WHITE);
  tft.print("Restkapazitaet: ");
  tft.print(remainingAh, 2);
  tft.print(" Ah");
  
  // Status-Zeile
  tft.fillRect(10, 300, 460, 30, ST7796S_BLACK);
  tft.setCursor(10, 300);
  tft.setTextColor(ST7796S_WHITE);
  if (wifiConnected) {
    tft.print("WiFi: ");
    tft.print(WiFi.localIP());
  } else {
    tft.print("WiFi nicht verbunden");
  }
  
  Serial.println("Display aktualisiert");
}
