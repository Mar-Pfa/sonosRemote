#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "esp_sleep.h"
#include "SonosUPnP.h"
#include "settings.h"

// ===== WLAN & Sonos =====
IPAddress SONOS_IP(192,168,2,228);
WiFiClient wifiClient;
SonosUPnP sonos(wifiClient);

// ===== mDNS & OTA =====
const char* HOSTNAME = "sonosremote";

// ===== Button Pins =====
#define BTN_DOWN 3
#define BTN_PLAY 4
#define BTN_UP   5

// ===== LED =====
#define LED_PIN 2 // onboard LED (change if different)

// LED state/timers
unsigned long ledFeedbackUntil = 0; // millis until which feedback LED stays on
unsigned long ledBlinkNext = 0;     // next toggle time for idle blink
bool ledBlinkOn = false;

// ===== LED Timing Constants (ms) =====
#define LED_STARTUP_ON_MS   100
#define LED_STARTUP_OFF_MS  150
#define LED_FEEDBACK_MS     500
#define LED_IDLE_ON_MS      100
#define LED_IDLE_OFF_MS     400
#define WIFI_CONNECT_TIMEOUT_MS 7000

// ===== Sleep Settings =====
const uint64_t AUTO_SLEEP_AFTER_MS = 60000;   // 30 Sekunden aktiv
RTC_DATA_ATTR int wakePin = -1;
unsigned long lastAction = 0;
bool actionDone = false;

// ===== Lautstärke Schrittweite =====
const int VOL_STEP = 5;

void setupButtons() {
  // Buttons are wired to GND; use internal pull-ups.
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

// --- Deep Sleep Setup ---
void goToSleep() {
  Serial.println("Gehe in Deep Sleep...");

  // Wakeup sources intentionally left default for widest compatibility

  // Ensure LED off before sleeping
  digitalWrite(LED_PIN, LOW);

  // Configure wakeup: use EXT0 on the volume-up button (BTN_UP).
  // Buttons are wired to GND, so we wake on LOW (pressed).
  // Note: EXT0 supports only a single RTC-capable GPIO. If this pin is not RTC-capable,
  // move the wake button to a supported pin or use an alternate wiring.
  // esp_deep_sleep_enable_gpio_wakeup((gpio_num_t)BTN_UP, 0);  
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN_UP, ESP_GPIO_WAKEUP_GPIO_LOW);
  
  Serial.printf("Configured EXT0 wake on pin %d (level LOW)\n", BTN_UP);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  delay(100);
  esp_deep_sleep_start();
}

// --- OTA Setup ---
void setupOTA() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA
    .onStart([]() { Serial.println("OTA Start"); })
    .onEnd([]() { Serial.println("\nOTA Ende"); })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Fortschritt: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Fehler[%u]: ", error);
    });
  ArduinoOTA.begin();
  Serial.println("OTA bereit.");
}

// --- mDNS Setup ---
void setupMDNS() {
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Fehler bei mDNS Start");
    return;
  }
  Serial.printf("mDNS aktiv: http://%s.local\n", HOSTNAME);
}

void setup() {
  Serial.begin(115200);
  delay(50);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  setupButtons();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ||
      wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    if (!digitalRead(BTN_DOWN)) wakePin = BTN_DOWN;
    else if (!digitalRead(BTN_UP)) wakePin = BTN_UP;
    else if (!digitalRead(BTN_PLAY)) wakePin = BTN_PLAY;
  } else {
    wakePin = -1;
  }

  Serial.printf("Wake reason: %d, pin: %d\n", wakeup_reason, wakePin);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  Serial.print("Verbinde WLAN");
  unsigned long start = millis();
  // Startup flashing until WiFi connects: LED_STARTUP_ON_MS on, LED_STARTUP_OFF_MS off
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    digitalWrite(LED_PIN, HIGH);
    delay(LED_STARTUP_ON_MS);
    digitalWrite(LED_PIN, LOW);
    delay(LED_STARTUP_OFF_MS);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) Serial.println(WiFi.localIP());
  else Serial.println("WLAN nicht verbunden");

  setupMDNS();
  setupOTA();

  // Aktion ausführen, falls durch Taste geweckt
  if (wakePin == BTN_DOWN) {
    Serial.println("→ Leiser");
    uint8_t cur = sonos.getVolume(SONOS_IP);
    int v = cur - VOL_STEP;
    if (v < 0) v = 0;
    Serial.printf("Calling Sonos %u.%u.%u.%u:%d SetVolume %d\n", SONOS_IP[0], SONOS_IP[1], SONOS_IP[2], SONOS_IP[3], UPNP_PORT, v);
    sonos.setVolume(SONOS_IP, (uint8_t)v);
    actionDone = true;
  } else if (wakePin == BTN_UP) {
    Serial.println("→ Lauter");
    uint8_t cur = sonos.getVolume(SONOS_IP);
    int v = cur + VOL_STEP;
    if (v > 100) v = 100;
    Serial.printf("Calling Sonos %u.%u.%u.%u:%d SetVolume %d\n", SONOS_IP[0], SONOS_IP[1], SONOS_IP[2], SONOS_IP[3], UPNP_PORT, v);
    sonos.setVolume(SONOS_IP, (uint8_t)v);
    actionDone = true;
  } else if (wakePin == BTN_PLAY) {
    Serial.println("→ Play/Pause");
    Serial.printf("Calling Sonos %u.%u.%u.%u:%d TogglePlayPause\n", SONOS_IP[0], SONOS_IP[1], SONOS_IP[2], SONOS_IP[3], UPNP_PORT);
    sonos.togglePause(SONOS_IP);
    actionDone = true;
  }

  // Give brief feedback LED when waking from standby + action
  if (actionDone) {
    ledFeedbackUntil = millis() + LED_FEEDBACK_MS; // feedback
    digitalWrite(LED_PIN, HIGH);
  }

  lastAction = millis();
}

void loop() {
  ArduinoOTA.handle();  // OTA im Wachzustand aktiv

  // Wenn Taste gedrückt → Aktion + Reset Timer
  bool downPressed = !digitalRead(BTN_DOWN);
  bool upPressed = !digitalRead(BTN_UP);
  bool playPressed = !digitalRead(BTN_PLAY);

  if (downPressed || upPressed || playPressed) {
    // Debug-Ausgabe welche Taste erkannt wurde
    Serial.print("Button pressed:");
    if (downPressed) Serial.print(" DOWN");
    if (upPressed) Serial.print(" UP");
    if (playPressed) Serial.print(" PLAY");
    Serial.println();

    lastAction = millis();

    // sofort sichtbares kurzes Feedback
    digitalWrite(LED_PIN, HIGH);
    delay(80);
    digitalWrite(LED_PIN, LOW);

    if (downPressed) {
      Serial.println("-> setVolume down");
      uint8_t cur = sonos.getVolume(SONOS_IP);
      Serial.printf(" cur=%u\n", cur);
      int v = cur - VOL_STEP; if (v < 0) v = 0;
      Serial.printf("Calling Sonos %u.%u.%u.%u:%d SetVolume %d\n", SONOS_IP[0], SONOS_IP[1], SONOS_IP[2], SONOS_IP[3], UPNP_PORT, v);
      sonos.setVolume(SONOS_IP, (uint8_t)v);
      Serial.println(" done");
    }
    if (upPressed) {
      Serial.println("-> setVolume up");
      uint8_t cur = sonos.getVolume(SONOS_IP);
      Serial.printf(" cur=%u\n", cur);
      int v = cur + VOL_STEP; if (v > 100) v = 100;
      Serial.printf("Calling Sonos %u.%u.%u.%u:%d SetVolume %d\n", SONOS_IP[0], SONOS_IP[1], SONOS_IP[2], SONOS_IP[3], UPNP_PORT, v);
      sonos.setVolume(SONOS_IP, (uint8_t)v);
      Serial.println(" done");
    }
    if (playPressed) {
      Serial.println("-> togglePause");
      Serial.printf("Calling Sonos %u.%u.%u.%u:%d TogglePlayPause\n", SONOS_IP[0], SONOS_IP[1], SONOS_IP[2], SONOS_IP[3], UPNP_PORT);
      sonos.togglePause(SONOS_IP);
      Serial.println(" done");
    }

    // Start feedback LED for configured duration
    ledFeedbackUntil = millis() + LED_FEEDBACK_MS;
    digitalWrite(LED_PIN, HIGH);
    delay(250); // Entprellen
  }

  unsigned long now = millis();

  // turn off feedback LED when time expired
  if (ledFeedbackUntil && now >= ledFeedbackUntil) {
    ledFeedbackUntil = 0;
    digitalWrite(LED_PIN, LOW);
  }

  // While active and waiting for going to standby show idle blink pattern:
  // 0.1s on, 0.4s off
  if (now - lastAction > 1000 && now - lastAction < AUTO_SLEEP_AFTER_MS) {
    if (now >= ledBlinkNext) {
      if (!ledBlinkOn) {
        // turn on for LED_IDLE_ON_MS
        digitalWrite(LED_PIN, HIGH);
        ledBlinkOn = true;
        ledBlinkNext = now + LED_IDLE_ON_MS;
      } else {
        // turn off for LED_IDLE_OFF_MS
        digitalWrite(LED_PIN, LOW);
        ledBlinkOn = false;
        ledBlinkNext = now + LED_IDLE_OFF_MS;
      }
    }
  } else {
    // ensure idle blink state reset when recent activity or sleeping
    ledBlinkOn = false;
    ledBlinkNext = now;
  }

  // Nach AUTO_SLEEP_AFTER_MS ohne Aktion → Sleep (ensure LED off)
  if (now - lastAction > AUTO_SLEEP_AFTER_MS) {
    digitalWrite(LED_PIN, LOW);
    goToSleep();
  }

  delay(10);
}
