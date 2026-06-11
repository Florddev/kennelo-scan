#pragma once

enum ButtonEvent
{
    BTN_NONE,
    BTN_HOLD_SLEEP
};

void buttonBegin();
bool buttonCheckBtMode();
ButtonEvent buttonLoop();
void buttonGoToSleep();
