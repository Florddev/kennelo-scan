#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

// ── Pins I2C — à adapter selon votre câblage ──────────────────────────────
#define OLED_SDA  8
#define OLED_SCL  9
// ──────────────────────────────────────────────────────────────────────────

#define SCREEN_W          128
#define SCREEN_H           32
#define OLED_ADDR        0x3C
#define ACTION_SHOW_MS   3000
#define DRAW_INTERVAL_MS  100

// Position des éléments de la barre de statut
#define X_WIFI    62
#define X_BT      74
#define X_BAT     90

static Adafruit_SSD1306 _oled(SCREEN_W, SCREEN_H, &Wire, -1);
static bool _ready = false;

static char          _code[9]   = "";
static bool          _bt        = false;
static bool          _wifi      = false;
static int           _battery   = -1;
static bool          _charging  = false;
static int           _queueSz   = 0;
static DisplayAction _action    = DISP_IDLE;
static char          _tag[16]   = "";
static unsigned long _actionAt  = 0;
static unsigned long _lastDraw  = 0;

// ── Init ──────────────────────────────────────────────────────────────────

void displayBegin(const char *scannerCode)
{
    strncpy(_code, scannerCode, 8);
    _code[8] = '\0';

    Wire.begin(OLED_SDA, OLED_SCL);
    if (!_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[OLED] erreur init");
        return;
    }
    _oled.clearDisplay();
    _oled.display();
    _ready = true;
    Serial.println("[OLED] init OK");
}

// ── Setters ───────────────────────────────────────────────────────────────

void displaySetBtMode(bool active)         { _bt = active; }
void displaySetWifiConnected(bool on)      { _wifi = on; }
void displaySetBattery(int pct, bool chg)  { _battery = pct; _charging = chg; }
void displaySetQueueSize(int n)            { _queueSz = n; }

void displaySetAction(DisplayAction action, const char *tag)
{
    _action  = action;
    _actionAt = millis();
    _lastDraw = 0; // force redraw immédiat
    if (tag) {
        strncpy(_tag, tag, 15);
        _tag[15] = '\0';
    }
}

// ── Helpers graphiques ────────────────────────────────────────────────────

// 4 barres de signal (6×6px) représentant la connexion WiFi
static void _drawWifi(int16_t x, int16_t y)
{
    if (_wifi) {
        for (int i = 0; i < 4; i++) {
            int h  = i + 2;
            int bx = x + i * 2;
            int by = y + (6 - h);
            _oled.drawFastVLine(bx, by, h, WHITE);
        }
    } else {
        // Croix 7×7
        _oled.drawLine(x, y,     x + 6, y + 6, WHITE);
        _oled.drawLine(x, y + 6, x + 6, y,     WHITE);
    }
}

// Symbole Bluetooth ᛒ en bitmap 8×8
// col:  0 1 2 3 4 5 6 7
//       . . . X . . . .   row 0
//       . . . X X . . .   row 1
//       . . X . . X . .   row 2
//       . . . X X . . .   row 3
//       . . . X X . . .   row 4
//       . . X . . X . .   row 5
//       . . . X X . . .   row 6
//       . . . X . . . .   row 7
static const uint8_t PROGMEM _iconBT[] = {
    0x10, 0x18, 0x24, 0x18,
    0x18, 0x24, 0x18, 0x10,
};

static void _drawBt(int16_t x, int16_t y)
{
    if (_bt)
        _oled.drawBitmap(x, y, _iconBT, 8, 8, WHITE);
}

// Rectangle batterie (30×8) + capuchon (3×4) + remplissage proportionnel
static void _drawBattery(int16_t x, int16_t y)
{
    if (_battery < 0) {
        _oled.setCursor(x, y);
        _oled.print("---");
        return;
    }

    _oled.drawRect(x, y, 30, 8, WHITE);
    _oled.fillRect(x + 30, y + 2, 3, 4, WHITE); // capuchon

    int fill = (_battery * 26) / 100;
    if (fill > 0)
        _oled.fillRect(x + 2, y + 2, fill, 4, WHITE);

    // Éclair de charge centré sur le corps
    if (_charging) {
        _oled.setCursor(x + 10, y);
        _oled.print("+");
    }
}

// ── Barre de statut (ligne 1, y=0..8) ────────────────────────────────────

static void _drawStatusBar()
{
    _oled.setTextSize(1);
    _oled.setTextColor(WHITE);

    _oled.setCursor(0, 0);
    _oled.print(_code);

    _drawWifi(X_WIFI, 0);
    _drawBt(X_BT, 0);
    _drawBattery(X_BAT, 0);

    _oled.drawLine(0, 9, SCREEN_W - 1, 9, WHITE);
}

// ── Zone action (ligne 2, y=11..31) ──────────────────────────────────────

static void _drawAction()
{
    // Retour automatique à l'idle pour les états transitoires
    bool transient = (_action == DISP_SCAN_DETECTED ||
                      _action == DISP_SENDING       ||
                      _action == DISP_SENT_OK        ||
                      _action == DISP_SENT_FAIL);

    if (transient && (millis() - _actionAt) > ACTION_SHOW_MS)
        _action = DISP_IDLE;

    _oled.setTextSize(1);
    _oled.setTextColor(WHITE);

    char buf[32];

    switch (_action) {

        case DISP_IDLE:
            if (_bt) {
                _oled.setCursor(0, 12);
                _oled.print("Mode Bluetooth");
                _oled.setCursor(0, 22);
                _oled.print("KNN-");
                _oled.print(_code);
            } else if (_queueSz > 0) {
                _oled.setCursor(0, 12);
                _oled.print("Pret a scanner");
                snprintf(buf, sizeof(buf), "[%d] en attente", _queueSz);
                _oled.setCursor(0, 22);
                _oled.print(buf);
            } else {
                _oled.setCursor(0, 17);
                _oled.print("Pret a scanner...");
            }
            break;

        case DISP_SCAN_DETECTED:
            _oled.setCursor(0, 12);
            _oled.print("Tag detecte !");
            _oled.setCursor(0, 22);
            _oled.print(_tag);
            break;

        case DISP_SENDING:
            _oled.setCursor(0, 12);
            _oled.print("Envoi en cours...");
            _oled.setCursor(0, 22);
            _oled.print(_tag);
            break;

        case DISP_SENT_OK:
            _oled.setCursor(0, 12);
            _oled.print("Envoye !");
            _oled.setCursor(0, 22);
            _oled.print(_tag);
            break;

        case DISP_SENT_FAIL:
            _oled.setCursor(0, 12);
            _oled.print("En file d'attente");
            _oled.setCursor(0, 22);
            _oled.print(_tag);
            break;

        case DISP_SLEEPING:
            _oled.setCursor(0, 17);
            _oled.print("Mise en veille...");
            break;
    }
}

// ── Tick principal ────────────────────────────────────────────────────────

void displayTick()
{
    if (!_ready) return;
    if (millis() - _lastDraw < DRAW_INTERVAL_MS) return;
    _lastDraw = millis();

    _oled.clearDisplay();
    _drawStatusBar();
    _drawAction();
    _oled.display();
}
