👋 Hallo! Ich bin Eric (ericder92), ein begeisterter Bastler ohne professionelle Programmierkenntnisse.

⚡ Die Projekte hier – wie z. B. mein ESP32 Batterie-Monitor – sind reine Hobby-Basteleien.

Ich habe mir alles selbst beigebracht und teile meine Ergebnisse, damit andere davon profitieren oder mitbasteln können. 

💬 Mein Englisch ist nicht perfekt, und auch meine Programmierung ist eher "learning by doing" – aber mit viel Leidenschaft.

Alles, was du hier findest, darfst du gerne frei verwenden – nur bitte nicht kommerziell verkaufen.

🔧 Viel Spaß beim Mitbasteln!


👋 Hi! I'm Eric (ericder92), a passionate tinkerer with no professional coding background.

⚡ Everything here – like my ESP32 Battery Monitor – is part of a personal DIY hobby project.

I learned everything by doing, and I share my code so others can benefit or build their own versions.

💬 My English isn't perfect, and my code is more "trial and error" – but always with heart.

You’re free to use everything here – just please don’t sell it commercially.

🔧 Have fun tinkering! 

# ESP32 Batterie-Monitor mit Webinterface

Dieses Projekt ist ein smarter Batterie-Monitor auf Basis eines ESP32 D1 Mini. Er misst in Echtzeit Spannung, Strom und die verbleibende Kapazität einer Batterie – ganz ohne Display. Die Bedienung und Anzeige erfolgt vollständig über eine Weboberfläche im lokalen Netzwerk.

## Features

- **Zwei ADS1115 (I2C)**:
  - Einer misst die Batteriespannung (über Spannungsteiler).
  - Der andere misst die Spannung über einen Shunt-Widerstand zur Strommessung.
- **I2C-Erkennung**:
  - Die Webseite gibt eine Warnung aus, wenn ein ADS1115 nicht erkannt wird.
- **Strom- und Spannungsmessung** in Echtzeit
- **Berechnung der verbleibenden Batteriekapazität**
- **Alle Parameter über die Weboberfläche einstellbar**:
  - Spannungsteiler-Verhältnis
  - Shunt-Widerstand (in mOhm)
  - Anfangskapazität der Batterie (z. B. 100Ah)
- **Unterspannungswarnung** bei Unterschreiten eines frei definierbaren Schwellenwerts
- **Webinterface statt Display**:
  - Darstellung und Konfiguration erfolgen per Browser
- **Statistik und Systemüberblick**:
  - Maximal gemessene Spannung
  - Minimal gemessene Spannung
  - Maximaler Entlade- und Ladestrom
  - Aktuelle Leistung in Watt
  - Betriebszeit

## Hardware-Voraussetzungen

- ESP32 D1 Mini
- 2x ADS1115 ADCs (I2C)
- Shunt-Widerstand zur Strommessung
- Batterie
- WLAN-Zugang (Router oder Hotspot)

### I2C-Adressen & Verdrahtung der ADS1115

Die beiden ADS1115 benötigen unterschiedliche I2C-Adressen. Diese werden durch die Verbindung des **ADDR-Pins** bestimmt:

| ADDR-Pin verbunden mit | I2C-Adresse | Verwendung             |
|------------------------|-------------|------------------------|
| GND                    | `0x48`      | Strommessung (Shunt)   |
| SCL                    | `0x4B`      | Batteriespannung       |

Standardmäßige I2C-Pins am ESP32 D1 Mini:
- **SDA**: GPIO 21
- **SCL**: GPIO 22

## Benötigte Bibliotheken

Bitte stelle sicher, dass folgende Bibliotheken in der Arduino IDE installiert sind:

- `Wire.h` *(I2C-Kommunikation)*
- `SPI.h` *(für interne Kommunikation, z. B. Flash oder externe Module)*
- `WiFi.h` *(WLAN-Anbindung des ESP32)*
- `WebServer.h` *(für das Webinterface)*
- `EEPROM.h` *(für das Speichern von Einstellungen)*
- `Adafruit_ADS1X15.h` *(zur Ansteuerung der ADS1115 ADCs)*
- `Adafruit_ST7796S_kbv.h` *(wird derzeit nicht mehr verwendet, ist aber noch im Code enthalten)*

## Einrichtung

1. Öffne den Arduino-Sketch (`.ino`) in der Arduino IDE.
2. Installiere die oben genannten Bibliotheken über den Bibliotheksverwalter.
3. Trage deine WLAN-Zugangsdaten in den Sketch ein.
4. Lade den Sketch auf deinen ESP32 D1 Mini.
5. Öffne die serielle Konsole, um die IP-Adresse des ESP32 zu ermitteln.
6. Gib die IP-Adresse im Browser ein – die Weboberfläche wird angezeigt.

## Webinterface

Die Weboberfläche ermöglicht:
- Live-Anzeige von Spannung, Strom, Leistung und Restkapazität
- Konfiguration von:
  - Spannungsteiler-Verhältnis
  - Shunt-Widerstand (in mOhm)
  - Anfangskapazität der Batterie
  - Unterspannungsschwelle
- Anzeige von Systemmeldungen (z. B. I2C-Fehler, Unterspannung)

## Beispiel-Webseite

So sieht die Statistik-Ansicht in der Weboberfläche aus:

![Webinterface Screenshot](screenshot.jpg)


## To-Do / Weiterentwicklungsideen

- Datenexport (z. B. CSV)
- MQTT-Anbindung für Home Assistant oder Node-RED
- Speicherung der Statistik in SPIFFS oder auf SD-Karte
- Responsives Design für Smartphone-Nutzung

## Lizenz

Dieses Projekt steht unter der [MIT License](LICENSE) und ist vollständig Open Source.

**Hinweis des Autors:** Ich freue mich, wenn dieses Projekt frei bleibt, weiterentwickelt wird und anderen hilft. Auch wenn die Lizenz kommerzielle Nutzung erlaubt, wünsche ich mir, dass es **nicht einfach nur verkauft**, sondern offen geteilt wird.

Copyright (c) 2025 [ericder92](https://github.com/ericder92)
