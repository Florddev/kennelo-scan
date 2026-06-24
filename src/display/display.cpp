#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <time.h>

// ── Pins I2C — à adapter selon votre câblage ──────────────────────────────
#define OLED_SDA  8
#define OLED_SCL  9
// ──────────────────────────────────────────────────────────────────────────

#define SCREEN_W         128
#define SCREEN_H          32
#define OLED_ADDR       0x3C
#define ACTION_SHOW_MS  3000
#define DRAW_INTERVAL_MS 100
#define WIFI_ANIM_MS     350   // vitesse d'animation des barres WiFi
#define FLUSH_ANIM_MS    400   // vitesse d'animation des points de chargement file
#define RFID_ANIM_MS     450   // vitesse d'animation pulse RFID (ms par arc)

// ── Layout sans séparateur : texte depuis haut-gauche ─────────────────────
#define LINE1_Y        1   // première ligne (size-1)
#define LINE2_Y       12   // deuxième ligne (size-1)
#define LINE_IDLE_Y   12   // ligne unique (centrée verticalement)
#define LINE_BIG_Y     0   // nom animal (size-2, y=0..15)
#define LINE_SUB_Y    17   // sous-titre après size-2 (y=17..24)
// ──────────────────────────────────────────────────────────────────────────

static Adafruit_SSD1306 _oled(SCREEN_W, SCREEN_H, &Wire, -1);
static bool _ready = false;

static char          _code[9]        = "";
static bool          _bt             = false;
static bool          _wifi           = false;
static bool          _wifiScanning   = false;
static int           _wifiRssi       = -100;
static int           _wifiAnimBars   = 1;
static unsigned long _wifiAnimAt     = 0;
static bool          _flushing       = false;
static int           _flushAnimDot   = 0;
static unsigned long _flushAnimAt    = 0;
static int           _rfidAnimFrame  = 0;
static unsigned long _rfidAnimAt     = 0;
static int           _queueSz        = 0;
static DisplayAction _action         = DISP_IDLE;
// ── Historique des scans ──────────────────────────────────────────────────
static int           _histPos        = 0;
static int           _histTotal      = 0;
static bool          _histFound      = false;
static time_t        _histTs         = 0;
static char          _histTag[16]    = "";
static char          _histName[32]   = "";
static char          _histSpec[32]   = "";
static char          _histBreed[48]  = "";
static char          _tag[16]        = "";
static char          _animalName[32] = "";
static char          _animalSpec[32] = "";
static char          _animalBreed[48]= "";
static unsigned long _actionAt       = 0;
static unsigned long _lastDraw       = 0;

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
void displaySetWifiScanning(bool scanning) { _wifiScanning = scanning; }
void displaySetWifiRssi(int rssi)          { _wifiRssi = rssi; }
void displaySetFlushing(bool flushing)     { _flushing = flushing; }
void displaySetQueueSize(int n)            { _queueSz = n; }

void displaySetHistoryEntry(int pos, int total, const char *tag,
                             const char *name, const char *species, const char *breed,
                             time_t timestamp, bool found)
{
    _histPos   = pos;
    _histTotal = total;
    _histFound = found;
    _histTs    = timestamp;
    strncpy(_histTag,   tag    ? tag    : "", sizeof(_histTag)   - 1);
    strncpy(_histName,  name   ? name   : "", sizeof(_histName)  - 1);
    strncpy(_histSpec,  species? species: "", sizeof(_histSpec)  - 1);
    strncpy(_histBreed, breed  ? breed  : "", sizeof(_histBreed) - 1);
}

void displaySetAction(DisplayAction action, const char *tag)
{
    _action   = action;
    _actionAt = millis();
    _lastDraw = 0;
    if (tag) {
        strncpy(_tag, tag, 15);
        _tag[15] = '\0';
    }
}

void displaySetAnimalResult(bool found, const char *name, const char *species, const char *breed)
{
    _action   = found ? DISP_ANIMAL_FOUND : DISP_ANIMAL_UNKNOWN;
    _actionAt = millis();
    _lastDraw = 0;
    strncpy(_animalName,  name    ? name    : "", sizeof(_animalName)  - 1);
    strncpy(_animalSpec,  species ? species : "", sizeof(_animalSpec)  - 1);
    strncpy(_animalBreed, breed   ? breed   : "", sizeof(_animalBreed) - 1);
}

// ── Helpers graphiques ────────────────────────────────────────────────────

// Icône WiFi 10×9px
//   Connecté   : 4 barres pleines (hauteurs 3/5/7/9px, espacées 3px)
//   Scan animé : N barres (N = 1..4 cyclique, une barre toutes les WIFI_ANIM_MS)
//   Déconnecté : croix 10×9px
static void _drawWifi(int16_t x, int16_t y)
{
    int bars = 0;

    if (_wifi) {
        // Nombre de barres selon le RSSI
        if      (_wifiRssi >= -55) bars = 4;
        else if (_wifiRssi >= -65) bars = 3;
        else if (_wifiRssi >= -75) bars = 2;
        else                       bars = 1;
    } else if (_wifiScanning)
        bars = _wifiAnimBars;
    else {
        // Croix (déconnecté, pas en scan)
        _oled.drawLine(x,     y,     x + 9, y + 8, WHITE);
        _oled.drawLine(x,     y + 8, x + 9, y,     WHITE);
        return;
    }

    for (int i = 0; i < bars; i++) {
        int h  = i * 2 + 3;        // 3, 5, 7, 9 px
        int bx = x + i * 3;        // espacées de 3px
        int by = y + (9 - h);      // alignées par le bas
        _oled.drawFastVLine(bx, by, h, WHITE);
    }
}

// Symbole Bluetooth vectoriel pleine hauteur (bounding box w×h à partir de (x,y))
//
//          cx
//          |  \  rx (tip droit)
//  x  \    |   \
//      \   |    >  q1
//       \  |   /
//          |  /
//          | /
//          |        mid
//          | \
//          |  \
//           \  >  q3
//            \ |
//       x /  |
//          | /
//          |
static void _drawBtLarge(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t cx  = x + w / 3;       // tige légèrement à gauche du centre
    int16_t rx  = x + w - 1;       // pointe droite
    int16_t top = y;
    int16_t bot = y + h - 1;
    int16_t mid = y + h / 2;
    int16_t q1  = y + h / 4;
    int16_t q3  = y + 3 * h / 4;

    // Tige centrale (2px pour la visibilité)
    _oled.drawFastVLine(cx,     top, h, WHITE);
    _oled.drawFastVLine(cx + 1, top, h, WHITE);

    // Losange haut-droite
    _oled.drawLine(cx, top, rx, q1,  WHITE);
    _oled.drawLine(rx, q1,  cx, mid, WHITE);

    // Losange bas-droite
    _oled.drawLine(cx, mid, rx, q3,  WHITE);
    _oled.drawLine(rx, q3,  cx, bot, WHITE);

    // Extensions haut-gauche et bas-gauche (les "serifs" du symbole BT)
    _oled.drawLine(cx, q1, x, top, WHITE);
    _oled.drawLine(cx, q3, x, bot, WHITE);
}

// Paire d'arcs RFID gauche+droite, clippée verticalement à ±ylimit pixels du centre.
// Sans ce clip, les deux demi-cercles se rejoignent et forment un cercle complet (effet cible).
// Bresenham : pixels "proches du horizontal" (cx±y, cy±x) toujours dessinés ;
//             pixels "proches du haut/bas"   (cx±x, cy±y) dessinés seulement si y ≤ ylimit.
static void _drawRfidArcPair(int16_t cx, int16_t cy, int16_t r, int16_t ylimit)
{
    int16_t x = 0, y = r, d = 1 - r;
    while (x <= y) {
        // Partie horizontale (extrémités gauche/droite des arcs) — toujours tracée
        _oled.drawPixel(cx + y, cy - x, WHITE);
        _oled.drawPixel(cx + y, cy + x, WHITE);
        _oled.drawPixel(cx - y, cy - x, WHITE);
        _oled.drawPixel(cx - y, cy + x, WHITE);
        // Partie haut/bas — clippée pour ouvrir les arcs et éviter l'effet cible
        if (y <= ylimit) {
            _oled.drawPixel(cx + x, cy - y, WHITE);
            _oled.drawPixel(cx + x, cy + y, WHITE);
            _oled.drawPixel(cx - x, cy - y, WHITE);
            _oled.drawPixel(cx - x, cy + y, WHITE);
        }
        if (d < 0) d += 2 * x + 3;
        else { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

static void _drawRfidIcon(int16_t cx, int16_t cy, int frames)
{
    _oled.fillCircle(cx, cy, 2, WHITE);

    if (frames >= 1) _drawRfidArcPair(cx, cy,  8,  6);
    if (frames >= 2) _drawRfidArcPair(cx, cy, 13, 10);
    if (frames >= 3) _drawRfidArcPair(cx, cy, 18, 13);
}

// Avertissement file d'attente : count puis triangle warning, en haut à gauche
static void _drawQueueWarning()
{
    char   qbuf[8];
    int    qlen;

    if (_flushing && _wifi) {
        static const char *dots[] = {"", ".", "..", "..."};
        qlen = snprintf(qbuf, sizeof(qbuf), "%d%s", _queueSz, dots[_flushAnimDot]);
    } else {
        qlen = snprintf(qbuf, sizeof(qbuf), "%d", _queueSz);
    }

    const int16_t ICON_W = 10;
    const int16_t GAP    = 2;

    // Nombre à gauche en haut
    _oled.setTextSize(1);
    _oled.setCursor(0, 1);
    _oled.print(qbuf);

    // Triangle warning 10×9px à droite du nombre
    int16_t ix = qlen * 6 + GAP;
    int16_t iy = 0;
    _oled.drawLine(ix + 4, iy,     ix,      iy + 8, WHITE);
    _oled.drawLine(ix + 4, iy,     ix + 9,  iy + 8, WHITE);
    _oled.drawLine(ix,     iy + 8, ix + 9,  iy + 8, WHITE);
    _oled.drawFastVLine(ix + 4, iy + 3, 3, WHITE);
    _oled.drawPixel(ix + 4, iy + 7, WHITE);
    (void)ICON_W;
}

// ── Icône statut en haut à droite (WiFi uniquement — BT géré dans _drawAction) ──

static void _drawStatusBar()
{
    if (_bt) return;
    _drawWifi(SCREEN_W - 11, 0);
}

// ── Contenu (haut-gauche, sans séparateur) ────────────────────────────────

static void _drawAction()
{
    bool transient = (_action == DISP_SCAN_DETECTED  ||
                      _action == DISP_SENDING        ||
                      _action == DISP_SENT_OK         ||
                      _action == DISP_SENT_FAIL       ||
                      _action == DISP_ANIMAL_FOUND    ||
                      _action == DISP_ANIMAL_UNKNOWN);

    if (transient && (millis() - _actionAt) > ACTION_SHOW_MS)
        _action = DISP_IDLE;

    _oled.setTextColor(WHITE);
    _oled.setTextSize(1);   // reset systématique — évite l'héritage du size-2 du mode BT
    char buf[32];

    switch (_action) {

        case DISP_IDLE:
            if (_bt) {
                // "Mode Bluetooth" en size-1 tout en haut
                _oled.setTextSize(1);
                _oled.setCursor(0, 0);
                _oled.print("Mode Bluetooth");
                // Code du scanner en size-2 en dessous
                _oled.setTextSize(2);
                _oled.setCursor(0, 13);
                _oled.print(_code);
                // Symbole BT réduit, centré verticalement à droite
                _drawBtLarge(108, 4, 18, 24);
            } else {
                // Icône RFID animée centrée + warning file en haut à gauche
                _drawRfidIcon(64, 16, _rfidAnimFrame);
                if (_queueSz > 0)
                    _drawQueueWarning();
            }
            break;

        case DISP_SCAN_DETECTED:
            _oled.setTextSize(1);
            _oled.setCursor(0, LINE1_Y);
            _oled.print("Tag detecte !");
            _oled.setCursor(0, LINE2_Y);
            _oled.print(_tag);
            break;

        case DISP_SENDING:
            _oled.setCursor(0, LINE1_Y);
            _oled.print("Chargement...");
            _oled.setCursor(0, LINE2_Y);
            _oled.print(_tag);
            break;

        case DISP_SENT_OK:
            _oled.setTextSize(1);
            _oled.setCursor(0, LINE1_Y);
            _oled.print("Envoye !");
            _oled.setCursor(0, LINE2_Y);
            _oled.print(_tag);
            break;

        case DISP_SENT_FAIL:
            _oled.setTextSize(1);
            _oled.setCursor(0, LINE1_Y);
            _oled.print("En file d'attente");
            _oled.setCursor(0, LINE2_Y);
            _oled.print(_tag);
            break;

        case DISP_ANIMAL_FOUND:
            // Nom en size-2 sur la première ligne
            _oled.setTextSize(2);
            _oled.setCursor(0, LINE_BIG_Y);
            _oled.print(_animalName);
            // Espèce + race en size-1 en dessous
            _oled.setTextSize(1);
            snprintf(buf, sizeof(buf), "%s - %s", _animalSpec, _animalBreed);
            _oled.setCursor(0, LINE_SUB_Y);
            _oled.print(buf);
            break;

        case DISP_ANIMAL_UNKNOWN:
            _oled.setTextSize(1);
            _oled.setCursor(0, LINE1_Y);
            _oled.print("Animal inconnu");
            _oled.setCursor(0, LINE2_Y);
            _oled.print(_tag);
            break;

        case DISP_HISTORY: {
            // Ligne 1 : #pos/total  HH:MM:SS
            if (_histTs > 0) {
                struct tm t;
                gmtime_r(&_histTs, &t);
                snprintf(buf, sizeof(buf), "#%d/%d  %02d:%02d:%02d",
                         _histPos, _histTotal, t.tm_hour, t.tm_min, t.tm_sec);
            } else {
                snprintf(buf, sizeof(buf), "#%d/%d", _histPos, _histTotal);
            }
            _oled.setCursor(0, LINE1_Y);
            _oled.print(buf);
            // Ligne 2 : nom + race, ou "Animal inconnu"
            if (_histName[0]) {
                if (_histBreed[0])
                    snprintf(buf, sizeof(buf), "%s - %s", _histName, _histBreed);
                else
                    snprintf(buf, sizeof(buf), "%s", _histName);
            } else {
                snprintf(buf, sizeof(buf), "Animal inconnu");
            }
            _oled.setCursor(0, LINE2_Y);
            _oled.print(buf);
            // Ligne 3 : numéro de microchip
            _oled.setCursor(0, 23);
            _oled.print(_histTag);
            break;
        }

        case DISP_NOT_REGISTERED:
            _oled.setTextSize(1);
            _oled.setCursor(0, LINE1_Y);
            _oled.print("Scanner non lie...");
            _oled.setCursor(0, LINE2_Y);
            _oled.print(_code);
            break;

        case DISP_SLEEPING:
            _oled.setTextSize(1);
            _oled.setCursor(0, LINE_IDLE_Y);
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

    // Avance l'animation WiFi (barres qui s'allument une par une)
    if (_wifiScanning && !_wifi && (millis() - _wifiAnimAt) > WIFI_ANIM_MS) {
        _wifiAnimAt   = millis();
        _wifiAnimBars = (_wifiAnimBars % 4) + 1;  // cycle 1 → 2 → 3 → 4 → 1
    }

    // Avance l'animation de chargement de la file (points cycliques)
    if (_flushing && _wifi && (millis() - _flushAnimAt) > FLUSH_ANIM_MS) {
        _flushAnimAt = millis();
        _flushAnimDot = (_flushAnimDot + 1) % 4;  // cycle 0 → 1 → 2 → 3 → 0
    }

    // Avance l'animation pulse RFID (écran idle WiFi uniquement)
    if (!_bt && (millis() - _rfidAnimAt) > RFID_ANIM_MS) {
        _rfidAnimAt   = millis();
        _rfidAnimFrame = (_rfidAnimFrame + 1) % 4;  // 0=dot, 1=inner, 2=mid, 3=outer
    }

    _oled.clearDisplay();
    _drawAction();      // texte d'abord
    _drawStatusBar();   // icônes par-dessus (priorité visuelle)
    _oled.display();
}

// ── Extinction de l'écran avant deep sleep ────────────────────────────────

void displayOff()
{
    if (!_ready) return;
    _oled.clearDisplay();
    _oled.display();
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(0xAE);
    Wire.endTransmission();
    _ready = false;
}
