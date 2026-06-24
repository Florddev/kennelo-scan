#include "button.h"
#include <Arduino.h>
#include "esp_sleep.h"

#define BUTTON_PIN          0
#define HOLD_SLEEP_MS       2500
#define BT_MODE_HOLD_MS     4000
#define CLICK_MAX_MS         400
#define SINGLE_WIN_MS        600  // délai de confirmation simple clic (pas de 2ème appui attendu)
#define TRIPLE_WIN_MS       1000

static bool          _pressing      = false;
static unsigned long _pressStart    = 0;
static int           _clickCount    = 0;
static unsigned long _firstClickAt  = 0;

void buttonBegin()
{
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    esp_deep_sleep_enable_gpio_wakeup(BIT(BUTTON_PIN), ESP_GPIO_WAKEUP_GPIO_LOW);
    Serial.printf("[BTN] init GPIO%d, état initial: %s\n", BUTTON_PIN, digitalRead(BUTTON_PIN) == LOW ? "LOW (appuyé??)" : "HIGH (repos OK)");
}

bool buttonCheckBtMode()
{
    if (digitalRead(BUTTON_PIN) != LOW)
    {
        Serial.println("[BTN] démarrage normal (bouton non appuyé)");
        return false;
    }

    Serial.println("[BTN] bouton tenu au démarrage, attente relâchement...");
    unsigned long holdStart = millis();
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);

    unsigned long held = millis() - holdStart;
    Serial.printf("[BTN] tenu %lums (seuil BT = %dms)\n", held, BT_MODE_HOLD_MS);
    return held >= BT_MODE_HOLD_MS;
}

ButtonEvent buttonLoop()
{
    bool pressed = (digitalRead(BUTTON_PIN) == LOW);

    if (pressed && !_pressing)
    {
        _pressing   = true;
        _pressStart = millis();
        Serial.println("[BTN] appui détecté");
    }
    else if (!pressed && _pressing)
    {
        unsigned long held = millis() - _pressStart;
        _pressing = false;
        Serial.printf("[BTN] relâché après %lums\n", held);

        if (held < CLICK_MAX_MS)
        {
            if (_clickCount == 0) _firstClickAt = millis();
            _clickCount++;
            Serial.printf("[BTN] click %d/3\n", _clickCount);
            if (_clickCount >= 3)
            {
                _clickCount = 0;
                return BTN_TRIPLE_CLICK;
            }
        }
    }

    // Simple clic confirmé si aucun 2ème appui dans SINGLE_WIN_MS
    if (_clickCount == 1 && (millis() - _firstClickAt) > SINGLE_WIN_MS)
    {
        _clickCount = 0;
        Serial.println("[BTN] simple clic confirmé");
        return BTN_SINGLE_CLICK;
    }
    // Fenêtre triple-click expirée sans atteindre 3 clics
    if (_clickCount > 1 && (millis() - _firstClickAt) > TRIPLE_WIN_MS)
    {
        Serial.printf("[BTN] fenêtre expirée (%d clics ignorés)\n", _clickCount);
        _clickCount = 0;
    }

    if (_pressing && (millis() - _pressStart) >= HOLD_SLEEP_MS)
    {
        Serial.println("[BTN] maintien long → sleep");
        _pressing   = false;
        _clickCount = 0;
        return BTN_HOLD_SLEEP;
    }

    return BTN_NONE;
}

void buttonGoToSleep()
{
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    delay(50);
    esp_deep_sleep_start();
}
