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
}

bool buttonCheckBtMode()
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_GPIO) return false;
    if (digitalRead(BUTTON_PIN) != LOW) return false;

    unsigned long holdStart = millis();
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);

    return (millis() - holdStart) >= BT_MODE_HOLD_MS;
}

ButtonEvent buttonLoop()
{
    bool pressed = (digitalRead(BUTTON_PIN) == LOW);

    if (pressed && !_pressing)
    {
        _pressing   = true;
        _pressStart = millis();
    }
    else if (!pressed && _pressing)
    {
        _pressing = false;
    }

    if (_pressing && (millis() - _pressStart) >= HOLD_SLEEP_MS)
    {
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
