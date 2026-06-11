#include "button.h"
#include <Arduino.h>
#include "esp_sleep.h"

#define BUTTON_PIN          0
#define HOLD_SLEEP_MS       2500
#define BT_MODE_HOLD_MS     4000

static bool          _pressing  = false;
static unsigned long _pressStart = 0;

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
        _pressing = false;
        Serial.printf("[BTN] relâché après %lums\n", millis() - _pressStart);
    }

    if (_pressing && (millis() - _pressStart) >= HOLD_SLEEP_MS)
    {
        Serial.println("[BTN] maintien long → sleep");
        _pressing = false;
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
