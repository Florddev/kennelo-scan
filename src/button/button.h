#pragma once

enum ButtonEvent
{
    BTN_NONE,
    BTN_SINGLE_CLICK,
    BTN_HOLD_SLEEP,
    BTN_TRIPLE_CLICK
};

void buttonBegin();
bool buttonCheckBtMode();
ButtonEvent buttonLoop();
void buttonGoToSleep();
