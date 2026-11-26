/*
  Mega 4.3 version 1.0. Freenove FNK0115B, 4.3 inch TN, 480x272.
  This is not the Waveshare 7 inch. Different size, pins, touch, clock, and bounce.
  Clock 9000000. Bounce 272 * 20. Rotation 2. Porches stay 0,8,4,43 and 0,8,4,12.
  XPT2046 touch. SPI.begin before ts.begin. Two samples, 4 ms apart.
  Raw map 3800 to 170, 250 to 3800, then flip both axes.
  Do not put the 7 inch clock, bounce, porch, or GT911 touch on this glass.
  JOIN WIFI keeps BACK, SCAN, the keyboard, SAVE, RESET WIFI and FACTORY.
  ISS PAGE IS LOCKED. Do not change drawIss / drawEarth / fetchIssMap.
  ABOUT update: CHECK / INSTALL from GitHub.
  https://github.com/stefi01/mega-4-3-inch-waveshare-plane-tracker
*/

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <Preferences.h>
#include "esp_ota_ops.h"
#include "XPT2046_Touchscreen.h"
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "iss_world.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GFX_BL 2
#define PW 480
#define PH 272
#define PIN_DE 40
#define PIN_VSYNC 41
#define PIN_HSYNC 39
#define PIN_PCLK 42
#define TOUCH_SCK 12
#define TOUCH_MISO 13
#define TOUCH_MOSI 11
#define TOUCH_CS 38
#define TOUCH_IRQ 18
#ifndef SD_CS
#define SD_CS 10
#endif
#ifndef SD_MOSI
#define SD_MOSI 11
#endif
#ifndef SD_SCK
#define SD_SCK 12
#endif
#ifndef SD_MISO
#define SD_MISO 13
#endif

#define HFS_VER "1.0"
#ifndef HFS_GH_USER
#define HFS_GH_USER "stefi01"
#endif
#ifndef HFS_GH_REPO
#define HFS_GH_REPO "mega-4-3-inch-waveshare-plane-tracker"
#endif

enum {
  PG_SCOPE = 0,
  PG_WIFI,
  PG_LIST,
  PG_DETAIL,
  PG_WX,
  PG_FIELD,
  PG_SKY,
  PG_CLOCK,
  PG_LAUNCH,
  PG_NIGHT,
  PG_ISS,
  PG_SETTINGS,
  PG_ABOUT,
  PG_SAVER
};

Arduino_ESP32RGBPanel *rgbBus = nullptr;
Arduino_RGB_Display *gfx = nullptr;
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Preferences prefs;
int page = PG_SCOPE;

char wifiSsid[64] = "";
char wifiPass[64] = "";
char wifiScan[16][33];
int nWifiScan = 0;
int wifiScanTop = 0;
int wifiFocus = 0;
bool wifiShift = false;
bool wifiSym = false;
bool wifiScanBusy = false;
char wifiMsg[28] = "";
uint32_t wifiRedrawAt = 0;
uint32_t lastTouch = 0;

float homeLat = 36.7762f;
float homeLon = -119.7181f;
char homeIcao[8] = "KFAT";
int rangeNm = 50;
bool mapOn = true;
bool mapOk = false;
int mapStyle = 1;
bool dayMode = false;
bool cloudsOn = false;
bool stormOn = false;
bool stormHeld = false;
bool jpgSrcClean = false;
uint32_t lastStorm = 0;
bool unitsF = true;
bool summerTime = true;
bool showMarks = true;
bool shipsOn = false;
int saverMin = 0;
char saverFile[24][72];
int nSaver = 0;
int saverIdx = 0;
int saverReturn = PG_SCOPE;
uint32_t saverSlideAt = 0;
uint32_t lastIdle = 0;
bool sdOk = false;
fs::FS *saverFs = nullptr;
int saverSeen = 0;
char saverFirst[24] = "";
int blLevel = 2;
int setPane = 1;
int fieldPick = 0;
int hist[8];
int nHist = 0;
float pinchDist = 0;
int geoEdit = 0;
char geoBuf[18] = "";
int mapHaveStyle = -1;
int mapNm = 50;
uint32_t zoomNeed = 0;
bool wifiOk = false;
uint16_t *mapFb = nullptr;
uint16_t *saverFb = nullptr;
uint16_t *jpgSrc = nullptr;
uint16_t *issFb = nullptr;
uint16_t *issShade = nullptr;
bool issMapOk = false;
uint32_t lastIssMapTry = 0;
uint32_t lastFetch = 0;
enum { NET_NONE = 0, NET_PLANES, NET_MAP, NET_ISS_MAP, NET_FACTS, NET_WX, NET_LAUNCH, NET_OTA, NET_STORM, NET_SHIPS, NET_ZIP, NET_CODE, NET_TZ };
uint8_t otaKind = 0;
char otaLatest[12] = "";
char otaMsg[44] = "";
volatile int otaPct = -1;
volatile bool otaBusy = false;
bool otaNewer = false;
uint32_t lastOtaDraw = 0;
volatile int netJob = NET_NONE;
volatile int netDoneJob = NET_NONE;
volatile bool netBusy = false;
volatile bool netDone = false;
volatile int netPend = NET_NONE;
uint32_t lastFactsTry = 0;
uint8_t factsTries = 0;
uint32_t lastPlanesAt = 0;
char planesErr[20] = "";
uint32_t lastWx = 0;
int sunDay = -1;
int sunDst = -2;
char tzSaved[40] = "";
bool tzDirty = false;
uint32_t lastTz = 0;
uint32_t lastTzKeep = 0;
uint32_t lastSweep = 0;
uint32_t lastIssPos = 0;
uint32_t lastIssTry = 0;
uint32_t lastAstro = 0;
uint32_t lastLaunch = 0;
uint32_t lastLaunchTry = 0;
uint32_t lastPassAt = 0;
uint32_t lastMapTry = 0;
uint32_t lastTle = 0;
uint32_t lastClock = 0;
char clockFace[16] = "";
char clockStamp[96] = "";
float sweepDeg = 0;
float prevSweepDeg = 0;
float issLat = 0, issLon = 0, issAltKm = 420;
float sunLat = 0, sunLon = 0;
bool sunOk = false;
bool issOk = false;
bool issVisibleNow = false;
int issAz = 0, issEl = 0;
char issLook[4] = "";
uint32_t lastMap = 0;
bool launchOk = false;
time_t issNextPass = 0;
uint32_t issPathAt = 0;
bool wxOk = false;
char metarRaw[220] = "";
int wxDir = 0, wxKt = 0, wxT = 0, wxCode = 0, wxAqi = 0, wxRh = 0, wxFeel = 0;
int wxHi = 0, wxLo = 0, wxPop = 0;
float wxPress = 0;
char sunUp[8] = "--:--";
char sunDn[8] = "--:--";

struct HomeField {
  const char *icao, *city, *twr, *atis;
  float lat, lon;
};
static const HomeField homeFields[] = {
    {"KFAT", "Fresno", "118.20", "119.65", 36.7762f, -119.7181f},
    {"KSFO", "San Francisco", "120.50", "118.85", 37.6189f, -122.3750f},
    {"KOAK", "Oakland", "118.30", "128.50", 37.7213f, -122.2208f},
    {"KSJC", "San Jose", "124.00", "124.52", 37.3626f, -121.9290f},
    {"KLAX", "Los Angeles", "119.80", "133.80", 33.9425f, -118.4081f},
    {"KBUR", "Burbank", "118.70", "135.20", 34.2007f, -118.3585f},
    {"KSNA", "Santa Ana", "119.90", "126.00", 33.6757f, -117.8682f},
    {"KSAN", "San Diego", "118.30", "134.80", 32.7338f, -117.1933f},
    {"KLAS", "Las Vegas", "119.90", "132.40", 36.0840f, -115.1537f},
    {"KPSP", "Palm Springs", "118.70", "118.25", 33.8297f, -116.5067f},
    {"KPHX", "Phoenix", "118.70", "127.55", 33.4342f, -112.0116f},
    {"KSMF", "Sacramento", "125.70", "125.75", 38.6954f, -121.5908f},
    {"KRNO", "Reno", "118.70", "135.80", 39.4991f, -119.7681f},
    {"KSEA", "Seattle", "119.90", "127.70", 47.4502f, -122.3088f},
    {"KSLC", "Salt Lake", "118.30", "125.75", 40.7899f, -111.9791f},
    {"KDEN", "Denver", "128.75", "125.60", 39.8561f, -104.6737f},
    {"KDFW", "Dallas", "124.15", "134.90", 32.8998f, -97.0403f},
    {"KORD", "Chicago", "120.75", "135.40", 41.9742f, -87.9073f},
    {"KATL", "Atlanta", "119.50", "119.65", 33.6407f, -84.4277f},
    {"KJFK", "New York", "119.10", "128.72", 40.6413f, -73.7781f},
    {"KPDX", "Portland", "118.70", "128.35", 45.5898f, -122.5951f},
    {"KGEG", "Spokane", "118.30", "124.32", 47.6199f, -117.5338f},
    {"KBOI", "Boise", "118.10", "123.90", 43.5644f, -116.2228f},
    {"KONT", "Ontario", "120.60", "121.00", 34.0560f, -117.6012f},
    {"KLGB", "Long Beach", "119.40", "127.75", 33.8177f, -118.1516f},
    {"KTUS", "Tucson", "118.30", "123.80", 32.1161f, -110.9410f},
    {"KABQ", "Albuquerque", "118.30", "118.00", 35.0402f, -106.6092f},
    {"KELP", "El Paso", "118.30", "120.10", 31.8072f, -106.3776f},
    {"KCOS", "Colo Springs", "119.90", "125.00", 38.8058f, -104.7008f},
    {"KBIL", "Billings", "118.30", "119.05", 45.8077f, -108.5429f},
    {"KMSO", "Missoula", "118.40", "126.65", 46.9163f, -114.0906f},
    {"KDAL", "Dallas Love", "118.70", "120.15", 32.8471f, -96.8518f},
    {"KIAH", "Houston", "118.10", "124.05", 29.9844f, -95.3414f},
    {"KHOU", "Houston Hobby", "118.70", "135.10", 29.6454f, -95.2789f},
    {"KAUS", "Austin", "121.00", "124.40", 30.1945f, -97.6699f},
    {"KSAT", "San Antonio", "119.80", "118.40", 29.5337f, -98.4698f},
    {"KMSY", "New Orleans", "119.50", "127.55", 29.9934f, -90.2580f},
    {"KOKC", "Oklahoma City", "119.35", "125.85", 35.3931f, -97.6007f},
    {"KTUL", "Tulsa", "118.70", "125.70", 36.1984f, -95.8881f},
    {"KLIT", "Little Rock", "118.70", "125.85", 34.7294f, -92.2243f},
    {"KMEM", "Memphis", "118.30", "127.75", 35.0424f, -89.9767f},
    {"KBNA", "Nashville", "118.60", "135.10", 36.1245f, -86.6782f},
    {"KBHM", "Birmingham", "119.90", "119.40", 33.5629f, -86.7535f},
    {"KHSV", "Huntsville", "127.60", "121.25", 34.6372f, -86.7751f},
    {"KTYS", "Knoxville", "121.20", "128.35", 35.8110f, -83.9940f},
    {"KMCO", "Orlando", "118.45", "121.25", 28.4294f, -81.3090f},
    {"KMIA", "Miami", "118.30", "133.67", 25.7959f, -80.2870f},
    {"KFLL", "Ft Lauderdale", "120.20", "135.00", 26.0726f, -80.1527f},
    {"KTPA", "Tampa", "119.50", "126.45", 27.9755f, -82.5332f},
    {"KPBI", "West Palm", "119.10", "123.75", 26.6832f, -80.0956f},
    {"KRSW", "Fort Myers", "128.75", "124.65", 26.5362f, -81.7552f},
    {"KJAX", "Jacksonville", "118.30", "125.85", 30.4941f, -81.6878f},
    {"KCLT", "Charlotte", "118.10", "132.10", 35.2140f, -80.9431f},
    {"KRDU", "Raleigh", "119.30", "123.80", 35.8776f, -78.7875f},
    {"KCHS", "Charleston", "126.00", "124.75", 32.8986f, -80.0405f},
    {"KSAV", "Savannah", "119.10", "126.37", 32.1276f, -81.2021f},
    {"KMDW", "Chicago Midway", "118.70", "132.87", 41.7868f, -87.7522f},
    {"KMSP", "Minneapolis", "126.70", "120.82", 44.8820f, -93.2218f},
    {"KMKE", "Milwaukee", "119.10", "126.40", 42.9472f, -87.8966f},
    {"KSTL", "St Louis", "118.50", "125.02", 38.7487f, -90.3700f},
    {"KMCI", "Kansas City", "128.20", "128.37", 39.2976f, -94.7139f},
    {"KDSM", "Des Moines", "118.30", "119.55", 41.5340f, -93.6631f},
    {"KOMA", "Omaha", "118.30", "120.40", 41.3032f, -95.8940f},
    {"KICT", "Wichita", "118.20", "125.15", 37.6499f, -97.4331f},
    {"KIND", "Indianapolis", "120.90", "134.25", 39.7173f, -86.2944f},
    {"KCVG", "Cincinnati", "118.30", "126.37", 39.0488f, -84.6678f},
    {"KCLE", "Cleveland", "124.50", "127.20", 41.4117f, -81.8498f},
    {"KCMH", "Columbus", "132.70", "124.60", 39.9980f, -82.8919f},
    {"KDTW", "Detroit", "118.40", "118.37", 42.2124f, -83.3534f},
    {"KSDF", "Louisville", "124.20", "118.72", 38.1744f, -85.7360f},
    {"KGRR", "Grand Rapids", "124.60", "118.55", 42.8808f, -85.5228f},
    {"KLGA", "LaGuardia", "118.70", "127.05", 40.7772f, -73.8726f},
    {"KEWR", "Newark", "118.30", "134.02", 40.6895f, -74.1745f},
    {"KPHL", "Philadelphia", "118.50", "133.40", 39.8721f, -75.2411f},
    {"KBOS", "Boston", "128.80", "135.00", 42.3656f, -71.0096f},
    {"KBDL", "Hartford", "120.30", "118.15", 41.9389f, -72.6832f},
    {"KPVD", "Providence", "120.70", "124.20", 41.7240f, -71.4282f},
    {"KPWM", "Portland ME", "120.90", "119.05", 43.6462f, -70.3087f},
    {"KALB", "Albany", "119.50", "120.85", 42.7483f, -73.8017f},
    {"KBUF", "Buffalo", "120.45", "134.47", 42.9405f, -78.7302f},
    {"KROC", "Rochester", "118.50", "124.85", 43.1189f, -77.6724f},
    {"KPIT", "Pittsburgh", "119.10", "127.25", 40.4915f, -80.2329f},
    {"KBWI", "Baltimore", "119.40", "127.80", 39.1754f, -76.6683f},
    {"KIAD", "Dulles", "120.10", "134.85", 38.9531f, -77.4565f},
    {"KDCA", "Reagan", "119.10", "132.65", 38.8512f, -77.0402f},
    {"KRIC", "Richmond", "121.10", "119.15", 37.5052f, -77.3197f},
    {"KORF", "Norfolk", "120.95", "128.65", 36.8946f, -76.2012f},
    {"KJAN", "Jackson", "119.05", "119.25", 32.3112f, -90.0759f},
    {"KMOB", "Mobile", "118.30", "119.82", 30.6914f, -88.2428f},
    {"KFAR", "Fargo", "120.40", "119.40", 46.9207f, -96.8158f},
    {"KFSD", "Sioux Falls", "118.30", "126.60", 43.5851f, -96.7411f},
    {"PHNL", "Honolulu", "118.10", "127.90", 21.3187f, -157.9224f},
    {"PHOG", "Kahului", "120.20", "128.60", 20.8986f, -156.4305f},
    {"PANC", "Anchorage", "118.30", "135.50", 61.1744f, -149.9963f},
    {nullptr, nullptr, nullptr, nullptr, 0, 0}};

struct Plane {
  char hex[8];
  char call[10];
  char type[8];
  char reg[10];
  char typeName[22];
  char mfr[16];
  char orig[6];
  char dest[6];
  char origCity[18];
  char destCity[18];
  char airline[18];
  char owner[20];
  char engines[18];
  char dep[10];
  char arr[10];
  int seats, cruise, fl;
  float lat, lon;
  int alt, gs, hdg, vs, squawk;
  int maxAlt, maxGs, durMin;
  bool routeOk, factsOk, timesOk;
};
#define HFS_MAXP 100
#define HFS_MAXSHIP 36
Plane planes[HFS_MAXP];
Plane keepStore[HFS_MAXP];
Plane inboundPlanes[HFS_MAXP];
int nPlanes = 0;
int selected = -1;
char selHex[8] = "";
Plane factHold;
bool factHoldOk = false;
int listTop = 0;
int fieldTop = 0;
const int LIST_ROWS = 8;
const int FIELD_ROWS = 6;
struct PlaneMark {
  int idx, x, y, bx, by, bw, bh;
};
PlaneMark marks[HFS_MAXP];
struct Ship {
  float lat, lon;
  int sog;
  char name[16];
  int x, y, w, h;
};
Ship ships[HFS_MAXSHIP];
int nShips = 0;
uint32_t lastShips = 0;
char zipCode[8] = "";
char zipMsg[40] = "";
char codeLook[6] = "";
struct ZipHit {
  char icao[8];
  char city[16];
  float lat, lon;
};
ZipHit zipHits[4];
int nZipHits = 0;
int nMarks = 0;

struct SkyObj {
  char name[12];
  char look[4];
  char note[16];
  char hours[22];
  int az, el;
};
SkyObj skyVis[8];
int nSkyVis = 0;

struct SkyCand {
  char name[12];
  char note[16];
  char hours[22];
  int az, el, score;
};

struct Launch {
  char day[10];
  char when[8];
  char rocket[18];
  char payload[24];
  char pad[20];
  char agency[16];
};
Launch launches[6];
int nLaunch = 0;
Launch usaNext[4];
int nUsaNext = 0;

struct IssPass {
  char line[48];
  char from[4];
  time_t when;
  int durSec;
};
IssPass issPasses[5];
int nIssPass = 0;

struct IssTle {
  bool ok;
  double epochJd, n, e, inc, raan, argp, m0;
};
IssTle issTle;
float issTrackLat[72];
float issTrackLon[72];
int nIssTrack = 0;

uint16_t COL_BG = RGB565(232, 226, 214);
uint16_t COL_DIM = RGB565(210, 202, 186);
uint16_t COL_MID = RGB565(90, 94, 102);
uint16_t COL_TXT = RGB565(28, 30, 34);
uint16_t COL_YEL = RGB565(176, 120, 8);
uint16_t COL_WHT = RGB565(255, 255, 255);
uint16_t COL_GRY = RGB565(100, 104, 110);
uint16_t COL_HOT = RGB565(176, 120, 8);
uint16_t COL_MAG = RGB565(180, 40, 110);
uint16_t COL_AMB = RGB565(196, 100, 16);
uint16_t COL_CYN = RGB565(0, 90, 180);
const uint16_t COL_BLK = RGB565(0, 0, 0);
uint16_t COL_PATH = RGB565(0, 40, 130);

uint16_t COL_FIELD = RGB565(236, 230, 218);
uint16_t COL_ROW0 = RGB565(224, 218, 206);
uint16_t COL_ROW1 = RGB565(214, 208, 196);
uint16_t COL_IN = RGB565(186, 210, 186);
uint16_t COL_OUT = RGB565(220, 198, 176);

bool fetchIssMap();
bool fetchMap();
void paintStormOnMap();
void fetchStorm();
bool fetchPlanes();
void waitNetIdle();
void askNet(int job);
void tickSweepIfDue();
void strokeSweep(float deg);
void drawScope();
void drawList();
void drawField();
void drawIss();
void drawSettings();
void drawAbout();
void drawOtaStatus();
void tapAbout(int x, int y);
void runOtaJob();
void drawClock();
void drawWeather();
void drawSky();
void drawLaunch();
void drawNight();
void drawWifi();
void drawWifiField(int focus, const char *raw, const char *empty, int y);
void pushHist();
void openIss();
void openLaunch();
void openWifi();
void connectWifi();
void refreshScopeTraffic();
void takeNetResult();
void paintPage(int pg);
void bakeMapLook();
void initSd();
void showSaverSlide();
void enterSaver();
void leaveSaver();
void tickSaver();
void goHome();
void goBack();
void netTask(void *pv);
void fetchAircraft(Plane &p);
void finishTowns(Plane &p);
bool fieldLL(const char *icao, float &lat, float &lon);
bool acceptRoute(Plane &p, float oLat, float oLon, float dLat, float dLon);
bool liveClock(time_t *out);
bool icaoMatch(const char *a, const char *b);
void jsonKey(const char *s, const char *lim, const char *key, char *out, int n);
const char *placeName(const char *icao, const char *city);

void cols() {
  if (dayMode) {
    COL_BG = RGB565(232, 226, 214);
    COL_DIM = RGB565(210, 202, 186);
    COL_MID = RGB565(90, 94, 102);
    COL_HOT = RGB565(196, 132, 8);
    COL_TXT = RGB565(28, 30, 34);
    COL_WHT = RGB565(255, 255, 255);
    COL_AMB = RGB565(196, 100, 16);
    COL_CYN = RGB565(0, 90, 180);
    COL_YEL = RGB565(176, 120, 8);
    COL_MAG = RGB565(180, 40, 110);
    COL_GRY = RGB565(100, 104, 110);
    COL_PATH = RGB565(0, 40, 130);
    COL_ROW0 = RGB565(224, 218, 206);
    COL_ROW1 = RGB565(214, 208, 196);
    COL_FIELD = RGB565(236, 230, 218);
    COL_IN = RGB565(186, 210, 186);
    COL_OUT = RGB565(220, 198, 176);
  } else {
    COL_BG = RGB565(18, 22, 28);
    COL_DIM = RGB565(32, 40, 52);
    COL_MID = RGB565(200, 210, 220);
    COL_HOT = RGB565(255, 210, 40);
    COL_TXT = RGB565(250, 250, 250);
    COL_WHT = RGB565(255, 255, 255);
    COL_AMB = RGB565(255, 170, 40);
    COL_CYN = RGB565(40, 220, 255);
    COL_YEL = RGB565(255, 230, 60);
    COL_MAG = RGB565(255, 70, 180);
    COL_GRY = RGB565(210, 215, 220);
    COL_PATH = RGB565(0, 56, 168);
    COL_ROW0 = RGB565(16, 24, 40);
    COL_ROW1 = RGB565(12, 18, 30);
    COL_FIELD = RGB565(10, 16, 24);
    COL_IN = RGB565(18, 40, 28);
    COL_OUT = RGB565(40, 28, 16);
  }
}

const int CX = 240, CY = 100, RR = 91;
const int MAP_H = 200;
const int SRC_W = 320, SRC_H = 176;
const int ISS_Y0 = 36;
const int ISS_H = 113;
const int ISS_SRC_W = 320;
const int ISS_SRC_H = 119;
int mapSrcW = SRC_W, mapSrcH = SRC_H;
const int ZOOM_Y = 200;
const int MENU_Y = 227;
const int KW = 48;

void centre(const char *s, int cx, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(cx - (int)w / 2, y);
  gfx->print(s);
}

bool hit(int x, int y, int x0, int y0, int x1, int y1) {
  return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

void chip(int x, int y, int w, int h, const char *t, bool on) {
  gfx->fillRoundRect(x, y, w, h, 3, on ? COL_HOT : COL_DIM);
  gfx->setTextColor(on ? COL_BG : COL_TXT, on ? COL_HOT : COL_DIM);
  gfx->setTextSize(1);
  centre(t, x + w / 2, y + (h / 2) - 4);
}

void compassPill(int x, int y, const char *t) {
  gfx->fillRect(x - 8, y - 2, 17, 12, COL_BLK);
  gfx->setTextColor(COL_YEL, COL_BLK);
  gfx->setTextSize(1);
  centre(t, x, y);
}

void drawCompass() {
  compassPill(CX, 9, "N");
  compassPill(CX, MAP_H - 16, "S");
  compassPill(14, CY - 5, "W");
  compassPill(PW - 14, CY - 5, "E");
}

void statusLine(const char *s) {
  gfx->fillRect(48, 85, PW - 96, 20, COL_BG);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  centre(s, CX, 95);
  Serial.println(s);
}

void applyHomeTz() {
  if (tzSaved[0]) {
    setenv("TZ", tzSaved, 1);
    tzset();
    return;
  }
  const char *tz = "EST5EDT,M3.2.0,M11.1.0";
  float la = homeLat, lo = homeLon;
  if (la >= 18.5f && la <= 22.5f && lo <= -154.5f && lo >= -160.5f)
    tz = "HST10";
  else if (la >= 51.0f && lo < -169.5f)
    tz = "HST10";
  else if (la >= 51.0f && lo <= -130.0f)
    tz = "AKST9AKDT,M3.2.0,M11.1.0";
  else if (la >= 17.5f && la <= 18.6f && lo >= -67.6f && lo <= -64.5f)
    tz = "AST4";
  else if (la >= 31.3f && la <= 37.0f && lo >= -114.85f && lo <= -109.04f)
    tz = "MST7";
  else if (la >= 42.0f && la <= 45.6f && lo >= -117.3f && lo < -114.0f)
    tz = "MST7MDT,M3.2.0,M11.1.0";
  else if (la >= 29.6f && la <= 31.1f && lo >= -87.7f && lo <= -85.0f)
    tz = "CST6CDT,M3.2.0,M11.1.0";
  else if (lo <= -115.0f && lo > -130.0f && la < 51.0f)
    tz = "PST8PDT,M3.2.0,M11.1.0";
  else if (lo <= -102.5f && la >= 25.0f)
    tz = "MST7MDT,M3.2.0,M11.1.0";
  else if (lo <= -86.6f && la >= 24.0f)
    tz = "CST6CDT,M3.2.0,M11.1.0";
  else if (lo > -130.0f && lo <= -67.0f && la >= 24.0f && la < 72.0f)
    tz = "EST5EDT,M3.2.0,M11.1.0";
  else {
    static char off[12];
    int h = (int)lroundf(-lo / 15.0f);
    if (h > 12) h = 12;
    if (h < -12) h = -12;
    snprintf(off, sizeof(off), "LCL%d", h);
    tz = off;
  }
  setenv("TZ", tz, 1);
  tzset();
}

void syncFieldPick();
void applyBl();
void savePrefs();
void buildIssOrbitPath();
void alignIssPathToLive();
void drawDetail();
void openGeo(int which);

void loadPrefs() {
  prefs.begin("mega43", true);
  homeLat = prefs.getFloat("lat", 36.7762f);
  homeLon = prefs.getFloat("lon", -119.7181f);
  rangeNm = prefs.getInt("nm", 50);
  shipsOn = prefs.getBool("ships", false);
  strlcpy(zipCode, prefs.getString("zip", "").c_str(), sizeof(zipCode));
  strlcpy(homeIcao, prefs.getString("icao", "KFAT").c_str(), sizeof(homeIcao));
  strlcpy(wifiSsid, prefs.getString("ssid", "").c_str(), sizeof(wifiSsid));
  strlcpy(wifiPass, prefs.getString("pass", "").c_str(), sizeof(wifiPass));
  mapOn = prefs.getBool("map", true);
  mapStyle = prefs.getInt("mapsv", 1);
  cloudsOn = prefs.getBool("cloud", false);
  stormOn = prefs.getBool("storm", false);
  unitsF = prefs.getBool("units", true);
  summerTime = prefs.getBool("sumr", true);
  strlcpy(tzSaved, prefs.getString("tz", "").c_str(), sizeof(tzSaved));
  showMarks = prefs.getBool("marks", true);
  dayMode = prefs.getBool("dayv", false);
  saverMin = prefs.getInt("saver", 0);
  if (saverMin != 0 && saverMin != 2 && saverMin != 5 && saverMin != 15) saverMin = 0;
  blLevel = prefs.getInt("bl", 2);
  if (blLevel < 0 || blLevel > 2) blLevel = 2;
  prefs.end();
  if (rangeNm < 10) rangeNm = 10;
  if (rangeNm > 250) rangeNm = 250;
  rangeNm = ((rangeNm + 5) / 10) * 10;
  syncFieldPick();
  cols();
  applyHomeTz();
  if (gfx) applyBl();
}

void writeScopeSettings() {
  prefs.putFloat("lat", homeLat);
  prefs.putFloat("lon", homeLon);
  prefs.putInt("nm", rangeNm);
  prefs.putString("icao", homeIcao);
  prefs.putBool("map", mapOn);
  prefs.putInt("mapsv", mapStyle);
  prefs.putBool("cloud", cloudsOn);
  prefs.putBool("storm", stormOn);
  prefs.putBool("units", unitsF);
  prefs.putBool("sumr", summerTime);
  prefs.putString("tz", tzSaved);
  prefs.putBool("marks", showMarks);
  prefs.putBool("ships", shipsOn);
  prefs.putString("zip", zipCode);
  prefs.putBool("dayv", dayMode);
  prefs.putInt("saver", saverMin);
  prefs.putInt("bl", blLevel);
}

void saveWifi() {
  prefs.begin("mega43", false);
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPass);
  writeScopeSettings();
  prefs.end();
}

void saveRange() {
  savePrefs();
}

void savePrefs() {
  prefs.begin("mega43", false);
  writeScopeSettings();
  prefs.end();
}

void clearWifi() {
  wifiSsid[0] = 0;
  wifiPass[0] = 0;
  prefs.begin("mega43", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
}

void factoryReset() {
  prefs.begin("mega43", false);
  prefs.clear();
  prefs.end();
}

float distNm(const Plane &p) {
  float dLat = (p.lat - homeLat) * 60.0f;
  float dLon = (p.lon - homeLon) * 60.0f * cosf(homeLat * 0.0174533f);
  return hypotf(dLat, dLon);
}

float nmBetween(float lat1, float lon1, float lat2, float lon2) {
  float dLat = (lat2 - lat1) * 60.0f;
  float dLon = (lon2 - lon1) * 60.0f * cosf(((lat1 + lat2) * 0.5f) * 0.0174533f);
  return hypotf(dLat, dLon);
}

float brgDeg(float lat1, float lon1, float lat2, float lon2) {
  float r1 = lat1 * 0.0174533f;
  float r2 = lat2 * 0.0174533f;
  float dL = (lon2 - lon1) * 0.0174533f;
  float y = sinf(dL) * cosf(r2);
  float x = cosf(r1) * sinf(r2) - sinf(r1) * cosf(r2) * cosf(dL);
  float d = atan2f(y, x) * 57.29578f;
  if (d < 0) d += 360.0f;
  return d;
}

bool headingToward(const Plane &p, float lat, float lon) {
  float brg = brgDeg(p.lat, p.lon, lat, lon);
  float d = fabsf((float)p.hdg - brg);
  if (d > 180.0f) d = 360.0f - d;
  return d <= 40.0f;
}

bool routeFitsPlane(const Plane &p, float oLat, float oLon, float dLat, float dLon) {
  if ((oLat == 0 && oLon == 0) || (dLat == 0 && dLon == 0)) return false;
  if (p.lat == 0 && p.lon == 0) return false;
  float dOD = nmBetween(oLat, oLon, dLat, dLon);
  if (dOD < 30.0f) return false;
  float scale = cosf(((oLat + dLat) * 0.5f) * 0.0174533f);
  float vx = (dLon - oLon) * 60.0f * scale;
  float vy = (dLat - oLat) * 60.0f;
  float px = (p.lon - oLon) * 60.0f * scale;
  float py = (p.lat - oLat) * 60.0f;
  float vlen = sqrtf(vx * vx + vy * vy);
  if (vlen < 1.0f) return false;
  float along = (px * vx + py * vy) / vlen;
  float cross = (px * vy - py * vx) / vlen;
  if (along < -40.0f || along > dOD + 40.0f) return false;
  return fabsf(cross) <= 80.0f;
}

bool routeMissesPlane(const Plane &p, float oLat, float oLon, float dLat, float dLon) {
  if ((oLat == 0 && oLon == 0) || (dLat == 0 && dLon == 0)) return false;
  if (p.lat == 0 && p.lon == 0) return false;
  float dOD = nmBetween(oLat, oLon, dLat, dLon);
  if (dOD < 30.0f) return false;
  float scale = cosf(((oLat + dLat) * 0.5f) * 0.0174533f);
  float vx = (dLon - oLon) * 60.0f * scale;
  float vy = (dLat - oLat) * 60.0f;
  float px = (p.lon - oLon) * 60.0f * scale;
  float py = (p.lat - oLat) * 60.0f;
  float vlen = sqrtf(vx * vx + vy * vy);
  if (vlen < 1.0f) return false;
  float along = (px * vx + py * vy) / vlen;
  float cross = (px * vy - py * vx) / vlen;
  if (along < -80.0f || along > dOD + 80.0f) return true;
  return fabsf(cross) > 120.0f;
}

void clearRoute(Plane &p) {
  p.orig[0] = 0;
  p.dest[0] = 0;
  p.origCity[0] = 0;
  p.destCity[0] = 0;
  p.airline[0] = 0;
  p.routeOk = false;
}

int findHomeField(const char *icao) {
  for (int i = 0; homeFields[i].icao; i++)
    if (!strcasecmp(homeFields[i].icao, icao)) return i;
  return -1;
}

void syncFieldPick() {
  int i = findHomeField(homeIcao);
  if (i >= 0) fieldPick = i;
}

void dropZone() {
  tzSaved[0] = 0;
  sunDay = -1;
  sunDst = -2;
  lastTz = 0;
  strlcpy(sunUp, "--:--", sizeof(sunUp));
  strlcpy(sunDn, "--:--", sizeof(sunDn));
}

void useHomeField(int i) {
  if (i < 0) return;
  int n = 0;
  while (homeFields[n].icao) n++;
  if (i >= n) i = 0;
  fieldPick = i;
  strlcpy(homeIcao, homeFields[i].icao, sizeof(homeIcao));
  homeLat = homeFields[i].lat;
  homeLon = homeFields[i].lon;
  lastPassAt = 0;
  dropZone();
  applyHomeTz();
}

void bumpIcaoLetter(int i) {
  if (i < 0 || i > 3) return;
  char c = homeIcao[i];
  if (c < 'A' || c > 'Z') c = 'A';
  else if (++c > 'Z') c = 'A';
  homeIcao[i] = c;
  homeIcao[4] = 0;
  int hit = findHomeField(homeIcao);
  if (hit >= 0) useHomeField(hit);
}

void refreshGeo() {
  mapOk = false;
  mapNm = 0;
  lastMap = 0;
  lastMapTry = 0;
  lastFetch = 0;
  lastWx = 0;
  lastAstro = 0;
  lastPassAt = 0;
  dropZone();
  applyHomeTz();
  nPlanes = 0;
  if (mapOn) {
    askNet(NET_MAP);
    if (!netPend) netPend = NET_PLANES;
  } else
    askNet(NET_PLANES);
}

int bearingTo(const Plane &p) {
  float dLat = (p.lat - homeLat) * 60.0f;
  float dLon = (p.lon - homeLon) * 60.0f * cosf(homeLat * 0.0174533f);
  int brg = (int)(atan2f(dLon, dLat) * 57.2958f);
  if (brg < 0) brg += 360;
  return brg;
}

void sortField(int *ord, int n) {
  for (int i = 0; i < n; i++) ord[i] = i;
  for (int i = 0; i < n; i++)
    for (int j = i + 1; j < n; j++)
      if (distNm(planes[ord[j]]) < distNm(planes[ord[i]])) {
        int t = ord[i];
        ord[i] = ord[j];
        ord[j] = t;
      }
}

bool wifiLinked() { return WiFi.status() == WL_CONNECTED; }

int ktMph(int kt) { return (int)(kt * 1.15078f + 0.5f); }

void drawWifiChip() {
  bool on = wifiLinked();
  wifiOk = on;
  int cx = 458, cy = 19;
  uint16_t c = on ? COL_YEL : COL_GRY;
  gfx->fillCircle(cx, cy, 2, c);
  for (int r = 6; r <= 12; r += 3) {
    for (int a = -55; a <= 55; a++) {
      float rad = a * 0.0174533f;
      int x = cx + (int)(sinf(rad) * r);
      int y = cy - (int)(cosf(rad) * r);
      gfx->drawPixel(x, y, c);
      gfx->drawPixel(x, y + 1, c);
    }
  }
  if (!on) gfx->drawLine(cx - 11, cy - 12, cx + 11, cy + 3, COL_TXT);
}

float nmPerPx() { return (float)RR / (float)rangeNm; }

void llToXY(float lat, float lon, float &x, float &y) {
  float dLat = (lat - homeLat) * 60.0f;
  float dLon = (lon - homeLon) * 60.0f * cosf(homeLat * 0.0174533f);
  x = CX + dLon * nmPerPx();
  y = CY - dLat * nmPerPx();
}

uint16_t planeColor(int alt) {
  if (alt >= 20000) return COL_CYN;
  if (alt >= 5000) return COL_WHT;
  return COL_AMB;
}

static const char *HFS_UA =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36";

void *psAlloc(size_t n) {
  size_t bytes = (n + 63u) & ~63u;
  bytes += 1024;
  void *p = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = ps_malloc(bytes);
  return p;
}

void hfsHttp(HTTPClient &http, uint16_t timeout) {
  http.setTimeout(timeout);
  http.setConnectTimeout(4000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setReuse(false);
  http.setUserAgent(HFS_UA);
}

SemaphoreHandle_t httpMu = nullptr;

void httpLockInit() {
  if (!httpMu) httpMu = xSemaphoreCreateMutex();
}

bool httpLock() {
  if (!httpMu) return true;
  return xSemaphoreTake(httpMu, pdMS_TO_TICKS(25000)) == pdTRUE;
}

void httpUnlock() {
  if (httpMu) xSemaphoreGive(httpMu);
}

void requestZoomRefresh() { zoomNeed = millis() ? millis() : 1; }

bool httpGet(const char *url, String &out, uint16_t timeout = 7000) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!httpLock()) return false;
  WiFiClientSecure c;
  c.setInsecure();
  HTTPClient http;
  hfsHttp(http, timeout);
  bool ok = false;
  if (http.begin(c, url)) {
    http.addHeader("Accept", "application/json,*/*");
    int code = http.GET();
    if (code == 200) {
      out = http.getString();
      ok = true;
    }
    http.end();
  }
  c.stop();
  httpUnlock();
  return ok;
}

bool httpGetPlain(const char *url, char *out, int outn, uint16_t timeout) {
  if (!out || outn < 8) return false;
  out[0] = 0;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (strncmp(url, "http://", 7) != 0) return false;
  const char *slash = strchr(url + 7, '/');
  if (!slash) return false;
  char host[96];
  int hlen = (int)(slash - (url + 7));
  if (hlen < 1 || hlen >= (int)sizeof(host)) return false;
  memcpy(host, url + 7, hlen);
  host[hlen] = 0;
  if (!httpLock()) return false;
  WiFiClient c;
  c.setTimeout(200);
  bool ok = false;
  if (c.connect(host, 80, (int32_t)timeout)) {
    c.printf("GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nAccept: */*\r\nConnection: close\r\n\r\n",
             slash, host, HFS_UA);
    int got = 0;
    uint32_t t1 = millis();
    while (got < outn - 1 && millis() - t1 < timeout) {
      int n = c.available();
      if (n > 0) {
        if (n > 256) n = 256;
        if (got + n > outn - 1) n = outn - 1 - got;
        int r = c.read((uint8_t *)out + got, n);
        if (r <= 0) break;
        got += r;
        continue;
      }
      if (!c.connected()) break;
      delay(2);
    }
    out[got] = 0;
    char *body = strstr(out, "\r\n\r\n");
    if (body && strncmp(out, "HTTP/1.", 7) == 0 && strstr(out, " 200 ")) {
      body += 4;
      int n = (int)strlen(body);
      memmove(out, body, n + 1);
      ok = n > 20;
    } else
      out[0] = 0;
  }
  c.stop();
  httpUnlock();
  return ok;
}

bool httpGetBin(const char *url, uint8_t **out, int *outLen) {
  *out = nullptr;
  *outLen = 0;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!httpLock()) return false;
  WiFiClientSecure c;
  c.setInsecure();
  HTTPClient http;
  hfsHttp(http, 10000);
  http.setConnectTimeout(4000);
  bool ok = false;
  if (http.begin(c, url)) {
    http.addHeader("Accept", "image/jpeg,*/*");
    int code = http.GET();
    if (code == 200) {
      int hint = http.getSize();
      const int CAP = 280000;
      int cap = (hint > 400 && hint < CAP) ? hint : CAP;
      uint8_t *buf = (uint8_t *)psAlloc(cap);
      if (buf) {
        int got = 0;
        uint32_t t0 = millis();
        while (got < cap && millis() - t0 < 10000UL) {
          int n = http.getStream().available();
          if (n > 0) {
            if (n > 1024) n = 1024;
            if (got + n > cap) n = cap - got;
            int r = http.getStream().readBytes(buf + got, n);
            if (r <= 0) break;
            got += r;
            delay(3);
            continue;
          }
          if (!http.connected()) break;
          delay(2);
        }
        bool whole = got >= 400 && buf[0] == 0xFF && buf[1] == 0xD8 && (hint <= 0 || got + 8 >= hint);
        if (whole) {
          int from = got - 24;
          if (from < 2) from = 2;
          whole = false;
          for (int i = got - 2; i >= from; i--) {
            if (buf[i] == 0xFF && buf[i + 1] == 0xD9) {
              whole = true;
              break;
            }
          }
        }
        if (whole) {
          *out = buf;
          *outLen = got;
          ok = true;
        } else
          free(buf);
      }
    }
    http.end();
  }
  c.stop();
  httpUnlock();
  return ok;
}

bool httpGetFile(const char *url, uint8_t **out, int *outLen, int cap, uint16_t timeout) {
  *out = nullptr;
  *outLen = 0;
  if (!url || strncmp(url, "http://", 7) != 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  const char *slash = strchr(url + 7, '/');
  if (!slash) return false;
  char host[96];
  int hlen = (int)(slash - (url + 7));
  if (hlen < 1 || hlen >= (int)sizeof(host)) return false;
  memcpy(host, url + 7, hlen);
  host[hlen] = 0;
  if (!httpLock()) return false;
  WiFiClient c;
  c.setTimeout(200);
  bool ok = false;
  uint8_t *buf = nullptr;
  if (c.connect(host, 80, (int32_t)timeout)) {
    c.printf("GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nAccept: image/png,*/*\r\nConnection: close\r\n\r\n",
             slash, host, HFS_UA);
    buf = (uint8_t *)psAlloc(cap);
    if (!buf) buf = (uint8_t *)malloc(cap);
    int got = 0;
    uint32_t t1 = millis();
    if (buf) {
      while (got < cap - 1 && millis() - t1 < timeout) {
        int n = c.available();
        if (n > 0) {
          if (n > 1024) n = 1024;
          if (got + n > cap - 1) n = cap - 1 - got;
          int r = c.read(buf + got, n);
          if (r <= 0) break;
          got += r;
          delay(3);
          continue;
        }
        if (!c.connected()) break;
        delay(2);
      }
    }
    if (buf && got > 40) {
      buf[got] = 0;
      char *body = strstr((char *)buf, "\r\n\r\n");
      if (body && strncmp((char *)buf, "HTTP/1.", 7) == 0 && strstr((char *)buf, " 200 ")) {
        body += 4;
        int n = got - (int)(body - (char *)buf);
        if (n > 40) {
          memmove(buf, body, n);
          *out = buf;
          *outLen = n;
          ok = true;
          buf = nullptr;
        }
      }
    }
  }
  if (buf) free(buf);
  c.stop();
  httpUnlock();
  return ok;
}

bool httpGetSmall(const char *url, char *out, int outn, uint16_t timeout) {
  if (!out || outn < 8) return false;
  out[0] = 0;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!httpLock()) return false;
  WiFiClientSecure c;
  c.setInsecure();
  HTTPClient http;
  hfsHttp(http, timeout);
  bool ok = false;
  if (http.begin(c, url)) {
    http.addHeader("Accept", "application/json,*/*");
    http.addHeader("User-Agent", "Mozilla/5.0");
    int code = http.GET();
    if (code == 200) {
      int got = 0;
      uint32_t t0 = millis();
      while (got < outn - 1 && millis() - t0 < timeout) {
        int n = http.getStream().available();
        if (n > 0) {
          if (n > 1024) n = 1024;
          if (got + n > outn - 1) n = outn - 1 - got;
          int r = http.getStream().readBytes(out + got, n);
          if (r <= 0) break;
          got += r;
          delay(3);
          continue;
        }
        if (!http.connected()) break;
        delay(2);
      }
      out[got] = 0;
      ok = got > 4;
    }
    http.end();
  }
  c.stop();
  httpUnlock();
  return ok;
}

bool mapPlotPace = false;

bool jpgPlot(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!jpgSrc) return 0;
  for (uint16_t j = 0; j < h; j++) {
    int yy = y + j;
    if (yy < 0 || yy >= SRC_H) continue;
    for (uint16_t i = 0; i < w; i++) {
      int xx = x + i;
      if (xx < 0 || xx >= SRC_W) continue;
      jpgSrc[yy * SRC_W + xx] = bitmap[j * w + i];
    }
  }
  if (mapPlotPace) {
    static uint8_t pace;
    if ((++pace & 15) == 0) delay(1);
  }
  return 1;
}

bool ensureSaverFb() {
  if (saverFb) return true;
  saverFb = (uint16_t *)ps_malloc((size_t)PW * PH * 2);
  return saverFb != nullptr;
}

bool saverJpgPlot(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!saverFb || !bitmap || !w || !h) return 1;
  for (uint16_t j = 0; j < h; j++) {
    int yy = y + j;
    if ((unsigned)yy >= (unsigned)PH) continue;
    uint16_t *row = saverFb + yy * PW;
    for (uint16_t i = 0; i < w; i++) {
      int xx = x + i;
      if ((unsigned)xx >= (unsigned)PW) continue;
      row[xx] = bitmap[j * w + i];
    }
  }
  return 1;
}

bool isJpgName(const char *n) {
  const char *dot = strrchr(n, '.');
  if (!dot) return false;
  return !strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg");
}

const char *saverBase(const char *path) {
  const char *slash = strrchr(path, '/');
  return (slash && slash[1]) ? slash + 1 : path;
}

void addSaverFile(const char *path) {
  if (!path || nSaver >= 24) return;
  const char *base = saverBase(path);
  if (!base[0] || base[0] == '.' || !isJpgName(base)) return;
  strlcpy(saverFile[nSaver], path, sizeof(saverFile[0]));
  nSaver++;
}

void saverChildPath(char *out, int n, const char *dir, const char *name) {
  if (!name || !name[0]) {
    out[0] = 0;
    return;
  }
  if (name[0] == '/') {
    strlcpy(out, name, n);
    return;
  }
  if (dir[0] == '/' && !dir[1])
    snprintf(out, n, "/%s", name);
  else
    snprintf(out, n, "%s/%s", dir, name);
}

void scanSaverTree(const char *dir, int depth) {
  if (!saverFs || nSaver >= 24 || depth > 4) return;
  File d = saverFs->open(dir);
  if (!d) return;
  while (nSaver < 24) {
    File f = d.openNextFile();
    if (!f) break;
    bool isDir = f.isDirectory();
    const char *nm = f.name();
#if defined(ARDUINO_ARCH_ESP32)
    const char *full = f.path();
    if (full && full[0]) nm = full;
#endif
    char child[72];
    saverChildPath(child, sizeof(child), dir, nm);
    f.close();
    if (!child[0]) continue;
    const char *base = saverBase(child);
    if (base[0] == '.') continue;
    if (isDir) {
      if (!strcasecmp(base, "System Volume Information")) continue;
      scanSaverTree(child, depth + 1);
      continue;
    }
    saverSeen++;
    if (!saverFirst[0]) strlcpy(saverFirst, base, sizeof(saverFirst));
    addSaverFile(child);
  }
  d.close();
}

void initSd() {
  nSaver = 0;
  saverSeen = 0;
  saverFirst[0] = 0;
  if (sdOk) {
    SD.end();
    sdOk = false;
    saverFs = nullptr;
  }
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOk = SD.begin(SD_CS, SPI, 10000000);
  if (!sdOk) sdOk = SD.begin(SD_CS, SPI, 4000000);
  if (!sdOk) sdOk = SD.begin(SD_CS, SPI, 1000000);
  if (!sdOk) {
    saverFs = nullptr;
    return;
  }
  saverFs = &SD;
  scanSaverTree("/", 0);
  if (!nSaver && saverSeen) {
    File d = saverFs->open("/");
    if (d) {
      while (nSaver < 24) {
        File f = d.openNextFile();
        if (!f) break;
        bool isDir = f.isDirectory();
        const char *nm = f.name();
#if defined(ARDUINO_ARCH_ESP32)
        const char *full = f.path();
        if (full && full[0]) nm = full;
#endif
        char child[72];
        saverChildPath(child, sizeof(child), "/", nm);
        f.close();
        if (!isDir && child[0] && saverBase(child)[0] != '.') {
          strlcpy(saverFile[nSaver], child, sizeof(saverFile[0]));
          nSaver++;
        }
      }
      d.close();
    }
  }
}

void saverMessage(const char *a, const char *b) {
  gfx->fillScreen(COL_BLK);
  gfx->setTextColor(COL_MID, COL_BLK);
  gfx->setTextSize(1);
  centre(a, CX, 113);
  centre(b, CX, 134);
  saverSlideAt = millis();
}

bool drawSaverJpg(const char *path) {
  if (!saverFs || !ensureSaverFb()) return false;
  uint16_t w = 0, h = 0;
  if (TJpgDec.getFsJpgSize(&w, &h, path, *saverFs) != 0 || !w || !h) return false;
  uint8_t sc = 1;
  while (sc < 8 && (w / sc > PW || h / sc > PH)) sc = (uint8_t)(sc * 2);
  int dw = (int)w / sc, dh = (int)h / sc;
  int x = (PW - dw) / 2, y = (PH - dh) / 2;
  for (int i = 0; i < PW * PH; i++) saverFb[i] = COL_BLK;
  TJpgDec.setJpgScale(sc);
  TJpgDec.setCallback(saverJpgPlot);
  TJpgDec.setSwapBytes(false);
  bool ok = TJpgDec.drawFsJpg(x, y, path, *saverFs) == 0;
  if (!ok) {
    TJpgDec.setSwapBytes(true);
    for (int i = 0; i < PW * PH; i++) saverFb[i] = COL_BLK;
    ok = TJpgDec.drawFsJpg(x, y, path, *saverFs) == 0;
  }
  TJpgDec.setCallback(jpgPlot);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  if (!ok) return false;
  gfx->draw16bitRGBBitmap(0, 0, saverFb, PW, PH);
  return true;
}

void showSaverSlide() {
  if (!sdOk) {
    saverMessage("no SD card", "FAT32 card in the slot");
    return;
  }
  if (!nSaver) {
    char line[40];
    if (saverSeen)
      snprintf(line, sizeof(line), "%d files  %s", saverSeen, saverFirst);
    else
      snprintf(line, sizeof(line), "card ok, empty list");
    saverMessage("no jpg names found", line);
    return;
  }
  for (int n = 0; n < nSaver; n++) {
    int i = (saverIdx + n) % nSaver;
    if (drawSaverJpg(saverFile[i])) {
      saverIdx = (i + 1) % nSaver;
      saverSlideAt = millis();
      return;
    }
  }
  saverMessage("could not read jpg", "check the SD card");
}

void enterSaver() {
  if (page == PG_SAVER || page == PG_WIFI) return;
  initSd();
  saverReturn = page;
  page = PG_SAVER;
  gfx->fillScreen(COL_BLK);
  saverSlideAt = 0;
  if (!netBusy && !netJob) showSaverSlide();
}

void leaveSaver() {
  int back = saverReturn;
  if (back == PG_SAVER) back = PG_SCOPE;
  page = back;
  lastTouch = lastIdle = millis();
  paintPage(back);
}

void tickSaver() {
  if (page == PG_SAVER) {
    if ((!saverSlideAt || millis() - saverSlideAt > 10000UL) && !netBusy && !netJob)
      showSaverSlide();
    return;
  }
  if (saverMin && page != PG_WIFI && millis() - lastIdle >= (uint32_t)saverMin * 60000UL)
    enterSaver();
}

bool ensureJpgSrc() {
  if (jpgSrc) return true;
  jpgSrc = (uint16_t *)psAlloc((size_t)SRC_W * SRC_H * 2);
  return jpgSrc != nullptr;
}

bool ensureMapFb() {
  if (mapFb) return true;
  mapFb = (uint16_t *)psAlloc((size_t)PW * MAP_H * 2);
  return mapFb != nullptr;
}

void scaleMapUp() {
  if (!jpgSrc || !mapFb) return;
  int sw = mapSrcW > 0 ? mapSrcW : SRC_W;
  int sh = mapSrcH > 0 ? mapSrcH : SRC_H;
  for (int y = 0; y < MAP_H; y++) {
    int sy = y * sh / MAP_H;
    if (sy >= sh) sy = sh - 1;
    for (int x = 0; x < PW; x++) {
      int sx = x * sw / PW;
      if (sx >= sw) sx = sw - 1;
      mapFb[y * PW + x] = jpgSrc[sy * SRC_W + sx];
    }
    if ((y & 7) == 0) delay(1);
  }
}

void bbox(float &minLon, float &minLat, float &maxLon, float &maxLat) {
  float c = cosf(homeLat * 0.0174533f);
  if (c < 0.3f) c = 0.3f;
  int rr = rangeNm > 0 ? rangeNm : 1;
  int span = (rr * 3 + 1) / 2;
  float halfW = (float)CX * (float)span / (float)RR;
  float halfH = (float)CY * (float)span / (float)RR;
  float dLon = halfW / (60.0f * c);
  float dLat = halfH / 60.0f;
  minLon = homeLon - dLon;
  maxLon = homeLon + dLon;
  minLat = homeLat - dLat;
  maxLat = homeLat + dLat;
}

uint32_t mapSig() {
  uint32_t s = 2166136261u;
  for (int i = 0; i < SRC_W * SRC_H; i += 13) s = (s ^ jpgSrc[i]) * 16777619u;
  return s;
}

bool drawMapOnce(const uint8_t *src, int got, uint32_t *sig) {
  const uint16_t SENT = 0x0001;
  for (int i = 0; i < SRC_W * SRC_H; i++) jpgSrc[i] = SENT;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(jpgPlot);
  TJpgDec.setSwapBytes(false);
  mapPlotPace = true;
  bool ok = TJpgDec.drawJpg(0, 0, src, (uint32_t)got) == 0;
  if (!ok) {
    for (int i = 0; i < SRC_W * SRC_H; i++) jpgSrc[i] = SENT;
    TJpgDec.setSwapBytes(true);
    ok = TJpgDec.drawJpg(0, 0, src, (uint32_t)got) == 0;
  }
  mapPlotPace = false;
  TJpgDec.setSwapBytes(false);
  if (!ok) return false;
  int filled = 0;
  for (int i = 0; i < SRC_W * SRC_H; i++)
    if (jpgSrc[i] != SENT) filled++;
  if (filled < (SRC_W * SRC_H) / 20) return false;
  *sig = mapSig();
  return true;
}

bool fetchJpegMap(const char *url) {
  if (!ensureJpgSrc() || !ensureMapFb()) return false;
  uint8_t *buf = nullptr;
  int got = 0;
  if (!httpGetBin(url, &buf, &got)) return false;
  uint8_t *src = buf;
  uint8_t *hold = nullptr;
  if (got > 0 && got <= 120000)
    hold = (uint8_t *)heap_caps_malloc((size_t)got, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (hold) {
    for (int at = 0; at < got; at += 1024) {
      int n = got - at;
      if (n > 1024) n = 1024;
      memcpy(hold + at, buf + at, (size_t)n);
      delay(1);
    }
    src = hold;
  }
  uint32_t a = 0, b = 0;
  bool steady = drawMapOnce(src, got, &a) && drawMapOnce(src, got, &b) && a == b;
  if (hold) free(hold);
  free(buf);
  if (!steady) return false;
  jpgSrcClean = true;
  scaleMapUp();
  return true;
}

const char *mapService() {
  if (mapStyle) return "World_Imagery";
  if (!dayMode) return "Canvas/World_Dark_Gray_Base";
  return "World_Street_Map";
}

bool fetchMap() {
  lastMapTry = millis();
  if (!mapOn || !wifiLinked()) return false;
  float minLon, minLat, maxLon, maxLat;
  bbox(minLon, minLat, maxLon, maxLat);
  mapSrcW = SRC_W;
  mapSrcH = SRC_H;
  char url[420];
  const char *trySvc[2];
  trySvc[0] = mapService();
  trySvc[1] = (!dayMode && !mapStyle) ? "World_Imagery" : (mapStyle ? "World_Street_Map" : "World_Imagery");
  for (int pass = 0; pass < 3; pass++) {
    for (int k = 0; k < 2; k++) {
      if (k && !strcmp(trySvc[0], trySvc[1])) continue;
      snprintf(url, sizeof(url),
               "https://server.arcgisonline.com/ArcGIS/rest/services/%s/MapServer/export"
               "?bbox=%.5f,%.5f,%.5f,%.5f&bboxSR=4326&imageSR=4326&size=%d,%d&format=jpg&f=image",
               trySvc[k], minLon, minLat, maxLon, maxLat, mapSrcW, mapSrcH);
      if (fetchJpegMap(url)) {
        mapOk = true;
        mapHaveStyle = mapStyle;
        mapNm = rangeNm;
        bakeMapLook();
        if (stormOn) paintStormOnMap();
        lastMap = millis();
        return true;
      }
    }
  }
  return false;
}

extern "C" {
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf,
                                   size_t src_buf_len, int flags);
}

static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c) {
  int p = (int)a + (int)b - (int)c;
  int pa = abs(p - (int)a), pb = abs(p - (int)b), pc = abs(p - (int)c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

static bool pngRgba(const uint8_t *png, int len, uint8_t **pix, int *w, int *h) {
  *pix = nullptr;
  *w = *h = 0;
  if (!png || len < 32) return false;
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (memcmp(png, sig, 8) != 0) return false;
  int width = 0, height = 0, bpp = 0;
  uint8_t *idat = nullptr;
  int idn = 0, idCap = 0;
  int p = 8;
  while (p + 12 <= len) {
    uint32_t n = be32(png + p);
    if (n > 200000 || p + 12 + (int)n > len) break;
    const uint8_t *typ = png + p + 4;
    const uint8_t *data = png + p + 8;
    if (!memcmp(typ, "IHDR", 4) && n >= 13) {
      width = (int)be32(data);
      height = (int)be32(data + 4);
      if (data[8] != 8 || data[12] != 0) break;
      if (data[9] == 6) bpp = 4;
      else if (data[9] == 2) bpp = 3;
      else break;
    } else if (!memcmp(typ, "IDAT", 4) && n > 0) {
      if (idn + (int)n > 180000) break;
      if (idn + (int)n > idCap) {
        int nc = idn + (int)n + 1024;
        uint8_t *grow = (uint8_t *)psAlloc(nc);
        if (!grow) break;
        if (idat && idn) memcpy(grow, idat, idn);
        if (idat) free(idat);
        idat = grow;
        idCap = nc;
      }
      memcpy(idat + idn, data, n);
      idn += (int)n;
    } else if (!memcmp(typ, "IEND", 4))
      break;
    p += 12 + (int)n;
  }
  bool ok = false;
  uint8_t *raw = nullptr;
  uint8_t *out = nullptr;
  if (idat && bpp && width >= 8 && height >= 8 && width <= 640 && height <= 480) {
    size_t rawLen = (size_t)height * (width * bpp + 1);
    if (rawLen > 0 && rawLen < 400000) {
      raw = (uint8_t *)psAlloc(rawLen);
      size_t got = raw ? tinfl_decompress_mem_to_mem(raw, rawLen, idat, (size_t)idn, 1 | 4) : (size_t)-1;
      if (raw && got != (size_t)-1 && got >= rawLen) {
        out = (uint8_t *)psAlloc((size_t)width * height * 4);
        if (out) {
          int stride = width * bpp;
          uint8_t *prev = (uint8_t *)calloc(stride, 1);
          uint8_t *row = (uint8_t *)malloc(stride);
          if (prev && row) {
            for (int y = 0; y < height; y++) {
              const uint8_t *src = raw + y * (stride + 1);
              uint8_t ft = src[0];
              src++;
              for (int x = 0; x < stride; x++) {
                int left = x >= bpp ? row[x - bpp] : 0;
                int up = prev[x];
                int ul = x >= bpp ? prev[x - bpp] : 0;
                int v = src[x];
                if (ft == 1) v += left;
                else if (ft == 2) v += up;
                else if (ft == 3) v += (left + up) / 2;
                else if (ft == 4) v += paeth(left, up, ul);
                row[x] = (uint8_t)v;
              }
              memcpy(prev, row, stride);
              uint8_t *dst = out + y * width * 4;
              for (int x = 0; x < width; x++) {
                const uint8_t *s = row + x * bpp;
                dst[x * 4] = s[0];
                dst[x * 4 + 1] = s[1];
                dst[x * 4 + 2] = s[2];
                dst[x * 4 + 3] = bpp == 4 ? s[3] : 255;
              }
            }
            *pix = out;
            *w = width;
            *h = height;
            ok = true;
            out = nullptr;
          }
          if (prev) free(prev);
          if (row) free(row);
        }
      }
    }
  }
  if (out) free(out);
  if (raw) free(raw);
  if (idat) free(idat);
  return ok;
}

bool blendStorm(const uint8_t *pix, int sw, int sh) {
  if (!stormOn || !pix || !mapFb || sw < 1 || sh < 1) return false;
  for (int y = 0; y < MAP_H; y++) {
    if (!stormOn) return false;
    int sy = y * sh / MAP_H;
    if (sy >= sh) sy = sh - 1;
    const uint8_t *row = pix + sy * sw * 4;
    for (int x = 0; x < PW; x++) {
      int sx = x * sw / PW;
      if (sx >= sw) sx = sw - 1;
      const uint8_t *s = row + sx * 4;
      int a = s[3];
      if (a < 24) continue;
      uint16_t src = (uint16_t)(((s[0] >> 3) << 11) | ((s[1] >> 2) << 5) | (s[2] >> 3));
      if (a > 230) {
        mapFb[y * PW + x] = src;
        continue;
      }
      uint16_t dst = mapFb[y * PW + x];
      int dr = (dst >> 11) & 31, dg = (dst >> 5) & 63, db = dst & 31;
      int sr = (src >> 11) & 31, sg = (src >> 5) & 63, sb = src & 31;
      dr += (sr - dr) * a / 255;
      dg += (sg - dg) * a / 255;
      db += (sb - db) * a / 255;
      mapFb[y * PW + x] = (uint16_t)((dr << 11) | (dg << 5) | db);
    }
  }
  return stormOn;
}

void rebuildMapFb() {
  if (!jpgSrcClean || !mapFb) return;
  scaleMapUp();
  bakeMapLook();
}

void paintStormOnMap() {
  if (!stormOn || !mapOn || !mapOk || !mapFb) return;
  if (!jpgSrcClean) {
    askNet(NET_MAP);
    return;
  }
  float minLon, minLat, maxLon, maxLat;
  bbox(minLon, minLat, maxLon, maxLat);
  char url[360];
  snprintf(url, sizeof(url),
           "http://mesonet.agron.iastate.edu/cgi-bin/wms/nexrad/n0q.cgi?SERVICE=WMS&REQUEST=GetMap"
           "&VERSION=1.1.1&LAYERS=nexrad-n0q-900913&FORMAT=image/png&TRANSPARENT=TRUE&SRS=EPSG:4326"
           "&BBOX=%.5f,%.5f,%.5f,%.5f&WIDTH=%d&HEIGHT=%d",
           minLon, minLat, maxLon, maxLat, SRC_W, SRC_H);
  uint8_t *file = nullptr;
  int flen = 0;
  uint8_t *pix = nullptr;
  int sw = 0, sh = 0;
  bool got = httpGetFile(url, &file, &flen, 160000, 8000) && pngRgba(file, flen, &pix, &sw, &sh);
  if (file) free(file);
  bool painted = got && stormOn && blendStorm(pix, sw, sh);
  if (pix) free(pix);
  if (!stormOn) {
    if (jpgSrcClean) rebuildMapFb();
    lastStorm = millis();
    return;
  }
  lastStorm = millis() - (painted ? 0 : 240000UL);
}

void fetchStorm() {
  if (!stormOn || !mapOn) return;
  if (!jpgSrcClean) {
    fetchMap();
    return;
  }
  scaleMapUp();
  bakeMapLook();
  paintStormOnMap();
}

bool ensureIssFb() {
  if (issFb) return true;
  issFb = (uint16_t *)ps_malloc((size_t)PW * ISS_H * 2);
  return issFb != nullptr;
}

bool fetchIssMap() {
  lastIssMapTry = millis();
  if (issMapOk) return true;
  if (!ensureJpgSrc() || !ensureIssFb()) return false;
  jpgSrcClean = false;
  const uint16_t SENT = 0x0001;
  for (int i = 0; i < SRC_W * SRC_H; i++) jpgSrc[i] = SENT;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(jpgPlot);
  TJpgDec.setSwapBytes(false);
  bool ok = TJpgDec.drawJpg(0, 0, ISS_WORLD_JPG, ISS_WORLD_JPG_LEN) == 0;
  if (!ok) {
    TJpgDec.setSwapBytes(true);
    ok = TJpgDec.drawJpg(0, 0, ISS_WORLD_JPG, ISS_WORLD_JPG_LEN) == 0;
  }
  TJpgDec.setSwapBytes(false);
  if (!ok) return false;
  int filled = 0;
  for (int i = 0; i < ISS_SRC_W * ISS_SRC_H; i++) {
    int x = i % ISS_SRC_W, y = i / ISS_SRC_W;
    if (jpgSrc[y * SRC_W + x] != SENT) filled++;
  }
  if (filled < (ISS_SRC_W * ISS_SRC_H) / 20) return false;
  for (int y = 0; y < ISS_H; y++) {
    int sy = y * ISS_SRC_H / ISS_H;
    if (sy >= ISS_SRC_H) sy = ISS_SRC_H - 1;
    for (int x = 0; x < PW; x++) {
      int sx = x * ISS_SRC_W / PW;
      if (sx >= ISS_SRC_W) sx = ISS_SRC_W - 1;
      issFb[y * PW + x] = jpgSrc[sy * SRC_W + sx];
    }
  }
  issMapOk = true;
  return true;
}

void copyCall(char *dst, size_t n, const char *a, const char *b) {
  const char *src = a ? a : "";
  while (*src == ' ') src++;
  if (!*src) {
    src = b ? b : "";
    while (*src == ' ') src++;
  }
  if (!*src) src = "----";
  strlcpy(dst, src, n);
  for (char *s = dst; *s; ++s) {
    if (*s == ' ') {
      *s = 0;
      break;
    }
  }
}

void tagGeom(int ix, int iy, const char *call, int &bx, int &by, int &bw, int &bh) {
  const char *tag = (call && call[0]) ? call : "----";
  int tw = (int)strlen(tag) * 6;
  bw = tw + 6;
  bh = 12;
  bx = ix + 5;
  by = iy - 14;
  if (bx + bw + 1 > PW - 2) bx = ix - bw - 4;
  if (bx < 2) bx = 2;
  if (by < 2) by = iy + 5;
  if (by + bh > MAP_H) by = iy - 14;
}

void planeArrow(int ix, int iy, float hdg, uint16_t pc) {
  float h = hdg * 0.0174533f;
  int tipx = ix + (int)(sinf(h) * 9);
  int tipy = iy - (int)(cosf(h) * 9);
  int bxx = ix - (int)(sinf(h) * 4);
  int byy = iy + (int)(cosf(h) * 4);
  int px = (int)(cosf(h) * 5);
  int py = (int)(sinf(h) * 5);
  gfx->fillTriangle(tipx, tipy, bxx - px, byy - py, bxx + px, byy + py, pc);
}

void planeTag(int idx, int ix, int iy, const char *call, uint16_t pc) {
  char tag[10];
  strlcpy(tag, (call && call[0]) ? call : "----", sizeof(tag));
  int bx, by, bw, bh;
  tagGeom(ix, iy, tag, bx, by, bw, bh);
  gfx->setTextSize(1);
  gfx->fillRoundRect(bx, by, bw, bh, 2, COL_BLK);
  gfx->drawRoundRect(bx, by, bw, bh, 2, pc);
  gfx->setTextColor(COL_YEL, COL_BLK);
  gfx->setCursor(bx + 3, by + 2);
  gfx->print(tag);
  if (nMarks < HFS_MAXP) {
    marks[nMarks].idx = idx;
    marks[nMarks].x = ix;
    marks[nMarks].y = iy;
    marks[nMarks].bx = bx;
    marks[nMarks].by = by;
    marks[nMarks].bw = bw;
    marks[nMarks].bh = bh;
    nMarks++;
  }
}

int findPlane(const char *hex, const char *call) {
  if (hex && hex[0]) {
    for (int i = 0; i < nPlanes; i++) {
      if (planes[i].hex[0] && !strcasecmp(planes[i].hex, hex)) return i;
    }
  }
  if (call && call[0] && call[0] != '-') {
    for (int i = 0; i < nPlanes; i++) {
      if (planes[i].call[0] && !strcasecmp(planes[i].call, call)) return i;
    }
  }
  return -1;
}

void rememberSel() {
  selHex[0] = 0;
  if (selected >= 0 && selected < nPlanes && planes[selected].hex[0])
    strlcpy(selHex, planes[selected].hex, sizeof(selHex));
}

void restoreSel() {
  if (selHex[0]) {
    int i = findPlane(selHex, nullptr);
    if (i >= 0) {
      selected = i;
      return;
    }
  }
  if (selected >= nPlanes) selected = nPlanes ? 0 : -1;
}

void copyFacts(Plane &p, const Plane &k, bool routeToo) {
  if (routeToo && k.routeOk) {
    p.routeOk = true;
    strlcpy(p.orig, k.orig, sizeof(p.orig));
    strlcpy(p.dest, k.dest, sizeof(p.dest));
    strlcpy(p.origCity, k.origCity, sizeof(p.origCity));
    strlcpy(p.destCity, k.destCity, sizeof(p.destCity));
    strlcpy(p.airline, k.airline, sizeof(p.airline));
    acceptRoute(p, 0, 0, 0, 0);
  }
  if (k.factsOk) {
    p.factsOk = true;
    strlcpy(p.typeName, k.typeName, sizeof(p.typeName));
    strlcpy(p.mfr, k.mfr, sizeof(p.mfr));
    strlcpy(p.owner, k.owner, sizeof(p.owner));
    strlcpy(p.engines, k.engines, sizeof(p.engines));
    p.seats = k.seats;
    p.cruise = k.cruise;
    p.fl = k.fl;
  }
  if (k.timesOk) {
    p.timesOk = true;
    strlcpy(p.dep, k.dep, sizeof(p.dep));
    strlcpy(p.arr, k.arr, sizeof(p.arr));
    p.durMin = k.durMin;
  }
  p.maxAlt = k.maxAlt;
  p.maxGs = k.maxGs;
  if (p.alt > p.maxAlt) p.maxAlt = p.alt;
  if (p.gs > p.maxGs) p.maxGs = p.gs;
}

void applyKeep(Plane &p, const Plane *keep, int nKeep) {
  int hexHit = -1, callHit = -1;
  for (int i = 0; i < nKeep; i++) {
    if (hexHit < 0 && p.hex[0] && keep[i].hex[0] && !strcasecmp(p.hex, keep[i].hex))
      hexHit = i;
    if (callHit < 0 && p.call[0] && p.call[0] != '-' && keep[i].call[0] &&
        !strcasecmp(p.call, keep[i].call))
      callHit = i;
  }
  if (hexHit >= 0)
    copyFacts(p, keep[hexHit], true);
  else if (callHit >= 0)
    copyFacts(p, keep[callHit], false);
  else {
    if (p.alt > p.maxAlt) p.maxAlt = p.alt;
    if (p.gs > p.maxGs) p.maxGs = p.gs;
  }
}

void applyFactHold(Plane &p) {
  if (!factHoldOk) return;
  if (p.hex[0] && factHold.hex[0] && !strcasecmp(p.hex, factHold.hex))
    copyFacts(p, factHold, true);
}

void stashFacts(const Plane &p) {
  if (p.routeOk || p.factsOk || p.timesOk) {
    factHold = p;
    factHoldOk = true;
  }
}

bool airportCode(const char *s) {
  if (!s || !s[0] || s[0] == '-' || isdigit((unsigned char)s[0])) return false;
  int n = 0;
  while (s[n] && isalnum((unsigned char)s[n])) n++;
  return n >= 3 && n <= 4;
}

void applyRouteCodes(Plane &p, const char *o, const char *d) {
  if (airportCode(o)) {
    char code[8] = {0};
    int n = 0;
    while (o[n] && n < (int)sizeof(code) - 1 && isalnum((unsigned char)o[n])) {
      code[n] = (char)toupper((unsigned char)o[n]);
      n++;
    }
    if (strcasecmp(p.orig, code)) {
      strlcpy(p.orig, code, sizeof(p.orig));
      p.origCity[0] = 0;
    }
  }
  if (airportCode(d)) {
    char code[8] = {0};
    int n = 0;
    while (d[n] && n < (int)sizeof(code) - 1 && isalnum((unsigned char)d[n])) {
      code[n] = (char)toupper((unsigned char)d[n]);
      n++;
    }
    if (strcasecmp(p.dest, code)) {
      strlcpy(p.dest, code, sizeof(p.dest));
      p.destCity[0] = 0;
    }
  }
  if (p.orig[0] || p.dest[0]) {
    p.routeOk = true;
    const char *fn = placeName(p.orig, p.origCity);
    const char *tn = placeName(p.dest, p.destCity);
    if (fn[0]) strlcpy(p.origCity, fn, sizeof(p.origCity));
    if (tn[0]) strlcpy(p.destCity, tn, sizeof(p.destCity));
  }
}

void applyRouteStr(Plane &p, const char *route) {
  if (!route || !route[0]) return;
  char a[8] = {0}, b[8] = {0};
  while (*route && !isalpha((unsigned char)*route)) route++;
  int i = 0;
  while (route[i] && isalnum((unsigned char)route[i]) && i < 4) {
    a[i] = (char)toupper((unsigned char)route[i]);
    i++;
  }
  const char *r = route + i;
  while (*r && !isalpha((unsigned char)*r)) r++;
  i = 0;
  while (r[i] && isalnum((unsigned char)r[i]) && i < 4) {
    b[i] = (char)toupper((unsigned char)r[i]);
    i++;
  }
  if (strlen(a) >= 3 && strlen(b) >= 3) applyRouteCodes(p, a, b);
}

int snapshotKeep() {
  int nKeep = nPlanes;
  if (nKeep > HFS_MAXP) nKeep = HFS_MAXP;
  if (nKeep > 0) memcpy(keepStore, planes, sizeof(Plane) * nKeep);
  return nKeep;
}

void considerPlane(Plane &tmp, int &nBuild) {
  if (tmp.lat == 0.0f && tmp.lon == 0.0f) return;
  float d = nmBetween(tmp.lat, tmp.lon, homeLat, homeLon);
  if (d > (float)rangeNm + 8.0f) return;
  if (nBuild < HFS_MAXP) {
    inboundPlanes[nBuild++] = tmp;
    return;
  }
  int far = 0;
  float farD = nmBetween(inboundPlanes[0].lat, inboundPlanes[0].lon, homeLat, homeLon);
  for (int i = 1; i < nBuild; i++) {
    float di = nmBetween(inboundPlanes[i].lat, inboundPlanes[i].lon, homeLat, homeLon);
    if (di > farD) {
      farD = di;
      far = i;
    }
  }
  if (d < farD) inboundPlanes[far] = tmp;
}

void commitPlanes(int nBuild) {
  if (nBuild > 0) memcpy(planes, inboundPlanes, sizeof(Plane) * (size_t)nBuild);
  nPlanes = nBuild;
}

void fillTypeFacts(Plane &p) {
  struct Row {
    const char *icao, *name, *mfr, *eng;
    int seats, cruise, fl;
  };
  static const Row rows[] = {
      {"B738", "737-800", "Boeing", "2x CFM56-7B", 189, 453, 410},
      {"B737", "737-700", "Boeing", "2x CFM56-7B", 143, 450, 410},
      {"B739", "737-900", "Boeing", "2x CFM56-7B", 189, 453, 410},
      {"B38M", "737 MAX 8", "Boeing", "2x LEAP-1B", 189, 453, 410},
      {"A320", "A320", "Airbus", "2x CFM56/V2500", 180, 447, 390},
      {"A321", "A321", "Airbus", "2x CFM/IAE", 220, 447, 390},
      {"A21N", "A321neo", "Airbus", "2x LEAP/PW1100", 220, 450, 390},
      {"A20N", "A320neo", "Airbus", "2x LEAP/PW1100", 180, 450, 390},
      {"A319", "A319", "Airbus", "2x CFM56", 156, 447, 390},
      {"B77W", "777-300ER", "Boeing", "2x GE90-115B", 396, 490, 431},
      {"B772", "777-200", "Boeing", "2x GE90/Trent", 314, 490, 431},
      {"B788", "787-8", "Boeing", "2x GEnx/Trent", 242, 488, 430},
      {"B789", "787-9", "Boeing", "2x GEnx/Trent", 296, 488, 430},
      {"A333", "A330-300", "Airbus", "2x Trent/PW", 335, 470, 411},
      {"E75L", "E175", "Embraer", "2x CF34-8E", 76, 430, 410},
      {"E190", "E190", "Embraer", "2x CF34-10E", 100, 447, 410},
      {"CRJ9", "CRJ-900", "Bombardier", "2x CF34-8C", 76, 447, 410},
      {"CRJ7", "CRJ-700", "Bombardier", "2x CF34-8C", 70, 447, 410},
      {"BCS3", "A220-300", "Airbus", "2x PW1500G", 160, 447, 410},
      {nullptr, nullptr, nullptr, nullptr, 0, 0, 0}};
  for (int i = 0; rows[i].icao; i++) {
    if (!strcasecmp(p.type, rows[i].icao)) {
      strlcpy(p.typeName, rows[i].name, sizeof(p.typeName));
      strlcpy(p.mfr, rows[i].mfr, sizeof(p.mfr));
      strlcpy(p.engines, rows[i].eng, sizeof(p.engines));
      p.seats = rows[i].seats;
      p.cruise = rows[i].cruise;
      p.fl = rows[i].fl;
      p.factsOk = true;
      return;
    }
  }
  if (!p.typeName[0] && p.type[0]) strlcpy(p.typeName, p.type, sizeof(p.typeName));
}

void clockLocal(time_t t, struct tm *ti) {
  localtime_r(&t, ti);
}

void fmtLocalClock(time_t t, char *out, int n) {
  if (t < 1700000000) {
    out[0] = 0;
    return;
  }
  struct tm ti;
  clockLocal(t, &ti);
  int hr = ti.tm_hour % 12;
  if (!hr) hr = 12;
  snprintf(out, n, "%d:%02d%s", hr, ti.tm_min, ti.tm_hour >= 12 ? "PM" : "AM");
}

bool planeNearHome(const Plane &p) {
  return p.alt < 8000 && nmBetween(p.lat, p.lon, homeLat, homeLon) <= 15.0f;
}

void pinHomeEnd(Plane &p) {
  if (!planeNearHome(p)) return;
  if (icaoMatch(p.orig, homeIcao) || icaoMatch(p.dest, homeIcao)) return;
  if (p.gs < 80 || headingToward(p, homeLat, homeLon)) {
    strlcpy(p.dest, homeIcao, sizeof(p.dest));
    p.destCity[0] = 0;
  } else {
    strlcpy(p.orig, homeIcao, sizeof(p.orig));
    p.origCity[0] = 0;
  }
}

bool trackDelta(const Plane &p, float lat, float lon, float &deg) {
  deg = 999.0f;
  if (p.gs < 80) return false;
  if (lat == 0.0f && lon == 0.0f) return false;
  if (p.lat == 0.0f && p.lon == 0.0f) return false;
  float brg = brgDeg(p.lat, p.lon, lat, lon);
  float d = fabsf((float)p.hdg - brg);
  if (d > 180.0f) d = 360.0f - d;
  deg = d;
  return true;
}

bool acceptRoute(Plane &p, float oLat, float oLon, float dLat, float dLon) {
  if ((oLat == 0 && oLon == 0) || (dLat == 0 && dLon == 0)) {
    float a = 0, b = 0, c = 0, e = 0;
    if (fieldLL(p.orig, a, b)) {
      oLat = a;
      oLon = b;
    }
    if (fieldLL(p.dest, c, e)) {
      dLat = c;
      dLon = e;
    }
  }
  float deg = 999.0f;
  bool have = trackDelta(p, dLat, dLon, deg);
  if (have && deg > 90.0f) {
    clearRoute(p);
    return false;
  }
  if (routeMissesPlane(p, oLat, oLon, dLat, dLon) && !(have && deg <= 45.0f)) {
    clearRoute(p);
    return false;
  }
  finishTowns(p);
  return p.orig[0] || p.dest[0] || p.origCity[0] || p.destCity[0];
}

bool applyAdsbdb(Plane &p, JsonDocument &doc) {
  JsonObject a = doc["response"]["aircraft"];
  if (!a.isNull()) {
    if (!p.type[0]) strlcpy(p.type, a["icao_type"] | "", sizeof(p.type));
    strlcpy(p.typeName, a["type"] | p.typeName, sizeof(p.typeName));
    strlcpy(p.mfr, a["manufacturer"] | p.mfr, sizeof(p.mfr));
    strlcpy(p.owner, a["registered_owner"] | p.owner, sizeof(p.owner));
    if (!p.reg[0]) strlcpy(p.reg, a["registration"] | "", sizeof(p.reg));
    fillTypeFacts(p);
    p.factsOk = true;
  }
  JsonObject fr = doc["response"]["flightroute"];
  if (fr.isNull()) return false;
  JsonObject o = fr["origin"];
  JsonObject d = fr["destination"];
  if (o.isNull() && d.isNull()) return false;
  strlcpy(p.orig, o["icao_code"] | "", sizeof(p.orig));
  strlcpy(p.dest, d["icao_code"] | "", sizeof(p.dest));
  strlcpy(p.origCity, o["municipality"] | o["iata_code"] | p.orig, sizeof(p.origCity));
  strlcpy(p.destCity, d["municipality"] | d["iata_code"] | p.dest, sizeof(p.destCity));
  strlcpy(p.airline, fr["airline"]["name"] | "", sizeof(p.airline));
  p.routeOk = p.orig[0] || p.dest[0] || p.origCity[0] || p.destCity[0];
  float oLat = o["latitude"] | 0.0f;
  float oLon = o["longitude"] | 0.0f;
  float dLat = d["latitude"] | 0.0f;
  float dLon = d["longitude"] | 0.0f;
  return acceptRoute(p, oLat, oLon, dLat, dLon);
}

bool findIcaoAfter(const char *s, const char *key, char *out, int n) {
  if (!s || !key || n < 4) return false;
  const char *h = strstr(s, key);
  if (!h) return false;
  h += strlen(key);
  while (*h && !isalpha((unsigned char)*h)) h++;
  int i = 0;
  while (h[i] && isalnum((unsigned char)h[i]) && i < n - 1 && i < 4) {
    out[i] = (char)toupper((unsigned char)h[i]);
    i++;
  }
  out[i] = 0;
  return i >= 3;
}

void pullRouteFromBody(Plane &p, const char *s) {
  if (!s) return;
  char o[8] = {0}, d[8] = {0};
  findIcaoAfter(s, "\"origin\"", o, sizeof(o));
  if (!o[0]) findIcaoAfter(s, "\"icao_code\"", o, sizeof(o));
  const char *second = s;
  if (o[0]) {
    const char *hit = strstr(s, o);
    if (hit) second = hit + strlen(o);
  }
  findIcaoAfter(second, "\"destination\"", d, sizeof(d));
  if (!d[0]) findIcaoAfter(second, "\"icao_code\"", d, sizeof(d));
  if (o[0] || d[0]) {
    applyRouteCodes(p, o, d);
    return;
  }
  const char *rte = strstr(s, "\"route\"");
  if (rte) applyRouteStr(p, rte);
  else applyRouteStr(p, s);
}

bool haveTowns(const Plane &p) {
  return p.origCity[0] && p.destCity[0];
}

void finishTowns(Plane &p) {
  const char *t = placeName(p.orig, p.origCity);
  if (t[0]) strlcpy(p.origCity, t, sizeof(p.origCity));
  else if (!p.origCity[0] && p.orig[0]) strlcpy(p.origCity, p.orig, sizeof(p.origCity));
  t = placeName(p.dest, p.destCity);
  if (t[0]) strlcpy(p.destCity, t, sizeof(p.destCity));
  else if (!p.destCity[0] && p.dest[0]) strlcpy(p.destCity, p.dest, sizeof(p.destCity));
  p.routeOk = p.orig[0] || p.dest[0] || p.origCity[0] || p.destCity[0];
}

float jsonNumAfter(const char *s, const char *lim, const char *key) {
  if (!s || !key) return 0;
  const char *h = strstr(s, key);
  if (!h || (lim && h >= lim)) return 0;
  h += strlen(key);
  while (*h && (*h == '"' || *h == ':' || *h == ' ' || *h == '\t')) h++;
  return (float)atof(h);
}

void pullTownsFromBody(Plane &p, const char *s) {
  if (!s || haveTowns(p)) return;
  const char *org = strstr(s, "\"origin\"");
  const char *dst = strstr(s, "\"destination\"");
  if (!org) return;
  const char *limO = dst ? dst : org + 2000;
  jsonKey(org, limO, "\"municipality\":", p.origCity, sizeof(p.origCity));
  if (!p.orig[0]) jsonKey(org, limO, "\"icao_code\":", p.orig, sizeof(p.orig));
  if (dst) {
    jsonKey(dst, dst + 2000, "\"municipality\":", p.destCity, sizeof(p.destCity));
    if (!p.dest[0]) jsonKey(dst, dst + 2000, "\"icao_code\":", p.dest, sizeof(p.dest));
  }
  float oLat = jsonNumAfter(org, limO, "\"latitude\"");
  float oLon = jsonNumAfter(org, limO, "\"longitude\"");
  float dLat = 0, dLon = 0;
  if (dst) {
    dLat = jsonNumAfter(dst, dst + 2000, "\"latitude\"");
    dLon = jsonNumAfter(dst, dst + 2000, "\"longitude\"");
  }
  acceptRoute(p, oLat, oLon, dLat, dLon);
}

bool fieldLL(const char *icao, float &lat, float &lon) {
  if (!icao || !icao[0]) return false;
  for (int i = 0; homeFields[i].icao; i++) {
    if (icaoMatch(icao, homeFields[i].icao)) {
      lat = homeFields[i].lat;
      lon = homeFields[i].lon;
      return true;
    }
  }
  static const struct {
    const char *id;
    float la, lo;
  } extraLL[] = {{"EGLL", 51.4700f, -0.4543f},  {"EGKK", 51.1537f, -0.1821f},
                 {"EGSS", 51.8850f, 0.2350f},   {"EGPH", 55.9500f, -3.3725f},
                 {"EGCC", 53.3537f, -2.2750f},  {"LFPG", 49.0097f, 2.5479f},
                 {"EHAM", 52.3105f, 4.7683f},   {"EDDF", 50.0379f, 8.5622f},
                 {"EDDM", 48.3538f, 11.7861f},  {"LEMD", 40.4983f, -3.5676f},
                 {"LIRF", 41.8003f, 12.2389f},  {"RJTT", 35.5494f, 139.7798f},
                 {"RJAA", 35.7647f, 140.3864f}, {"VHHH", 22.3080f, 113.9185f},
                 {"OMDB", 25.2532f, 55.3657f},  {"OTHH", 25.2731f, 51.6081f},
                 {"YSSY", -33.9399f, 151.1753f},{"YMML", -37.6733f, 144.8433f},
                 {"CYYZ", 43.6772f, -79.6306f}, {"CYVR", 49.1947f, -123.1840f},
                 {"CYUL", 45.4706f, -73.7408f}, {"MMMX", 19.4363f, -99.0721f},
                 {"WSSS", 1.3644f, 103.9915f},  {"VTBS", 13.6811f, 100.7475f},
                 {"ZBAA", 40.0799f, 116.6031f}, {"VABB", 19.0896f, 72.8656f},
                 {nullptr, 0, 0}};
  for (int n = 0; extraLL[n].id; n++) {
    if (icaoMatch(icao, extraLL[n].id)) {
      lat = extraLL[n].la;
      lon = extraLL[n].lo;
      return true;
    }
  }
  return false;
}

bool altCallsign(const char *call, char *out, int n) {
  static const struct {
    const char *icao, *iata;
  } map[] = {{"AAL", "AA"}, {"UAL", "UA"}, {"DAL", "DL"}, {"SWA", "WN"}, {"ASA", "AS"},
             {"JBU", "B6"}, {"NKS", "NK"}, {"FFT", "F9"}, {"SKW", "OO"}, {"EDV", "9E"},
             {"RPA", "YX"}, {"ENY", "MQ"}, {"ASH", "YV"}, {"HAL", "HA"}, {"QXE", "QX"},
             {"CPZ", "CP"}, {"GJS", "G7"}, {"AWI", "ZW"}, {nullptr, nullptr}};
  if (!call || !out || n < 6) return false;
  for (int i = 0; map[i].icao; i++) {
    int a = (int)strlen(map[i].icao);
    int b = (int)strlen(map[i].iata);
    if (!strncmp(call, map[i].icao, a) && isdigit((unsigned char)call[a])) {
      snprintf(out, n, "%s%s", map[i].iata, call + a);
      return true;
    }
    if (!strncmp(call, map[i].iata, b) && isdigit((unsigned char)call[b])) {
      snprintf(out, n, "%s%s", map[i].icao, call + b);
      return true;
    }
  }
  return false;
}

bool fetchCallsignRoute(Plane &p, char *body, int CAP, const char *call) {
  if (!call || !call[0] || call[0] == '-') return false;
  char url[200];
  snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", call);
  if (!httpGetSmall(url, body, CAP, 5000)) return false;
  bool saw = false;
  JsonDocument doc;
  if (!deserializeJson(doc, body)) {
    saw = !doc["response"]["flightroute"].isNull();
    applyAdsbdb(p, doc);
  }
  if (!haveTowns(p) && !saw) pullTownsFromBody(p, body);
  return haveTowns(p);
}

void fetchHexdbRoute(Plane &p, char *body, int CAP, const char *call) {
  if (!call || !call[0] || call[0] == '-') return;
  char url[200];
  snprintf(url, sizeof(url), "https://hexdb.net/callsign-route-api?callsign=%s", call);
  if (!httpGetSmall(url, body, CAP, 3000)) return;
  const char *rte = strstr(body, "\"route\"");
  if (!rte) rte = strstr(body, "\"Route\"");
  if (rte) {
    rte = strchr(rte + 7, '"');
    if (rte) applyRouteStr(p, rte + 1);
  } else if (airportCode(body))
    applyRouteStr(p, body);
  acceptRoute(p, 0, 0, 0, 0);
}

bool sameToken(const char *a, const char *b) {
  if (!a || !b || !a[0] || !b[0]) return false;
  while (*a && *b) {
    while (*a == ' ') a++;
    while (*b == ' ') b++;
    if (!*a || !*b) break;
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return false;
    a++;
    b++;
  }
  while (*a == ' ') a++;
  while (*b == ' ') b++;
  return !*a && !*b;
}

bool takeFr24Live(Plane &p, JsonObject det, char *bestO, char *bestD, float &bestNm, bool &got) {
  if (det.isNull()) return false;
  const char *from = det["schd_from"] | "";
  const char *to = det["schd_to"] | "";
  if (!airportCode(from) || !airportCode(to)) return false;
  const char *cs = det["callsign"] | "";
  const char *rg = det["reg"] | "";
  bool idOk = sameToken(cs, p.call) || (p.reg[0] && sameToken(rg, p.reg));
  if (!idOk) return false;
  float lat = det["lat"] | 0.0f;
  float lon = det["lon"] | 0.0f;
  if ((lat == 0 && lon == 0) || (p.lat == 0 && p.lon == 0)) return false;
  float dist = nmBetween(p.lat, p.lon, lat, lon);
  if (dist > 150.0f) return false;
  if (!got || dist < bestNm) {
    strlcpy(bestO, from, 8);
    strlcpy(bestD, to, 8);
    bestNm = dist;
    got = true;
  }
  return true;
}

bool fetchFr24LiveRoute(Plane &p, char *body, int CAP, const char *qraw) {
  if (!qraw || !qraw[0] || qraw[0] == '-') return false;
  char q[12] = {0};
  int n = 0;
  for (const char *s = qraw; *s && n < 10; s++) {
    if (*s != ' ' && *s != '-') q[n++] = *s;
  }
  if (n < 3) return false;
  char url[220];
  snprintf(url, sizeof(url),
           "https://www.flightradar24.com/v1/search/web/find?query=%s&limit=8", q);
  if (!httpGetSmall(url, body, CAP, 5000)) return false;
  char bestO[8] = {0}, bestD[8] = {0};
  float bestNm = 9999.0f;
  bool got = false;
  JsonDocument doc;
  if (!deserializeJson(doc, body) && doc["results"].is<JsonArray>()) {
    for (JsonObject r : doc["results"].as<JsonArray>()) {
      if (strcmp(r["type"] | "", "live")) continue;
      takeFr24Live(p, r["detail"], bestO, bestD, bestNm, got);
    }
  }
  if (!got) return false;
  applyRouteCodes(p, bestO, bestD);
  return acceptRoute(p, 0, 0, 0, 0);
}

void fetchOpenskyRoute(Plane &p, char *body, int CAP) {
  if (!body || !p.hex[0]) return;
  time_t now = 0;
  if (!liveClock(&now) || now < 1700000000) return;
  char hex[12];
  strlcpy(hex, p.hex, sizeof(hex));
  for (char *s = hex; *s; ++s) *s = (char)tolower((unsigned char)*s);
  char url[220];
  snprintf(url, sizeof(url),
           "https://opensky-network.org/api/flights/aircraft?icao24=%s&begin=%lu&end=%lu", hex,
           (unsigned long)(now - 28800UL), (unsigned long)now);
  if (!httpGetSmall(url, body, CAP, 4000)) return;
  const char *dep = nullptr;
  for (const char *scan = body; (scan = strstr(scan, "\"estDepartureAirport\"")); scan += 20)
    dep = scan;
  if (!dep) return;
  const char *arr = strstr(dep, "\"estArrivalAirport\"");
  char o[8] = {0}, d[8] = {0};
  const char *q = strchr(dep, ':');
  if (q) {
    q = strchr(q, '"');
    if (q && q[1] && q[1] != '"' && strncmp(q + 1, "null", 4)) {
      q++;
      int i = 0;
      while (q[i] && q[i] != '"' && i < 4) {
        o[i] = (char)toupper((unsigned char)q[i]);
        i++;
      }
    }
  }
  if (arr) {
    q = strchr(arr, ':');
    if (q) {
      q = strchr(q, '"');
      if (q && q[1] && q[1] != '"' && strncmp(q + 1, "null", 4)) {
        q++;
        int i = 0;
        while (q[i] && q[i] != '"' && i < 4) {
          d[i] = (char)toupper((unsigned char)q[i]);
          i++;
        }
      }
    }
  }
  if (!o[0] && !d[0]) return;
  if (o[0] && d[0])
    applyRouteCodes(p, o, d);
  else if (o[0]) {
    applyRouteCodes(p, o, "");
  }
  acceptRoute(p, 0, 0, 0, 0);
}

void fetchAircraft(Plane &p) {
  if (!p.hex[0] && !p.reg[0] && !(p.call[0] && p.call[0] != '-')) return;
  const int CAP = 12000;
  char *body = (char *)psAlloc(CAP);
  if (!body) return;
  clearRoute(p);
  if (p.call[0] && p.call[0] != '-') {
    fetchCallsignRoute(p, body, CAP, p.call);
    if (!haveTowns(p)) {
      char alt[12];
      if (altCallsign(p.call, alt, sizeof(alt))) fetchCallsignRoute(p, body, CAP, alt);
    }
  }
  if (!haveTowns(p) && p.call[0] && p.call[0] != '-') {
    fetchFr24LiveRoute(p, body, CAP, p.call);
    if (!haveTowns(p)) {
      char alt[12];
      if (altCallsign(p.call, alt, sizeof(alt))) fetchFr24LiveRoute(p, body, CAP, alt);
    }
  }
  if (!haveTowns(p) && p.reg[0]) fetchFr24LiveRoute(p, body, CAP, p.reg);
  if (!haveTowns(p)) fetchOpenskyRoute(p, body, CAP);
  if (!p.orig[0] && !p.dest[0]) clearRoute(p);
  else finishTowns(p);
  free(body);
  fillTypeFacts(p);
  stashFacts(p);
}

bool parsePlanes(JsonDocument &doc) {
  if (doc["error"].is<const char*>()) return false;
  if (!doc["ac"].is<JsonArray>()) return false;
  JsonArray ac = doc["ac"].as<JsonArray>();
  int nKeep = snapshotKeep();
  rememberSel();
  int nBuild = 0;
  for (JsonObject o : ac) {
    if (o["lat"].isNull() || o["lon"].isNull()) continue;
    Plane p;
    memset(&p, 0, sizeof(p));
    p.lat = o["lat"].as<float>();
    p.lon = o["lon"].as<float>();
    if (o["alt_baro"].is<const char*>()) p.alt = 0;
    else {
      p.alt = (int)o["alt_baro"].as<float>();
      if (p.alt > 60000) p.alt = 0;
    }
    p.gs = (int)(o["gs"] | 0);
    p.hdg = (int)(o["track"] | 0);
    p.vs = (int)(o["baro_rate"] | 0);
    p.squawk = atoi(o["squawk"] | "0");
    strlcpy(p.hex, o["hex"] | "", sizeof(p.hex));
    copyCall(p.call, sizeof(p.call), o["flight"] | "", o["r"] | "");
    strlcpy(p.reg, o["r"] | "", sizeof(p.reg));
    strlcpy(p.type, o["t"] | "", sizeof(p.type));
    applyKeep(p, keepStore, nKeep);
    applyFactHold(p);
    fillTypeFacts(p);
    stashFacts(p);
    considerPlane(p, nBuild);
  }
  commitPlanes(nBuild);
  restoreSel();
  lastPlanesAt = millis();
  return true;
}

bool jsonFind(const char *s, const char *lim, const char *key, const char **val) {
  const char *hit = strstr(s, key);
  if (!hit || hit >= lim) return false;
  *val = hit + strlen(key);
  return true;
}

void copyQuoted(const char *v, char *dst, int n) {
  dst[0] = 0;
  if (!v || n < 2) return;
  while (*v == ' ') v++;
  if (*v != '"') return;
  v++;
  int i = 0;
  while (i < n - 1 && v[i] && v[i] != '"') {
    dst[i] = v[i];
    i++;
  }
  dst[i] = 0;
}

bool parsePlanesRaw(char *buf, int n) {
  if (!buf || n < 8) return false;
  if (!strstr(buf, "\"ac\"")) return false;
  int nKeep = snapshotKeep();
  rememberSel();
  int nBuild = 0;
  char *p = buf;
  char *end = buf + n;
  for (;;) {
    char *hex = strstr(p, "\"hex\":");
    if (!hex || hex >= end) break;
    char *next = strstr(hex + 6, "\"hex\":");
    char *lim = (next && next < hex + 2400) ? next : hex + 2400;
    if (lim > end) lim = end;
    const char *v = nullptr;
    if (!jsonFind(hex, lim, "\"lat\":", &v) || *v == '"') {
      p = hex + 6;
      continue;
    }
    float lat = atof(v);
    if (!jsonFind(hex, lim, "\"lon\":", &v)) {
      p = hex + 6;
      continue;
    }
    float lon = atof(v);
    Plane pl;
    memset(&pl, 0, sizeof(pl));
    pl.lat = lat;
    pl.lon = lon;
    if (jsonFind(hex, lim, "\"alt_baro\":", &v)) {
      if (*v == '"') pl.alt = 0;
      else {
        pl.alt = (int)atof(v);
        if (pl.alt > 60000) pl.alt = 0;
      }
    }
    if (jsonFind(hex, lim, "\"gs\":", &v)) pl.gs = (int)atof(v);
    if (jsonFind(hex, lim, "\"track\":", &v)) pl.hdg = (int)atof(v);
    if (jsonFind(hex, lim, "\"baro_rate\":", &v)) pl.vs = (int)atof(v);
    char flight[12] = "", reg[12] = "";
    if (jsonFind(hex, lim, "\"hex\":", &v)) copyQuoted(v, pl.hex, sizeof(pl.hex));
    if (jsonFind(hex, lim, "\"flight\":", &v)) copyQuoted(v, flight, sizeof(flight));
    if (jsonFind(hex, lim, "\"r\":", &v)) copyQuoted(v, reg, sizeof(reg));
    if (jsonFind(hex, lim, "\"t\":", &v)) copyQuoted(v, pl.type, sizeof(pl.type));
    if (jsonFind(hex, lim, "\"squawk\":", &v)) {
      if (*v == '"') pl.squawk = atoi(v + 1);
      else pl.squawk = atoi(v);
    }
    copyCall(pl.call, sizeof(pl.call), flight, reg);
    strlcpy(pl.reg, reg, sizeof(pl.reg));
    applyKeep(pl, keepStore, nKeep);
    applyFactHold(pl);
    fillTypeFacts(pl);
    stashFacts(pl);
    considerPlane(pl, nBuild);
    p = hex + 6;
  }
  commitPlanes(nBuild);
  restoreSel();
  lastPlanesAt = millis();
  return true;
}

bool fetchPlanesUrl(const char *fmt) {
  char url[220];
  snprintf(url, sizeof(url), fmt, homeLat, homeLon, rangeNm);
  if (!httpLock()) return false;
  WiFiClientSecure c;
  c.setInsecure();
  HTTPClient http;
  hfsHttp(http, 12000);
  http.setConnectTimeout(5000);
  http.begin(c, url);
  http.addHeader("Accept", "application/json");
  int code = http.GET();
  if (code != 200) {
    snprintf(planesErr, sizeof(planesErr), "HTTP %d", code);
    http.end();
    c.stop();
    httpUnlock();
    return false;
  }
  const int CAP = 250000;
  char *buf = (char *)psAlloc(CAP);
  if (!buf) {
    strlcpy(planesErr, "NO MEM", sizeof(planesErr));
    http.end();
    c.stop();
    httpUnlock();
    return false;
  }
  int got = 0;
  uint32_t t0 = millis();
  while (got < CAP - 1 && millis() - t0 < 12000UL) {
    int n = http.getStream().available();
    if (n > 0) {
      if (n > 1024) n = 1024;
      if (got + n > CAP - 1) n = CAP - 1 - got;
      int r = http.getStream().readBytes(buf + got, n);
      if (r <= 0) break;
      got += r;
      delay(3);
      continue;
    }
    if (!http.connected()) break;
    delay(2);
  }
  http.end();
  c.stop();
  httpUnlock();
  buf[got] = 0;
  if (got < 8) {
    free(buf);
    strlcpy(planesErr, "EMPTY", sizeof(planesErr));
    return false;
  }
  bool ok = parsePlanesRaw(buf, got);
  free(buf);
  if (!ok) {
    strlcpy(planesErr, "JSON", sizeof(planesErr));
    return false;
  }
  planesErr[0] = 0;
  return true;
}

bool fetchOpensky() {
  float c = cosf(homeLat * 0.0174533f);
  if (c < 0.3f) c = 0.3f;
  float dLat = rangeNm / 60.0f;
  float dLon = rangeNm / (60.0f * c);
  char url[220];
  snprintf(url, sizeof(url),
           "https://opensky-network.org/api/states/all?lamin=%.3f&lomin=%.3f&lamax=%.3f&lomax=%.3f",
           homeLat - dLat, homeLon - dLon, homeLat + dLat, homeLon + dLon);
  if (!httpLock()) return false;
  WiFiClientSecure csec;
  csec.setInsecure();
  HTTPClient http;
  hfsHttp(http, 12000);
  http.begin(csec, url);
  http.addHeader("Accept", "application/json");
  int code = http.GET();
  if (code != 200) {
    snprintf(planesErr, sizeof(planesErr), "HTTP %d", code);
    http.end();
    csec.stop();
    httpUnlock();
    return false;
  }
  const int CAP = 160000;
  char *buf = (char *)psAlloc(CAP);
  if (!buf) {
    strlcpy(planesErr, "NO MEM", sizeof(planesErr));
    http.end();
    csec.stop();
    httpUnlock();
    return false;
  }
  int got = 0;
  uint32_t t0 = millis();
  while (got < CAP - 1 && millis() - t0 < 12000UL) {
    int n = http.getStream().available();
    if (n > 0) {
      if (n > 1024) n = 1024;
      if (got + n > CAP - 1) n = CAP - 1 - got;
      int r = http.getStream().readBytes(buf + got, n);
      if (r <= 0) break;
      got += r;
      delay(3);
      continue;
    }
    if (!http.connected()) break;
    delay(2);
  }
  http.end();
  csec.stop();
  httpUnlock();
  buf[got] = 0;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, buf);
  free(buf);
  if (err || !doc["states"].is<JsonArray>()) {
    strlcpy(planesErr, "JSON", sizeof(planesErr));
    return false;
  }
  int nKeep = snapshotKeep();
  rememberSel();
  int nBuild = 0;
  for (JsonArray s : doc["states"].as<JsonArray>()) {
    if (s[5].isNull() || s[6].isNull()) continue;
    Plane p;
    memset(&p, 0, sizeof(p));
    p.lon = s[5].as<float>();
    p.lat = s[6].as<float>();
    p.alt = s[8].as<bool>() ? 0 : (int)(s[7].as<float>() * 3.28084f);
    p.gs = (int)(s[9].as<float>() * 1.94384f);
    p.hdg = (int)s[10].as<float>();
    p.vs = (int)(s[11].as<float>() * 196.85f);
    strlcpy(p.hex, s[0] | "", sizeof(p.hex));
    copyCall(p.call, sizeof(p.call), s[1] | "", "");
    p.squawk = atoi(s[14] | "0");
    applyKeep(p, keepStore, nKeep);
    applyFactHold(p);
    fillTypeFacts(p);
    stashFacts(p);
    considerPlane(p, nBuild);
  }
  commitPlanes(nBuild);
  restoreSel();
  lastPlanesAt = millis();
  planesErr[0] = 0;
  return true;
}

bool fetchPlanes() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (fetchPlanesUrl("https://opendata.adsb.fi/api/v3/lat/%.4f/lon/%.4f/dist/%d"))
    return true;
  if (strncmp(planesErr, "HTTP 403", 8) == 0 || strncmp(planesErr, "HTTP 429", 8) == 0)
    return fetchOpensky();
  if (fetchPlanesUrl("https://api.adsb.lol/v2/lat/%.4f/lon/%.4f/dist/%d"))
    return true;
  return fetchOpensky();
}

const char *wxWord(int code) {
  if (code >= 199) return "tornado";
  if (code >= 95) return "thunder";
  if ((code >= 71 && code < 80) || (code >= 85 && code < 95)) return "snow";
  if (code >= 51) return "rain";
  if (code >= 45) return "fog";
  if (code >= 3) return "cloudy";
  if (wxKt >= 32) return "windy";
  if (code == 0) return "clear";
  return "partly cloudy";
}

const char *clockWxWord(int code, bool night) {
  const char *w = wxWord(code);
  if (!strcmp(w, "clear") && !night) return "sunny";
  return w;
}

const char *aqiWord(int a) {
  if (a <= 50) return "good";
  if (a <= 100) return "moderate";
  if (a <= 150) return "bad";
  if (a <= 200) return "unhealthy";
  if (a <= 300) return "severe";
  return "hazardous";
}

void copyHm(const char *s, char *dst) {
  const char *t = s ? strchr(s, 'T') : nullptr;
  if (!t || strlen(t) < 6) return;
  dst[0] = t[1];
  dst[1] = t[2];
  dst[2] = ':';
  dst[3] = t[4];
  dst[4] = t[5];
  dst[5] = 0;
}

bool clockNeedsWx() { return !wxOk || !sunUp[0] || sunUp[0] == '-'; }

void fetchMetar() {
  if (!wifiLinked()) return;
  bool filling = clockNeedsWx();
  char url[420];
  char *body = (char *)malloc(8000);
  if (!body) {
    lastWx = millis();
    return;
  }
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,wind_speed_10m,wind_direction_10m,"
           "visibility,surface_pressure,relative_humidity_2m,apparent_temperature"
           "&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&temperature_unit=fahrenheit&wind_speed_unit=kn&timezone=auto",
           homeLat, homeLon);
  if (httpGetPlain(url, body, 8000, 6000)) {
    JsonDocument doc;
    if (!deserializeJson(doc, body) && !doc["current"]["temperature_2m"].isNull()) {
      wxT = (int)(doc["current"]["temperature_2m"].as<float>());
      wxCode = doc["current"]["weather_code"] | 0;
      wxKt = (int)(doc["current"]["wind_speed_10m"].as<float>());
      wxDir = (int)(doc["current"]["wind_direction_10m"].as<float>());
      wxPress = doc["current"]["surface_pressure"].as<float>();
      wxRh = (int)(doc["current"]["relative_humidity_2m"].as<float>());
      wxFeel = (int)(doc["current"]["apparent_temperature"].as<float>());
      wxHi = (int)(doc["daily"]["temperature_2m_max"][0].as<float>());
      wxLo = (int)(doc["daily"]["temperature_2m_min"][0].as<float>());
      wxPop = (int)(doc["daily"]["precipitation_probability_max"][0].as<float>());
      copyHm(doc["daily"]["sunrise"][0] | "", sunUp);
      copyHm(doc["daily"]["sunset"][0] | "", sunDn);
      wxOk = true;
    }
  }
  lastWx = millis();
  if (filling || !wxOk) {
    free(body);
    return;
  }
  snprintf(url, sizeof(url),
           "http://aviationweather.gov/api/data/metar?ids=%s&format=raw", homeIcao);
  if (httpGetPlain(url, body, 8000, 4000)) {
    char *w = strstr(body, "KT");
    if (w && w - body >= 5) {
      char blk[8] = {0};
      strncpy(blk, w - 5, 5);
      wxDir = atoi(blk);
      wxKt = atoi(blk + 3);
      strlcpy(metarRaw, body, sizeof(metarRaw));
    }
  }
  snprintf(url, sizeof(url),
           "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
           "&current=us_aqi",
           homeLat, homeLon);
  if (httpGetPlain(url, body, 8000, 4000)) {
    JsonDocument aq;
    if (!deserializeJson(aq, body)) wxAqi = (int)(aq["current"]["us_aqi"].as<float>());
  }
  free(body);
}

bool liveClock(time_t *out) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  if (out) *out = now;
  return true;
}

bool wxDue() {
  return (uint32_t)(millis() - lastWx) >= 6UL * 3600UL * 1000UL;
}

static float sunUtcHour(float lat, float lng, int yday, bool rise) {
  const float deg = 0.0174532925f;
  float n = (float)(yday + 1);
  float lngHour = lng / 15.0f;
  float t = rise ? n + ((6.0f - lngHour) / 24.0f) : n + ((18.0f - lngHour) / 24.0f);
  float m = (0.9856f * t) - 3.289f;
  float l = m + (1.916f * sinf(m * deg)) + (0.020f * sinf(2.0f * m * deg)) + 282.634f;
  l = fmodf(l, 360.0f);
  if (l < 0) l += 360.0f;
  float ra = atan2f(0.91764f * sinf(l * deg), cosf(l * deg)) / deg;
  ra = fmodf(ra, 360.0f);
  if (ra < 0) ra += 360.0f;
  ra = (ra + (floorf(l / 90.0f) * 90.0f - floorf(ra / 90.0f) * 90.0f)) / 15.0f;
  float sinDec = 0.39782f * sinf(l * deg);
  float cosDec = cosf(asinf(sinDec));
  float cosH = (cosf(90.833f * deg) - sinDec * sinf(lat * deg)) / (cosDec * cosf(lat * deg));
  if (cosH > 1.0f || cosH < -1.0f) return -1.0f;
  float h = rise ? 360.0f - acosf(cosH) / deg : acosf(cosH) / deg;
  h /= 15.0f;
  float ut = h + ra - (0.06571f * t) - 6.622f - lngHour;
  ut = fmodf(ut, 24.0f);
  if (ut < 0) ut += 24.0f;
  return ut;
}

static void putSunLocal(float utcH, char *dst) {
  if (utcH < 0) {
    strlcpy(dst, "--:--", 8);
    return;
  }
  time_t now = time(nullptr);
  struct tm g, loc;
  gmtime_r(&now, &g);
  localtime_r(&now, &loc);
  int day = loc.tm_yday - g.tm_yday;
  if (day > 1) day = -1;
  if (day < -1) day = 1;
  int mins = (int)lroundf(utcH * 60.0f) + (loc.tm_hour - g.tm_hour) * 60 + (loc.tm_min - g.tm_min) + day * 1440;
  mins %= 1440;
  if (mins < 0) mins += 1440;
  snprintf(dst, 8, "%02d:%02d", mins / 60, mins % 60);
}

void fillSunLocal(time_t now) {
  struct tm ti;
  localtime_r(&now, &ti);
  putSunLocal(sunUtcHour(homeLat, homeLon, ti.tm_yday, true), sunUp);
  putSunLocal(sunUtcHour(homeLat, homeLon, ti.tm_yday, false), sunDn);
}

void tickSun() {
  if (!lastTzKeep || (uint32_t)(millis() - lastTzKeep) > 3600000UL || (uint32_t)millis() < 120000UL) {
    if (!lastTzKeep || (uint32_t)(millis() - lastTzKeep) > 5000UL) {
      lastTzKeep = millis();
      applyHomeTz();
    }
  }
  time_t now;
  if (!liveClock(&now)) return;
  struct tm ti;
  localtime_r(&now, &ti);
  bool blank = !sunUp[0] || sunUp[0] == '-';
  bool first = sunDay < 0;
  bool dayChanged = !first && sunDay != ti.tm_yday;
  bool dstChanged = sunDst != -2 && sunDst != ti.tm_isdst;
  if (!first && !dayChanged && !dstChanged) return;
  if (first || dstChanged) applyHomeTz();
  localtime_r(&now, &ti);
  sunDay = ti.tm_yday;
  sunDst = ti.tm_isdst;
  bool wrote = blank || dayChanged || dstChanged;
  if (wrote) fillSunLocal(now);
  if (wifiLinked() && (dayChanged || dstChanged || (first && blank))) askNet(NET_WX);
  if (wrote && page == PG_CLOCK) drawClock();
}

void storeZone(const char *posix) {
  if (!posix || !posix[0]) return;
  if (strcmp(tzSaved, posix)) {
    strlcpy(tzSaved, posix, sizeof(tzSaved));
    tzDirty = true;
  }
  applyHomeTz();
}

void fetchPlaceTz() {
  char url[120];
  snprintf(url, sizeof(url), "https://timeapi.io/api/TimeZone/coordinate?latitude=%.4f&longitude=%.4f",
           homeLat, homeLon);
  String body;
  if (!httpGet(url, body, 8000)) return;
  const char *s = body.c_str();
  char zone[48] = "";
  jsonKey(s, s + body.length(), "\"timeZone\":", zone, sizeof(zone));
  if (!strcmp(zone, "America/Los_Angeles") || !strcmp(zone, "America/Vancouver"))
    storeZone("PST8PDT,M3.2.0,M11.1.0");
  else if (!strcmp(zone, "America/Denver") || !strcmp(zone, "America/Boise") || !strcmp(zone, "America/Edmonton"))
    storeZone("MST7MDT,M3.2.0,M11.1.0");
  else if (!strcmp(zone, "America/Phoenix"))
    storeZone("MST7");
  else if (!strcmp(zone, "America/Chicago") || !strcmp(zone, "America/Winnipeg"))
    storeZone("CST6CDT,M3.2.0,M11.1.0");
  else if (!strcmp(zone, "America/New_York") || !strcmp(zone, "America/Toronto") || !strcmp(zone, "America/Detroit"))
    storeZone("EST5EDT,M3.2.0,M11.1.0");
  else if (!strcmp(zone, "America/Anchorage"))
    storeZone("AKST9AKDT,M3.2.0,M11.1.0");
  else if (!strcmp(zone, "Pacific/Honolulu"))
    storeZone("HST10");
  else if (!strcmp(zone, "America/Puerto_Rico"))
    storeZone("AST4");
  else {
    const char *off = strstr(s, "\"currentUtcOffset\"");
    const char *sec = off ? strstr(off, "\"seconds\":") : nullptr;
    if (!sec) return;
    int seconds = atoi(sec + 10);
    int west = -seconds;
    int sign = west < 0 ? -1 : 1;
    west = abs(west);
    int hh = west / 3600;
    int mm = (west % 3600) / 60;
    char posix[28];
    if (sign < 0)
      snprintf(posix, sizeof(posix), "LCL-%d:%02d", hh, mm);
    else if (mm)
      snprintf(posix, sizeof(posix), "LCL%d:%02d", hh, mm);
    else
      snprintf(posix, sizeof(posix), "LCL%d", hh);
    storeZone(posix);
  }
}

const char *lookWord(int az) {
  static const char *d[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int i = (az + 22) / 45;
  return d[i & 7];
}

void wrap360(double &a) {
  while (a < 0) a += 360;
  while (a >= 360) a -= 360;
}

double unixToJd(time_t t) { return 2440587.5 + (double)t / 86400.0; }

void azelFromRaDec(double raRad, double dec, time_t t, int *az, int *el) {
  double jd = unixToJd(t);
  double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0);
  wrap360(gmst);
  double lst = (gmst + homeLon) * 0.01745329252;
  double ha = lst - raRad;
  double lat = homeLat * 0.01745329252;
  double alt = asin(sin(lat) * sin(dec) + cos(lat) * cos(dec) * cos(ha));
  double azi = atan2(-cos(dec) * sin(ha), sin(dec) * cos(lat) - cos(dec) * sin(lat) * cos(ha));
  *el = (int)(alt * 57.2957795);
  int a = (int)(azi * 57.2957795);
  if (a < 0) a += 360;
  *az = a;
}

void sunAlt(time_t t, float *altOut) {
  double jd = unixToJd(t);
  double T = (jd - 2451545.0) / 36525.0;
  double L = 280.460 + 36000.770 * T;
  double M = 357.528 + 35999.050 * T;
  wrap360(L);
  wrap360(M);
  double Mr = M * 0.01745329252;
  double lam = L + 1.915 * sin(Mr) + 0.020 * sin(2 * Mr);
  double eps = 23.439 - 0.013 * T;
  double lr = lam * 0.01745329252, er = eps * 0.01745329252;
  double ra = atan2(cos(er) * sin(lr), cos(lr));
  double dec = asin(sin(er) * sin(lr));
  int az, el;
  azelFromRaDec(ra, dec, t, &az, &el);
  if (altOut) *altOut = (float)el;
}

void planetRaDec(double L, double er, double *ra, double *dec) {
  double lr = L * 0.01745329252;
  *ra = atan2(cos(er) * sin(lr), cos(lr));
  *dec = asin(sin(er) * sin(lr));
}

void helioLam(double L, double a, double Ls, double *lam) {
  double lr = L * 0.01745329252, sr = Ls * 0.01745329252;
  double x = a * cos(lr) + cos(sr);
  double y = a * sin(lr) + sin(sr);
  double t = atan2(y, x) * 57.2957795;
  if (t < 0) t += 360;
  *lam = t;
}

void bestNightAzEl(double ra, double dec, const time_t *s, int n, int *az, int *el) {
  *el = -99;
  *az = 0;
  for (int i = 0; i < n; i++) {
    int a, e;
    azelFromRaDec(ra, dec, s[i], &a, &e);
    if (e > *el) {
      *el = e;
      *az = a;
    }
  }
}

void skyHours(double ra, double dec, time_t now, char *hrs) {
  char rs[6] = "", ss[6] = "";
  int prev = -99;
  for (int m = 0; m <= 24 * 60; m += 20) {
    int az, el;
    azelFromRaDec(ra, dec, now + m * 60, &az, &el);
    if (prev < 0 && el >= 0 && !rs[0]) {
      struct tm ti;
      time_t t = now + m * 60;
      localtime_r(&t, &ti);
      snprintf(rs, 6, "%02d:%02d", ti.tm_hour, ti.tm_min);
    }
    if (prev >= 0 && el < 0 && !ss[0]) {
      struct tm ti;
      time_t t = now + m * 60;
      localtime_r(&t, &ti);
      snprintf(ss, 6, "%02d:%02d", ti.tm_hour, ti.tm_min);
    }
    prev = el;
  }
  if (rs[0] && ss[0]) snprintf(hrs, 22, "rise %s  set %s", rs, ss);
  else strlcpy(hrs, "tonight", 22);
}

int skyCandCmp(const void *a, const void *b) {
  return ((const SkyCand *)b)->score - ((const SkyCand *)a)->score;
}

void addSky(const char *name, int az, int el, const char *note, const char *hours) {
  if (nSkyVis >= 6) return;
  SkyObj &o = skyVis[nSkyVis++];
  strlcpy(o.name, name, sizeof(o.name));
  strlcpy(o.look, lookWord(az), sizeof(o.look));
  strlcpy(o.note, note, sizeof(o.note));
  strlcpy(o.hours, hours ? hours : "", sizeof(o.hours));
  o.az = az;
  o.el = el;
}

void pushSky(SkyCand *c, int *n, const char *name, const char *note, int az, int el, int score,
             const char *hrs) {
  if (*n >= 16 || el < 8) return;
  SkyCand &o = c[(*n)++];
  strlcpy(o.name, name, sizeof(o.name));
  strlcpy(o.note, note, sizeof(o.note));
  strlcpy(o.hours, hrs, sizeof(o.hours));
  o.az = az;
  o.el = el;
  o.score = score + el / 2;
}

int moonIllum() {
  time_t now;
  if (!liveClock(&now)) return -1;
  double days = ((double)now - 947182440.0) / 86400.0;
  double syn = 29.53058867;
  double phase = days / syn - floor(days / syn);
  int pct = (int)(50.0 * (1.0 - cos(2.0 * 3.14159265 * phase)) + 0.5);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

void computeAstro() {
  nSkyVis = 0;
  time_t now = time(nullptr);
  if (now < 1700000000) return;
  float sunNow = 0;
  sunAlt(now, &sunNow);
  time_t t0 = (sunNow > -6) ? now + 4 * 3600 : now;
  time_t samples[4];
  for (int i = 0; i < 4; i++) samples[i] = t0 + i * 7200;
  double jd = unixToJd(t0);
  double T = (jd - 2451545.0) / 36525.0;
  double er = 23.439 * 0.01745329252;
  double Ls = 280.460 + 36000.770 * T;
  wrap360(Ls);
  SkyCand c[16];
  int nc = 0;
  char hrs[22];
  double Lp = 218.316 + 481267.881 * T;
  double Mp = 134.963 + 477198.868 * T;
  wrap360(Lp);
  wrap360(Mp);
  double mlam = Lp + 6.289 * sin(Mp * 0.01745329252);
  double ra, dec;
  planetRaDec(mlam, er, &ra, &dec);
  int az, el;
  bestNightAzEl(ra, dec, samples, 4, &az, &el);
  double age = fmod(Lp - Ls + 360.0, 360.0);
  const char *phase = "waxing";
  if (age < 20 || age > 340) phase = "new";
  else if (age > 70 && age < 110) phase = "first qtr";
  else if (age > 160 && age < 200) phase = "full moon";
  else if (age > 250 && age < 290) phase = "last qtr";
  else if (age > 200) phase = "waning";
  if (strcmp(phase, "new") && el >= 5) {
    skyHours(ra, dec, now, hrs);
    pushSky(c, &nc, "Moon", phase, az, el, 95, hrs);
  }
  struct P {
    const char *n;
    double L0, Ld, a;
    int score;
  };
  const P ps[] = {{"Venus", 181.98, 1.60213, 0.723, 86},
                  {"Mars", 355.43, 0.52402, 1.524, 72},
                  {"Jupiter", 34.35, 0.08309, 5.203, 91},
                  {"Saturn", 50.08, 0.03346, 9.537, 90}};
  for (int i = 0; i < 4; i++) {
    double L = ps[i].L0 + ps[i].Ld * (jd - 2451545.0);
    wrap360(L);
    double lam;
    helioLam(L, ps[i].a, Ls, &lam);
    double elong = fabs(lam - Ls);
    if (elong > 180) elong = 360 - elong;
    if (elong < 18) continue;
    double raP, decP;
    planetRaDec(lam, er, &raP, &decP);
    bestNightAzEl(raP, decP, samples, 4, &az, &el);
    if (el < 10) continue;
    skyHours(raP, decP, now, hrs);
    pushSky(c, &nc, ps[i].n, "planet", az, el, ps[i].score, hrs);
  }
  struct Dso {
    const char *n;
    const char *note;
    double raH, decD;
    int score, minEl;
  };
  const Dso ds[] = {{"M45", "open cluster", 3.792, 24.12, 76, 15},
                    {"M42", "nebula", 5.588, -5.39, 80, 15},
                    {"Albireo", "double star", 19.512, 27.96, 78, 18},
                    {"M13", "globular", 16.695, 36.46, 73, 20},
                    {"Double", "open cluster", 2.317, 57.13, 74, 20},
                    {"M31", "galaxy", 0.712, 41.27, 71, 18},
                    {"M44", "open cluster", 8.667, 19.67, 64, 20},
                    {"Sirius", "bright star", 6.752, -16.72, 88, 10}};
  for (int i = 0; i < 8; i++) {
    double raD = ds[i].raH * 15.0 * 0.01745329252;
    double decD = ds[i].decD * 0.01745329252;
    bestNightAzEl(raD, decD, samples, 4, &az, &el);
    if (el < ds[i].minEl) continue;
    skyHours(raD, decD, now, hrs);
    pushSky(c, &nc, ds[i].n, ds[i].note, az, el, ds[i].score, hrs);
  }
  if (nc > 1) qsort(c, nc, sizeof(SkyCand), skyCandCmp);
  int keep = nc > 6 ? 6 : nc;
  for (int i = 0; i < keep; i++) addSky(c[i].name, c[i].az, c[i].el, c[i].note, c[i].hours);
  lastAstro = millis();
}

void llToAzEl(float lat, float lon, float altKm, int *az, int *el) {
  float dLat = (lat - homeLat) * 0.0174533f;
  float dLon = (lon - homeLon) * 0.0174533f;
  float rlat = homeLat * 0.0174533f;
  float a = sinf(dLat / 2) * sinf(dLat / 2) +
            cosf(rlat) * cosf(lat * 0.0174533f) * sinf(dLon / 2) * sinf(dLon / 2);
  float c = 2 * atan2f(sqrtf(a), sqrtf(1 - a));
  float R = 6371.0f;
  float h = altKm;
  float elev = atan2f((R + h) * cosf(c) - R, (R + h) * sinf(c));
  *el = (int)lroundf(elev * 57.2958f);
  float y = sinf(dLon) * cosf(lat * 0.0174533f);
  float x = cosf(rlat) * sinf(lat * 0.0174533f) - sinf(rlat) * cosf(lat * 0.0174533f) * cosf(dLon);
  int brg = (int)(atan2f(y, x) * 57.2958f);
  if (brg < 0) brg += 360;
  *az = brg;
}

void fetchIssNow() {
  if (!wifiLinked()) return;
  lastIssTry = millis();
  String body;
  if (!httpGet("https://api.wheretheiss.at/v1/satellites/25544", body, 6000)) return;
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  issLat = doc["latitude"].as<float>();
  issLon = doc["longitude"].as<float>();
  issAltKm = doc["altitude"].as<float>();
  lastIssPos = millis();
  issOk = true;
  llToAzEl(issLat, issLon, issAltKm, &issAz, &issEl);
  strlcpy(issLook, lookWord(issAz), sizeof(issLook));
  time_t now;
  float sunA = 0;
  if (liveClock(&now)) sunAlt(now, &sunA);
  issVisibleNow = (issEl >= 15 && sunA < -4);
  if (issTle.ok) {
    buildIssOrbitPath();
    alignIssPathToLive();
  }
}

time_t utcYmd(int y, int mo, int d, int h, int mi, int se) {
  if (mo <= 2) {
    y--;
    mo += 12;
  }
  int A = y / 100;
  int B = 2 - A + A / 4;
  double jd = floor(365.25 * (y + 4716)) + floor(30.6001 * (mo + 1)) + d + B - 1524.5 +
              (h + mi / 60.0 + se / 3600.0) / 24.0;
  return (time_t)((jd - 2440587.5) * 86400.0);
}

bool issPosAt(time_t t, float *lat, float *lon, float *altKm) {
  if (!issTle.ok) return false;
  double dt = unixToJd(t) - issTle.epochJd;
  double n = issTle.n * 2.0 * M_PI;
  double M = issTle.m0 * 0.01745329252 + n * dt;
  double e = issTle.e;
  double E = M;
  for (int i = 0; i < 7; i++) E = M + e * sin(E);
  double nu = 2 * atan2(sqrt(1 + e) * sin(E / 2), sqrt(1 - e) * cos(E / 2));
  double ns = issTle.n * 2.0 * M_PI / 86400.0;
  double a = pow(398600.4418 / (ns * ns), 1.0 / 3.0);
  double r = a * (1 - e * cos(E));
  double inc = issTle.inc * 0.01745329252;
  double j2 = 1.08263e-3, re = 6378.137;
  double cosi = cos(inc);
  double raanDot = -1.5 * ns * j2 * (re / a) * (re / a) * cosi / pow(1 - e * e, 2);
  double raan = issTle.raan * 0.01745329252 + raanDot * dt * 86400.0;
  double arg = issTle.argp * 0.01745329252 + nu;
  double x = r * cos(arg), y = r * sin(arg);
  double ecix = x * cos(raan) - y * cosi * sin(raan);
  double eciy = x * sin(raan) + y * cosi * cos(raan);
  double eciz = y * sin(inc);
  double gmst = 280.46061837 + 360.98564736629 * (unixToJd(t) - 2451545.0);
  wrap360(gmst);
  double g = gmst * 0.01745329252;
  double ex = ecix * cos(g) + eciy * sin(g);
  double ey = -ecix * sin(g) + eciy * cos(g);
  *lon = (float)(atan2(ey, ex) * 57.2957795);
  *lat = (float)(atan2(eciz, sqrt(ex * ex + ey * ey)) * 57.2957795);
  *altKm = (float)(sqrt(ex * ex + ey * ey + eciz * eciz) - 6371.0);
  return true;
}

void buildIssOrbitPath() {
  nIssTrack = 0;
  if (!issTle.ok) return;
  time_t now;
  if (!liveClock(&now)) return;
  double period = 86400.0 / issTle.n;
  for (int i = 0; i < 72; i++) {
    float la, lo, al;
    time_t t = now + (time_t)(period * i / 72.0);
    if (!issPosAt(t, &la, &lo, &al)) break;
    issTrackLat[nIssTrack] = la;
    issTrackLon[nIssTrack] = lo;
    nIssTrack++;
  }
  issPathAt = millis();
}

void alignIssPathToLive() {
  if (!nIssTrack) return;
  float dlat = issLat - issTrackLat[0];
  float dlon = issLon - issTrackLon[0];
  while (dlon > 180) dlon -= 360;
  while (dlon < -180) dlon += 360;
  for (int i = 0; i < nIssTrack; i++) {
    issTrackLat[i] += dlat;
    issTrackLon[i] += dlon;
    if (issTrackLon[i] > 180) issTrackLon[i] -= 360;
    if (issTrackLon[i] < -180) issTrackLon[i] += 360;
  }
}

static const int ISS_MIN_EL = 10;
static const int ISS_SEARCH_MIN = 10 * 24 * 60;

bool issViewAt(time_t t, int *az, int *el) {
  float la, lo, al;
  if (!issPosAt(t, &la, &lo, &al)) return false;
  llToAzEl(la, lo, al, az, el);
  float sunA = 0;
  sunAlt(t, &sunA);
  return *el >= ISS_MIN_EL && sunA < -4;
}

void fmtIssDur(int sec, char *out, int n) {
  if (sec < 0) sec = 0;
  int m = sec / 60;
  int s = sec % 60;
  if (m && s)
    snprintf(out, n, "%d min %d sec", m, s);
  else if (m)
    snprintf(out, n, "%d min", m);
  else
    snprintf(out, n, "%d sec", s);
}

void recordIssPass(time_t start, time_t end, const char *from) {
  if (nIssPass >= 5) return;
  int dur = (int)(end - start);
  if (dur < 30) return;
  struct tm ti;
  localtime_r(&start, &ti);
  char durS[18];
  fmtIssDur(dur, durS, sizeof(durS));
  IssPass &P = issPasses[nIssPass++];
  P.when = start;
  P.durSec = dur;
  strlcpy(P.from, from, sizeof(P.from));
  snprintf(P.line, sizeof(P.line), "%02d/%02d %02d:%02d  from %s  %s", ti.tm_mon + 1,
           ti.tm_mday, ti.tm_hour, ti.tm_min, P.from, durS);
}

void closeIssPass(time_t start, time_t lastVis, const char *from) {
  int az = 0, el = 0;
  time_t t0 = start;
  char rise[4];
  strlcpy(rise, from, sizeof(rise));
  while (t0 > start - 480) {
    if (!issViewAt(t0 - 10, &az, &el)) break;
    t0 -= 10;
    strlcpy(rise, lookWord(az), sizeof(rise));
  }
  time_t t1 = lastVis;
  while (issViewAt(t1 + 10, &az, &el)) t1 += 10;
  recordIssPass(t0, t1, rise);
}

void buildIssTrackAndPasses() {
  nIssPass = 0;
  issVisibleNow = false;
  issNextPass = 0;
  time_t now;
  if (!liveClock(&now)) return;
  if (!issTle.ok) return;
  if (!issPathAt || millis() - issPathAt > 900000UL) buildIssOrbitPath();
  float la, lo, al;
  bool haveLive = lastIssPos && (millis() - lastIssPos < 120000UL);
  if (haveLive) {
    llToAzEl(issLat, issLon, issAltKm, &issAz, &issEl);
  } else if (issPosAt(now, &la, &lo, &al)) {
    issLat = la;
    issLon = lo;
    issAltKm = al;
    llToAzEl(la, lo, al, &issAz, &issEl);
  }
  strlcpy(issLook, lookWord(issAz), sizeof(issLook));
  float sunA = 0;
  sunAlt(now, &sunA);
  issVisibleNow = issEl >= ISS_MIN_EL && sunA < -4;
  bool inPass = false;
  time_t start = 0, lastVis = 0;
  char riseD[4] = "";
  for (int s = 0; s < ISS_SEARCH_MIN && nIssPass < 5; s += 1) {
    if ((s & 127) == 0) vTaskDelay(pdMS_TO_TICKS(1));
    time_t t = now + s * 60;
    int az = 0, el = 0;
    bool vis = issViewAt(t, &az, &el);
    if (vis && !inPass) {
      inPass = true;
      start = t;
      lastVis = t;
      strlcpy(riseD, lookWord(az), sizeof(riseD));
    } else if (vis && inPass) {
      lastVis = t;
    } else if (!vis && inPass) {
      closeIssPass(start, lastVis, riseD);
      inPass = false;
      s += 8;
    }
  }
  if (inPass) closeIssPass(start, lastVis, riseD);
  if (nIssPass) issNextPass = issPasses[0].when;
  lastPassAt = millis();
}

void fetchIssTle() {
  if (!wifiLinked()) return;
  String body;
  if (!httpGet("https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=JSON", body, 8000))
    return;
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  JsonObject o = doc[0];
  if (o.isNull()) return;
  const char *ep = o["EPOCH"] | "";
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  float se = 0;
  if (sscanf(ep, "%d-%d-%dT%d:%d:%f", &y, &mo, &d, &h, &mi, &se) < 5) return;
  double epochJd = unixToJd(utcYmd(y, mo, d, h, mi, (int)se));
  if (o["MEAN_MOTION"].isNull() || o["INCLINATION"].isNull() ||
      o["RA_OF_ASC_NODE"].isNull() || o["ARG_OF_PERICENTER"].isNull() ||
      o["MEAN_ANOMALY"].isNull() || o["ECCENTRICITY"].isNull())
    return;
  double n = o["MEAN_MOTION"].as<double>();
  if (n < 1) return;
  issTle.epochJd = epochJd;
  issTle.n = n;
  issTle.e = o["ECCENTRICITY"].as<double>();
  issTle.inc = o["INCLINATION"].as<double>();
  issTle.raan = o["RA_OF_ASC_NODE"].as<double>();
  issTle.argp = o["ARG_OF_PERICENTER"].as<double>();
  issTle.m0 = o["MEAN_ANOMALY"].as<double>();
  issTle.ok = true;
  lastTle = millis();
  lastPassAt = 0;
}

static bool hasIgnoreCase(const char *hay, const char *ndl) {
  if (!hay || !ndl || !ndl[0]) return false;
  const size_t n = strlen(ndl);
  for (const char *p = hay; *p; p++) {
    size_t i = 0;
    while (i < n && p[i] && (tolower((unsigned char)p[i]) == tolower((unsigned char)ndl[i]))) i++;
    if (i == n) return true;
  }
  return false;
}

void launchSite(const char *loc, const char *pad, char *out, int n) {
  char blob[140];
  snprintf(blob, sizeof(blob), "%s %s", loc ? loc : "", pad ? pad : "");
  struct {
    const char *key;
    const char *name;
  } sites[] = {{"vandenberg", "Vandenberg"},
               {"cape canaveral", "Cape Canaveral"},
               {"canaveral", "Cape Canaveral"},
               {"kennedy", "Kennedy"},
               {"starbase", "Starbase"},
               {"boca chica", "Starbase"},
               {"wallops", "Wallops"},
               {"jiuquan", "Jiuquan"},
               {"taiyuan", "Taiyuan"},
               {"xichang", "Xichang"},
               {"wenchang", "Wenchang"},
               {"baikonur", "Baikonur"},
               {"plesetsk", "Plesetsk"},
               {"vostochny", "Vostochny"},
               {"kourou", "Kourou"},
               {"guiana", "Kourou"},
               {"mahia", "Mahia"},
               {"kodiak", "Kodiak"},
               {"pacific spaceport", "Kodiak"},
               {"tanegashima", "Tanegashima"},
               {"uchinoura", "Uchinoura"},
               {"sriharikota", "Sriharikota"},
               {"satish dhawan", "Sriharikota"},
               {"semnan", "Semnan"},
               {"alcantara", "Alcantara"},
               {"palmachim", "Palmachim"},
               {nullptr, nullptr}};
  for (int i = 0; sites[i].key; i++) {
    if (hasIgnoreCase(blob, sites[i].key)) {
      strlcpy(out, sites[i].name, n);
      return;
    }
  }
  const char *s = (loc && loc[0]) ? loc : pad;
  if (!s || !s[0]) {
    out[0] = 0;
    return;
  }
  int i = 0;
  while (s[i] && s[i] != ',' && i < n - 1) {
    out[i] = s[i];
    i++;
  }
  out[i] = 0;
}

bool isUsaLaunch(const char *loc, const char *pad) {
  char blob[140];
  snprintf(blob, sizeof(blob), "%s %s", loc ? loc : "", pad ? pad : "");
  return hasIgnoreCase(blob, "united states") || hasIgnoreCase(blob, "usa") ||
         hasIgnoreCase(blob, "vandenberg") || hasIgnoreCase(blob, "canaveral") ||
         hasIgnoreCase(blob, "kennedy") || hasIgnoreCase(blob, "starbase") ||
         hasIgnoreCase(blob, "boca chica") || hasIgnoreCase(blob, "wallops") ||
         hasIgnoreCase(blob, "kodiak") || hasIgnoreCase(blob, "pacific spaceport") ||
         hasIgnoreCase(blob, "spaceport america") || hasIgnoreCase(blob, "midland") ||
         hasIgnoreCase(blob, "brownsville") || hasIgnoreCase(blob, "virginia") ||
         hasIgnoreCase(blob, "florida") || hasIgnoreCase(blob, "california") ||
         hasIgnoreCase(blob, "texas") || hasIgnoreCase(blob, "alaska");
}

void jsonKey(const char *s, const char *lim, const char *key, char *out, int n) {
  out[0] = 0;
  if (!s || !lim || s >= lim) return;
  const char *h = strstr(s, key);
  if (!h || h >= lim) return;
  h += strlen(key);
  while (h < lim && *h && *h != '"') h++;
  if (h >= lim || *h != '"') return;
  h++;
  int i = 0;
  while (h < lim && *h && *h != '"' && i < n - 1) out[i++] = *h++;
  out[i] = 0;
}

void fillLaunchStr(Launch &L, time_t t, bool today, const char *name, const char *locn,
                   const char *pad, const char *lsp) {
  struct tm loc;
  localtime_r(&t, &loc);
  if (today)
    strlcpy(L.day, "TODAY", sizeof(L.day));
  else {
    static const char *mo[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                               "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    snprintf(L.day, sizeof(L.day), "%d %s", loc.tm_mday, mo[loc.tm_mon]);
  }
  snprintf(L.when, sizeof(L.when), "%02d:%02d", loc.tm_hour, loc.tm_min);
  const char *bar = name ? strstr(name, " | ") : nullptr;
  if (bar) {
    int n = (int)(bar - name);
    if (n > 17) n = 17;
    strncpy(L.rocket, name, n);
    L.rocket[n] = 0;
    strlcpy(L.payload, bar + 3, sizeof(L.payload));
  } else
    strlcpy(L.rocket, name ? name : "", sizeof(L.rocket));
  launchSite(locn ? locn : "", pad ? pad : "", L.pad, sizeof(L.pad));
  strlcpy(L.agency, lsp ? lsp : "", sizeof(L.agency));
}

void fetchLaunches() {
  lastLaunchTry = millis();
  if (!wifiLinked()) return;
  time_t now;
  if (!liveClock(&now)) return;
  struct tm today;
  localtime_r(&now, &today);
  const int CAP = 64000;
  char *buf = (char *)psAlloc(CAP);
  if (!buf) return;
  if (!httpGetSmall("https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=20&mode=list", buf, CAP,
                    8000)) {
    free(buf);
    return;
  }
  nLaunch = 0;
  nUsaNext = 0;
  char *p = buf;
  while (*p && (nLaunch < 6 || nUsaNext < 4)) {
    char *netp = strstr(p, "\"net\":");
    if (!netp) break;
    char *next = strstr(netp + 6, "\"net\":");
    char *lim = next ? next : netp + 2000;
    char net[32] = "", name[64] = "", locn[64] = "", pad[48] = "", lsp[32] = "";
    jsonKey(netp, lim, "\"net\":", net, sizeof(net));
    jsonKey(netp, lim, "\"name\":", name, sizeof(name));
    jsonKey(netp, lim, "\"location\":", locn, sizeof(locn));
    if (!locn[0] || !strcmp(locn, "id") || !strcmp(locn, "url") || !strcmp(locn, "name")) {
      const char *lh = strstr(netp, "\"location\"");
      if (lh && lh < lim) jsonKey(lh, lim, "\"name\":", locn, sizeof(locn));
    }
    jsonKey(netp, lim, "\"pad\":", pad, sizeof(pad));
    if (!pad[0] || !strcmp(pad, "id") || !strcmp(pad, "name")) {
      const char *ph = strstr(netp, "\"pad\"");
      if (ph && ph < lim) jsonKey(ph, lim, "\"name\":", pad, sizeof(pad));
    }
    jsonKey(netp, lim, "\"lsp_name\":", lsp, sizeof(lsp));
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    float sec = 0;
    if (sscanf(net, "%d-%d-%dT%d:%d:%f", &y, &mo, &d, &h, &mi, &sec) >= 5) {
      time_t t = utcYmd(y, mo, d, h, mi, (int)sec);
      if (t >= now - 3600) {
        struct tm loc;
        localtime_r(&t, &loc);
        int dayDiff = (loc.tm_yday + loc.tm_year * 366) - (today.tm_yday + today.tm_year * 366);
        if (dayDiff <= 0 && nLaunch < 6) {
          fillLaunchStr(launches[nLaunch], t, true, name, locn, pad, lsp);
          nLaunch++;
        }
        if (nUsaNext < 4) {
          fillLaunchStr(usaNext[nUsaNext], t, dayDiff <= 0, name, locn, pad, lsp);
          nUsaNext++;
        }
      }
    }
    p = netp + 6;
  }
  free(buf);
  lastLaunch = millis();
  launchOk = true;
}

void drawHomeMenu() {
  const char *lab[] = {"WX", "LIST", "FIELD", "SKY", "SETUP"};
  int bw = PW / 5;
  for (int i = 0; i < 5; i++) {
    int x = i * bw;
    gfx->fillRect(x, MENU_Y, bw, PH - MENU_Y, COL_DIM);
    gfx->drawRect(x, MENU_Y, bw, PH - MENU_Y, COL_MID);
    gfx->setTextColor(COL_TXT, COL_DIM);
    gfx->setTextSize(1);
    centre(lab[i], x + bw / 2, MENU_Y + 16);
  }
}

void drawZoomBar() {
  gfx->fillRect(0, ZOOM_Y, PW, MENU_Y - ZOOM_Y, COL_BG);
  gfx->fillRoundRect(10, ZOOM_Y + 5, 96, 18, 3, COL_DIM);
  gfx->fillRoundRect(PW - 106, ZOOM_Y + 5, 96, 18, 3, COL_DIM);
  gfx->setTextColor(COL_TXT, COL_DIM);
  gfx->setTextSize(1);
  centre("ZOOM -", 58, ZOOM_Y + 9);
  centre("ZOOM +", PW - 58, ZOOM_Y + 9);
  gfx->setTextColor(COL_YEL, COL_BG);
  char t[28];
  snprintf(t, sizeof(t), "ZOOM  %dNM", rangeNm);
  centre(t, CX, ZOOM_Y + 9);
}

uint16_t mapPix(uint16_t c) {
  if (dayMode || mapStyle) return c;
  int r = ((c >> 11) & 31) * 19 / 24;
  int g = ((c >> 5) & 63) * 19 / 24;
  int b = (c & 31) * 19 / 24;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void bakeMapLook() {
  if (!mapFb || dayMode || mapStyle) return;
  for (int i = 0; i < PW * MAP_H; i++) mapFb[i] = mapPix(mapFb[i]);
}

static int mapSX = 256, mapSY = 256;
static int mapScaleKey = -999999;
static void prepMapScale() {
  int rr = rangeNm > 0 ? rangeNm : 1;
  int key = mapNm * 500 + rr;
  if (key == mapScaleKey) return;
  mapScaleKey = key;
  if (mapNm <= 0) {
    mapSX = 256;
    mapSY = 256;
    return;
  }
  int span = (mapNm * 3 + 1) / 2;
  int s = (int)((256L * rr + span / 2) / span);
  if (s < 1) s = 1;
  mapSX = s;
  mapSY = s;
}
static int mapSrcCoord(int v, int origin, int scale, int limit) {
  int d = v - origin;
  int s = origin + (int)(((long)d * scale + (d < 0 ? -128 : 128)) / 256);
  if (s < 0) s = 0;
  if (s >= limit) s = limit - 1;
  return s;
}

void blitMap() {
  if (!mapOn || !mapOk || !mapFb) {
    gfx->fillRect(0, 0, PW, MAP_H, COL_BG);
    return;
  }
  prepMapScale();
  if (mapSX == 256 && mapSY == 256) {
    gfx->draw16bitRGBBitmap(0, 0, mapFb, PW, MAP_H);
    return;
  }
  uint16_t row[PW];
  for (int y = 0; y < MAP_H; y++) {
    int sy = mapSrcCoord(y, CY, mapSY, MAP_H);
    for (int x = 0; x < PW; x++) row[x] = mapFb[sy * PW + mapSrcCoord(x, CX, mapSX, PW)];
    gfx->draw16bitRGBBitmap(0, y, row, PW, 1);
  }
}

void restoreRect(int x, int y, int w, int h) {
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > PW) w = PW - x;
  if (y + h > MAP_H) h = MAP_H - y;
  if (w <= 0 || h <= 0) return;
  if (!mapOn || !mapOk || !mapFb) {
    gfx->fillRect(x, y, w, h, COL_BG);
    return;
  }
  prepMapScale();
  if (mapSX == 256 && mapSY == 256) {
    for (int row = 0; row < h; row++)
      gfx->draw16bitRGBBitmap(x, y + row, &mapFb[(y + row) * PW + x], w, 1);
    return;
  }
  uint16_t line[PW];
  for (int yy = 0; yy < h; yy++) {
    int sy = mapSrcCoord(y + yy, CY, mapSY, MAP_H);
    for (int xx = 0; xx < w; xx++)
      line[xx] = mapFb[sy * PW + mapSrcCoord(x + xx, CX, mapSX, PW)];
    gfx->draw16bitRGBBitmap(x, y + yy, line, w, 1);
  }
}

void erasePlaneMarks() {
  for (int i = 0; i < nMarks; i++) {
    restoreRect(marks[i].x - 12, marks[i].y - 14, 26, 28);
    restoreRect(marks[i].bx - 1, marks[i].by - 1, marks[i].bw + 1, marks[i].bh + 1);
  }
  nMarks = 0;
  for (int i = 0; i < nShips; i++) {
    if (ships[i].w > 0) restoreRect(ships[i].x, ships[i].y, ships[i].w, ships[i].h);
  }
  restoreRect(4, 2, 137, 29);
  restoreRect(438, 2, 42, 28);
}

void ringInk(uint16_t &ring, uint16_t &outer) {
  if (dayMode) {
    ring = RGB565(36, 38, 42);
    outer = RGB565(210, 170, 28);
  } else {
    ring = RGB565(210, 216, 222);
    outer = RGB565(255, 214, 56);
  }
}

void drawRings() {
  uint16_t ring, outer;
  ringInk(ring, outer);
  for (int r = rangeNm / 3; r <= rangeNm; r += max(1, rangeNm / 3))
    gfx->drawCircle(CX, CY, (int)(r * nmPerPx()), ring);
  gfx->drawCircle(CX, CY, RR, outer);
  gfx->drawLine(CX, 5, CX, MAP_H - 5, ring);
  gfx->drawLine(5, CY, PW - 5, CY, ring);
}

void patchRings(float deg) {
  uint16_t ring, outer;
  ringInk(ring, outer);
  for (int d = -10; d <= 10; d++) {
    float a = (deg + d) * 0.0174533f;
    int x = CX + (int)(sinf(a) * RR);
    int y = CY - (int)(cosf(a) * RR);
    if ((unsigned)x < (unsigned)PW && (unsigned)y < (unsigned)MAP_H) gfx->drawPixel(x, y, outer);
    for (int r = rangeNm / 3; r <= rangeNm; r += max(1, rangeNm / 3)) {
      int rr = (int)(r * nmPerPx());
      int ix = CX + (int)(sinf(a) * rr);
      int iy = CY - (int)(cosf(a) * rr);
      if ((unsigned)ix < (unsigned)PW && (unsigned)iy < (unsigned)MAP_H) gfx->drawPixel(ix, iy, ring);
    }
  }
  gfx->drawLine(CX, 5, CX, MAP_H - 5, ring);
  gfx->drawLine(5, CY, PW - 5, CY, ring);
}

void drawPlaneDots() {
  nMarks = 0;
  for (int i = 0; i < nPlanes; i++) {
    float x, y;
    llToXY(planes[i].lat, planes[i].lon, x, y);
    if (x < 5 || x > PW - 5 || y < 5 || y > MAP_H - 5) continue;
    int ix = (int)x, iy = (int)y;
    uint16_t pc = planeColor(planes[i].alt);
    if (showMarks) {
      if (i == selected) gfx->drawCircle(ix, iy, 8, COL_MAG);
      planeArrow(ix, iy, planes[i].hdg, pc);
      if (nMarks < HFS_MAXP) {
        marks[nMarks].idx = i;
        marks[nMarks].x = ix;
        marks[nMarks].y = iy;
        marks[nMarks].bx = ix - 10;
        marks[nMarks].by = iy - 12;
        marks[nMarks].bw = 20;
        marks[nMarks].bh = 24;
        nMarks++;
      }
    } else {
      if (i == selected) gfx->drawCircle(ix, iy, 6, COL_MAG);
      gfx->fillCircle(ix, iy, 3, pc);
      float h = planes[i].hdg * 0.0174533f;
      gfx->drawLine(ix, iy, ix + (int)(sinf(h) * 8), iy - (int)(cosf(h) * 8), pc);
      planeTag(i, ix, iy, planes[i].call, pc);
    }
  }
  gfx->fillCircle(CX, CY, 3, COL_MAG);
}

void drawHomeClock() {
  time_t now;
  if (!liveClock(&now)) return;
  struct tm ti;
  clockLocal(now, &ti);
  static const char *mo[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                             "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  int hr = ti.tm_hour % 12;
  if (!hr) hr = 12;
  char line[28];
  snprintf(line, sizeof(line), "%d:%02d %s  %s %d", hr, ti.tm_min,
           ti.tm_hour >= 12 ? "PM" : "AM", mo[ti.tm_mon], ti.tm_mday);
  gfx->fillRoundRect(289, 3, 144, 18, 2, COL_BLK);
  gfx->setTextColor(COL_YEL, COL_BLK);
  gfx->setTextSize(1);
  centre(line, 361, 7);
}

void drawLiveHud() {
  drawWifiChip();
  char live[28];
  if (lastPlanesAt) {
    uint32_t ag = (millis() - lastPlanesAt) / 1000UL;
    if (nPlanes)
      snprintf(live, sizeof(live), "%s %lus %d", HFS_VER, (unsigned long)ag, nPlanes);
    else
      snprintf(live, sizeof(live), "LIVE %lus  none", (unsigned long)ag);
  } else if (wifiLinked())
    snprintf(live, sizeof(live), "%s", (mapOn && !mapOk) ? "MAP ..." : (planesErr[0] ? planesErr : "TRAFFIC ..."));
  else
    snprintf(live, sizeof(live), "NO WIFI");
  gfx->fillRoundRect(5, 3, 132, 18, 2, COL_BLK);
  gfx->setTextColor(COL_YEL, COL_BLK);
  gfx->setTextSize(1);
  gfx->setCursor(10, 7);
  gfx->print(live);
  if (!nPlanes && lastPlanesAt) {
    gfx->setTextColor(COL_YEL);
    gfx->setTextSize(1);
    gfx->setCursor(7, 24);
    gfx->print("none in range");
  }
  int known = findHomeField(homeIcao);
  gfx->setTextColor(COL_YEL);
  gfx->setTextSize(1);
  centre(known >= 0 ? homeFields[known].city : homeIcao, CX, CY + 10);
  drawHomeClock();
}

static int shipCell(float v, float cell) {
  float q = v / cell;
  int n = (int)q;
  if (q < 0.0f && (float)n != q) n -= 1;
  return n;
}

static void keepNearestShip(const Ship &s) {
  float dLat = (s.lat - homeLat) * 60.0f;
  float c = cosf(homeLat * 0.0174533f);
  if (c < 0.25f) c = 0.25f;
  float dLon = (s.lon - homeLon) * 60.0f * c;
  float d = sqrtf(dLat * dLat + dLon * dLon);
  if (d > (float)rangeNm + 2.0f) return;
  float cell = (float)rangeNm / 3.0f;
  if (cell < 2.0f) cell = 2.0f;
  int gx = shipCell(dLon, cell);
  int gy = shipCell(dLat, cell);
  for (int i = 0; i < nShips; i++) {
    float dy = (ships[i].lat - homeLat) * 60.0f;
    float dx = (ships[i].lon - homeLon) * 60.0f * c;
    if (shipCell(dx, cell) == gx && shipCell(dy, cell) == gy) {
      if (s.sog > ships[i].sog) ships[i] = s;
      return;
    }
  }
  if (nShips < HFS_MAXSHIP) ships[nShips++] = s;
}

static bool shipNum(const char *p, float &out, const char *&next) {
  char tmp[24];
  int n = 0;
  if (*p == '-') tmp[n++] = *p++;
  int digits = 0;
  while ((*p >= '0' && *p <= '9') || *p == '.') {
    if (n < 22) tmp[n++] = *p;
    if (*p >= '0' && *p <= '9') digits++;
    p++;
  }
  if (!digits) return false;
  tmp[n] = 0;
  out = atof(tmp);
  next = p;
  return true;
}

void fetchShips() {
  if (!shipsOn || WiFi.status() != WL_CONNECTED) return;
  float dLat = (float)rangeNm / 60.0f;
  float cosl = cosf(homeLat * 0.0174533f);
  if (cosl < 0.25f) cosl = 0.25f;
  float dLon = (float)rangeNm / (60.0f * cosl);
  float area = (2.0f * dLat) * (2.0f * dLon);
  if (area > 90.0f) {
    float s = sqrtf(90.0f / area);
    dLat *= s;
    dLon *= s;
  }
  char url[180];
  snprintf(url, sizeof(url), "https://ais.openwaters.io/v1/vessels?bbox=%.3f,%.3f,%.3f,%.3f",
           homeLat - dLat, homeLon - dLon, homeLat + dLat, homeLon + dLon);
  if (!httpLock()) return;
  WiFiClientSecure c;
  c.setInsecure();
  HTTPClient http;
  hfsHttp(http, 15000);
  http.useHTTP10(true);
  http.setUserAgent("Mega43/1.0 (github.com/stefi01/mega-4-3-inch-waveshare-plane-tracker)");
  bool ok = false;
  const int CAP = 200000;
  char *buf = nullptr;
  int got = 0;
  if (http.begin(c, url)) {
    http.addHeader("Accept", "application/json");
    int code = http.GET();
    if (code == 200) {
      buf = (char *)psAlloc(CAP);
      if (buf) {
        WiFiClient *s = http.getStreamPtr();
        uint32_t t0 = millis();
        uint32_t idle = t0;
        while (s && got < CAP - 1 && millis() - t0 < 15000) {
          int n = s->available();
          if (n > 0) {
            idle = millis();
            if (n > 1024) n = 1024;
            if (got + n > CAP - 1) n = CAP - 1 - got;
            int r = s->readBytes(buf + got, n);
            if (r <= 0) break;
            got += r;
            delay(3);
            continue;
          }
          if (!s->connected() && millis() - idle > 400) break;
          delay(2);
        }
        buf[got] = 0;
        ok = got > 20;
      }
    }
    http.end();
  }
  c.stop();
  httpUnlock();
  if (!ok || !buf) {
    if (buf) free(buf);
    lastShips = millis() - 45000;
    return;
  }
  nShips = 0;
  const char *p = buf;
  while ((p = strstr(p, "\"coordinates\":[")) != nullptr) {
    const char *br = strchr(p, '[');
    float lon = 0, lat = 0;
    const char *after = nullptr;
    if (br && shipNum(br + 1, lon, after) && after && *after == ',' &&
        shipNum(after + 1, lat, after) && lat > -80.0f && lat < 80.0f) {
      Ship s;
      memset(&s, 0, sizeof(s));
      s.lat = lat;
      s.lon = lon;
      const char *np = strstr(p, "\"name\":");
      if (np && np < p + 700) {
        np = strchr(np + 7, '"');
        if (np) copyQuoted(np, s.name, sizeof(s.name));
      }
      const char *sp = strstr(p, "\"sog\":");
      if (sp && sp < p + 700) s.sog = (int)atof(sp + 6);
      if (!s.name[0]) strlcpy(s.name, "ship", sizeof(s.name));
      keepNearestShip(s);
    }
    p += 16;
  }
  free(buf);
  lastShips = nShips ? millis() : millis() - 45000;
}

void rankAirports(float lat, float lon) {
  nZipHits = 0;
  for (int i = 0; homeFields[i].icao; i++) {
    float d = nmBetween(lat, lon, homeFields[i].lat, homeFields[i].lon);
    if (nZipHits == 4) {
      float worst = nmBetween(lat, lon, zipHits[3].lat, zipHits[3].lon);
      if (d >= worst) continue;
    }
    int pos = (nZipHits < 4) ? nZipHits : 3;
    if (nZipHits < 4) nZipHits++;
    while (pos > 0 && d < nmBetween(lat, lon, zipHits[pos - 1].lat, zipHits[pos - 1].lon)) {
      zipHits[pos] = zipHits[pos - 1];
      pos--;
    }
    strlcpy(zipHits[pos].icao, homeFields[i].icao, sizeof(zipHits[pos].icao));
    strlcpy(zipHits[pos].city, homeFields[i].city, sizeof(zipHits[pos].city));
    zipHits[pos].lat = homeFields[i].lat;
    zipHits[pos].lon = homeFields[i].lon;
  }
}

void fetchZip() {
  char z[8];
  int n = 0;
  for (int i = 0; zipCode[i] && n < 5; i++) {
    if (isdigit((unsigned char)zipCode[i])) z[n++] = zipCode[i];
  }
  z[n] = 0;
  if (n < 5) {
    strlcpy(zipMsg, "zip needs 5 digits", sizeof(zipMsg));
    nZipHits = 0;
    return;
  }
  char url[64];
  snprintf(url, sizeof(url), "https://api.zippopotam.us/us/%s", z);
  String body;
  if (!httpGet(url, body, 8000)) {
    strlcpy(zipMsg, "zip lookup failed", sizeof(zipMsg));
    nZipHits = 0;
    return;
  }
  const char *s = body.c_str();
  const char *la = strstr(s, "\"latitude\":");
  const char *lo = strstr(s, "\"longitude\":");
  if (!la || !lo) {
    strlcpy(zipMsg, "zip not found", sizeof(zipMsg));
    nZipHits = 0;
    return;
  }
  float lat = 0, lon = 0;
  sscanf(la, "\"latitude\": \"%f\"", &lat);
  sscanf(lo, "\"longitude\": \"%f\"", &lon);
  if (lat == 0 && lon == 0) {
    strlcpy(zipMsg, "zip not found", sizeof(zipMsg));
    return;
  }
  rankAirports(lat, lon);
  if (nZipHits)
    strlcpy(zipMsg, "tap an airport", sizeof(zipMsg));
  else
    strlcpy(zipMsg, "no saved airport near that zip", sizeof(zipMsg));
}

void fetchCode() {
  if ((int)strlen(codeLook) < 4) {
    strlcpy(zipMsg, "code needs 4 letters", sizeof(zipMsg));
    return;
  }
  int known = findHomeField(codeLook);
  if (known < 0) {
    strlcpy(zipMsg, "that code is not in the list", sizeof(zipMsg));
    return;
  }
  useHomeField(known);
  applyHomeTz();
  savePrefs();
  snprintf(zipMsg, sizeof(zipMsg), "%s", homeFields[known].city);
}

void drawShips() {
  if (!shipsOn) return;
  uint16_t ink = RGB565(70, 230, 120);
  for (int i = 0; i < nShips; i++) {
    float px, py;
    llToXY(ships[i].lat, ships[i].lon, px, py);
    int x = (int)px;
    int y = (int)py;
    ships[i].w = 0;
    if (x < 8 || x >= PW - 8 || y < 8 || y >= MAP_H - 8) continue;
    gfx->fillTriangle(x, y - 6, x - 5, y + 4, x + 5, y + 4, ink);
    ships[i].x = x - 6;
    ships[i].y = y - 7;
    ships[i].w = 12;
    ships[i].h = 13;
  }
}

void paintOverlays() {
  drawRings();
  drawPlaneDots();
  drawShips();
  drawLiveHud();
  gfx->fillCircle(CX, CY, 3, COL_MAG);
  drawCompass();
}

void paintRadar() {
  blitMap();
  paintOverlays();
}

void refreshScopeTraffic() {
  if (page != PG_SCOPE) return;
  erasePlaneMarks();
  drawPlaneDots();
  drawShips();
  drawLiveHud();
}

bool onCompass(int x, int y) {
  if (x >= CX - 11 && x <= CX + 11 && y >= 3 && y <= 23) return true;
  if (x >= CX - 11 && x <= CX + 11 && y >= MAP_H - 23 && y <= MAP_H - 3) return true;
  if (x >= 4 && x <= 31 && y >= CY - 10 && y <= CY + 10) return true;
  if (x >= PW - 31 && x <= PW - 4 && y >= CY - 10 && y <= CY + 10) return true;
  if (x >= 4 && x <= 139 && y >= 2 && y <= 23) return true;
  return false;
}

void mapPut(int x, int y) {
  if ((unsigned)x >= (unsigned)PW || (unsigned)y >= (unsigned)MAP_H) return;
  if (onCompass(x, y)) return;
  uint16_t c = COL_BG;
  if (mapOk && mapFb) {
    prepMapScale();
    if (mapSX == 256 && mapSY == 256)
      c = mapFb[y * PW + x];
    else
      c = mapFb[mapSrcCoord(y, CY, mapSY, MAP_H) * PW + mapSrcCoord(x, CX, mapSX, PW)];
  }
  gfx->drawPixel(x, y, c);
}

void restoreSweep(float deg) {
  float rad = deg * 0.0174533f;
  int x0 = CX, y0 = CY;
  int x1 = CX + (int)(sinf(rad) * RR);
  int y1 = CY - (int)(cosf(rad) * RR);
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    mapPut(x0, y0);
    mapPut(x0 + 1, y0);
    mapPut(x0 - 1, y0);
    mapPut(x0 + 2, y0);
    mapPut(x0 - 2, y0);
    mapPut(x0, y0 + 1);
    mapPut(x0, y0 - 1);
    mapPut(x0, y0 + 2);
    mapPut(x0, y0 - 2);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void strokeSweep(float deg) {
  float rad = deg * 0.0174533f;
  int x0 = CX, y0 = CY;
  int x1 = CX + (int)(sinf(rad) * RR);
  int y1 = CY - (int)(cosf(rad) * RR);
  uint16_t ink = dayMode ? RGB565(90, 48, 0) : RGB565(255, 210, 40);
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (!onCompass(x0, y0) && (unsigned)x0 < (unsigned)PW && (unsigned)y0 < (unsigned)MAP_H) {
      gfx->drawPixel(x0, y0, ink);
      gfx->drawPixel(x0 + 1, y0, ink);
      gfx->drawPixel(x0, y0 + 1, ink);
      if (dayMode) {
        gfx->drawPixel(x0 - 1, y0, ink);
        gfx->drawPixel(x0, y0 - 1, ink);
      }
    }
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void tickSweep() {
  restoreSweep(prevSweepDeg);
  patchRings(prevSweepDeg);
  prevSweepDeg = sweepDeg;
  patchRings(sweepDeg);
  strokeSweep(sweepDeg);
  gfx->fillCircle(CX, CY, 3, COL_MAG);
}

void drawOneMark(int m) {
  if (m < 0 || m >= nMarks) return;
  int i = marks[m].idx;
  if (i < 0 || i >= nPlanes) return;
  int ix = marks[m].x, iy = marks[m].y;
  uint16_t pc = planeColor(planes[i].alt);
  if (showMarks) {
    if (i == selected) gfx->drawCircle(ix, iy, 8, COL_MAG);
    planeArrow(ix, iy, planes[i].hdg, pc);
    return;
  }
  if (i == selected) gfx->drawCircle(ix, iy, 6, COL_MAG);
  gfx->fillCircle(ix, iy, 3, pc);
  float h = planes[i].hdg * 0.0174533f;
  gfx->drawLine(ix, iy, ix + (int)(sinf(h) * 8), iy - (int)(cosf(h) * 8), pc);
  char tag[10];
  strlcpy(tag, planes[i].call[0] ? planes[i].call : "----", sizeof(tag));
  gfx->setTextSize(1);
  gfx->fillRoundRect(marks[m].bx, marks[m].by, marks[m].bw, marks[m].bh, 2, COL_BLK);
  gfx->drawRoundRect(marks[m].bx, marks[m].by, marks[m].bw, marks[m].bh, 2, pc);
  gfx->setTextColor(COL_YEL, COL_BLK);
  gfx->setCursor(marks[m].bx + 3, marks[m].by + 2);
  gfx->print(tag);
}

bool markNearSweep(int m, float deg) {
  if (m < 0 || m >= nMarks) return false;
  int ix = marks[m].x, iy = marks[m].y;
  float ang = atan2f((float)(ix - CX), (float)(CY - iy)) * 57.2958f;
  if (ang < 0) ang += 360.0f;
  float d = fabsf(ang - deg);
  if (d > 180.0f) d = 360.0f - d;
  if (d < 24.0f) return true;
  int bx = marks[m].bx, by = marks[m].by;
  float ang2 = atan2f((float)(bx + marks[m].bw / 2 - CX), (float)(CY - (by + marks[m].bh / 2))) * 57.2958f;
  if (ang2 < 0) ang2 += 360.0f;
  float d2 = fabsf(ang2 - deg);
  if (d2 > 180.0f) d2 = 360.0f - d2;
  return d2 < 24.0f;
}

void redrawMarksOnSweep(float deg) {
  for (int m = 0; m < nMarks; m++)
    if (markNearSweep(m, deg)) drawOneMark(m);
}

void redrawPlaneOverlays() {
  if (page != PG_SCOPE) return;
  for (int m = 0; m < nMarks; m++) drawOneMark(m);
  patchRings(sweepDeg);
  gfx->fillCircle(CX, CY, 3, COL_MAG);
}

void tickSweepIfDue() {
  return;
  if (page != PG_SCOPE) return;
  if (millis() - lastSweep <= 80UL) return;
  lastSweep = millis();
  float oldDeg = prevSweepDeg;
  sweepDeg = fmodf(sweepDeg + 8.0f, 360.0f);
  tickSweep();
  redrawMarksOnSweep(oldDeg);
  redrawMarksOnSweep(sweepDeg);
}

int verPack(const char *s) {
  if (!s || !s[0]) return 0;
  int a = 0, b = 0;
  while (*s && !isdigit((unsigned char)*s)) s++;
  while (*s && isdigit((unsigned char)*s)) a = a * 10 + (*s++ - '0');
  if (*s == '.') s++;
  while (*s && isdigit((unsigned char)*s)) b = b * 10 + (*s++ - '0');
  return a * 100 + b;
}

void trimWord(char *s) {
  if (!s) return;
  char *a = s;
  while (*a && isspace((unsigned char)*a)) a++;
  char *e = a + strlen(a);
  while (e > a && isspace((unsigned char)e[-1])) e--;
  *e = 0;
  if (a != s) memmove(s, a, (size_t)(e - a) + 1);
}

bool otaSlotOk() { return esp_ota_get_next_update_partition(NULL) != nullptr; }

void otaNote(const char *s) {
  strlcpy(otaMsg, s ? s : "", sizeof(otaMsg));
}

void drawOtaStatus() {
  if (page != PG_ABOUT) return;
  gfx->fillRect(12, 222, 456, 45, COL_BG);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  centre(otaMsg[0] ? otaMsg : "SETUP > ABOUT  then CHECK", CX, 227);
  if (otaPct >= 0) {
    int w = (432 * constrain(otaPct, 0, 100)) / 100;
    gfx->drawRect(24, 247, 432, 12, COL_DIM);
    if (w > 0) gfx->fillRect(24, 247, w, 12, COL_HOT);
  }
}

void otaProgress(int cur, int tot) {
  if (tot > 0) otaPct = (int)((cur * 100L) / tot);
}

bool flashFromFile(File &f) {
  size_t sz = f.size();
  if (sz < 100000) {
    otaNote("file too small");
    return false;
  }
  if (!Update.begin(sz)) {
    otaNote(otaSlotOk() ? "flash begin fail" : "need USB OTA partition");
    return false;
  }
  uint8_t *buf = (uint8_t *)ps_malloc(4096);
  if (!buf) buf = (uint8_t *)malloc(4096);
  if (!buf) {
    Update.abort();
    otaNote("no ram");
    return false;
  }
  size_t got = 0;
  while (f.available()) {
    int n = f.read(buf, 4096);
    if (n <= 0) break;
    if (Update.write(buf, n) != (size_t)n) {
      free(buf);
      Update.abort();
      otaNote("flash write fail");
      return false;
    }
    got += (size_t)n;
    otaProgress((int)got, (int)sz);
  }
  free(buf);
  if (!Update.end()) {
    otaNote("flash end fail");
    return false;
  }
  return true;
}

void otaCheckGithub() {
  otaNewer = false;
  otaLatest[0] = 0;
  if (!HFS_GH_USER[0] || !strcmp(HFS_GH_USER, "YOUR_GITHUB")) {
    otaNote("set HFS_GH_USER or use SD");
    return;
  }
  if (!wifiLinked()) {
    otaNote("join wifi first");
    return;
  }
  char url[220];
  snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/main/ota/version_43.txt",
           HFS_GH_USER, HFS_GH_REPO);
  char body[64];
  if (!httpGetSmall(url, body, sizeof(body), 6000)) {
    otaNote("version_43.txt not on github");
    return;
  }
  trimWord(body);
  if (!body[0] || strlen(body) > 10) {
    otaNote("bad version file");
    return;
  }
  strlcpy(otaLatest, body, sizeof(otaLatest));
  if (verPack(otaLatest) > verPack(HFS_VER)) {
    otaNewer = true;
    snprintf(otaMsg, sizeof(otaMsg), "new  %s  tap INSTALL", otaLatest);
  } else if (verPack(otaLatest) == verPack(HFS_VER))
    otaNote("this is the latest");
  else
    snprintf(otaMsg, sizeof(otaMsg), "github has  %s", otaLatest);
}

void otaFlashGithub() {
  if (!HFS_GH_USER[0] || !strcmp(HFS_GH_USER, "YOUR_GITHUB")) {
    otaNote("set HFS_GH_USER or use SD");
    return;
  }
  if (!wifiLinked()) {
    otaNote("join wifi first");
    return;
  }
  if (!otaSlotOk()) {
    otaNote("USB once: 16M 3MB APP/OTA");
    return;
  }
  otaNote("downloading  do not unplug");
  otaPct = 0;
  char url[220];
  snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/main/ota/mega43.bin",
           HFS_GH_USER, HFS_GH_REPO);
  if (!httpLock()) {
    otaNote("net busy");
    return;
  }
  WiFiClientSecure c;
  c.setInsecure();
  HTTPUpdate u(180000);
  u.rebootOnUpdate(false);
  u.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  u.onProgress(otaProgress);
  t_httpUpdate_return r = u.update(
      c, url, "", [](HTTPClient *http) {
        if (!http) return;
        http->addHeader("Accept", "application/octet-stream");
        http->setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        http->setTimeout(180000);
      });
  httpUnlock();
  if (r == HTTP_UPDATE_OK) {
    otaPct = 100;
    otaNote("done  rebooting");
    delay(800);
    ESP.restart();
  } else
    otaNote("download fail");
}

void otaFlashSd() {
  if (!sdOk) {
    otaNote("no SD card");
    return;
  }
  if (!otaSlotOk()) {
    otaNote("USB once: 16M 3MB APP/OTA");
    return;
  }
  const char *names[] = {"/HFS.BIN", "/hfs.bin", "/hfs_7in.bin", "/firmware.bin", nullptr};
  File f;
  for (int i = 0; names[i]; i++) {
    f = SD.open(names[i], FILE_READ);
    if (f) break;
  }
  if (!f) {
    otaNote("put HFS.BIN on SD");
    return;
  }
  otaNote("writing SD  do not unplug");
  otaPct = 0;
  bool ok = flashFromFile(f);
  f.close();
  if (ok) {
    otaPct = 100;
    otaNote("done  rebooting");
    delay(800);
    ESP.restart();
  }
}

void runOtaJob() {
  otaBusy = true;
  if (otaKind == 1) otaCheckGithub();
  else if (otaKind == 2) otaFlashGithub();
  else if (otaKind == 3) otaFlashSd();
  otaBusy = false;
  if (otaKind != 2 && otaKind != 3) otaPct = -1;
  otaKind = 0;
}

void askOta(uint8_t kind) {
  if (otaBusy) return;
  otaKind = kind;
  otaPct = (kind == 1) ? -1 : 0;
  if (kind == 1) otaNote("checking github");
  else otaNote("starting update");
  askNet(NET_OTA);
}

bool holdNet() {
  return page == PG_WIFI || geoEdit || (page == PG_SETTINGS && setPane == 1);
}

void askNet(int job) {
  if (holdNet() && job != NET_ZIP && job != NET_CODE) return;
  if (netBusy || netJob) {
    if (job == NET_OTA) netPend = NET_OTA;
    else if (job == NET_FACTS && netPend != NET_OTA && netPend != NET_TZ) netPend = NET_FACTS;
    else if (job == NET_ISS_MAP && netPend != NET_FACTS && netPend != NET_OTA && netPend != NET_TZ) netPend = NET_ISS_MAP;
    else if (job == NET_WX && netPend != NET_OTA && netPend != NET_TZ) netPend = NET_WX;
    else if (job == NET_TZ && netPend != NET_OTA && netPend != NET_WX) netPend = NET_TZ;
    else if (job == NET_MAP && netPend != NET_FACTS && netPend != NET_ISS_MAP && netPend != NET_OTA &&
             netPend != NET_WX && netPend != NET_TZ)
      netPend = NET_MAP;
    else if ((job == NET_LAUNCH || job == NET_STORM || job == NET_SHIPS) && (!netPend || netPend == NET_PLANES)) netPend = job;
    else if ((job == NET_ZIP || job == NET_CODE) && netPend != NET_OTA && netPend != NET_TZ) netPend = job;
    else if (!netPend) netPend = job;
    return;
  }
  if (job == NET_FACTS) {
    lastFactsTry = millis();
    if (factsTries < 8) factsTries++;
  }
  netJob = job;
}

void waitNetIdle() {
  while (netBusy || netJob) {
    tickSweepIfDue();
    delay(8);
  }
}

void netTask(void *pv) {
  (void)pv;
  for (;;) {
    int job = netJob;
    if (job) {
      netBusy = true;
      netJob = NET_NONE;
      if (job == NET_PLANES) fetchPlanes();
      else if (job == NET_MAP) fetchMap();
      else if (job == NET_ISS_MAP) {
        if (!issMapOk) fetchIssMap();
        if (!issTle.ok || !lastTle || millis() - lastTle > 86400000UL) fetchIssTle();
        if (!lastIssPos || millis() - lastIssPos > 60000UL) fetchIssNow();
        if (issTle.ok && (!lastPassAt || millis() - lastPassAt > 86400000UL)) buildIssTrackAndPasses();
      }
      else if (job == NET_FACTS && selected >= 0 && selected < nPlanes)
        fetchAircraft(planes[selected]);
      else if (job == NET_WX) fetchMetar();
      else if (job == NET_TZ) fetchPlaceTz();
      else if (job == NET_LAUNCH) fetchLaunches();
      else if (job == NET_OTA) runOtaJob();
      else if (job == NET_STORM) fetchStorm();
      else if (job == NET_SHIPS) fetchShips();
      else if (job == NET_ZIP) fetchZip();
      else if (job == NET_CODE) fetchCode();
      netDoneJob = job;
      netDone = true;
      netBusy = false;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void clockStampNow(char *dst, int n) {
  snprintf(dst, n, "%s|%s|%d|%d|%d|%d|%d|%d|%d|%d", sunUp, sunDn, wxT, wxHi, wxLo, wxFeel,
           wxRh, wxKt, wxDir, wxCode);
}

void takeNetResult() {
  if (!netDone) return;
  netDone = false;
  int j = netDoneJob;
  if (j == NET_PLANES) {
    if (page == PG_SCOPE) refreshScopeTraffic();
  } else if (j == NET_FACTS && page == PG_DETAIL) {
    if (selected >= 0 && selected < nPlanes &&
        (planes[selected].origCity[0] || planes[selected].destCity[0] ||
         planes[selected].routeOk))
      drawDetail();
  } else if (j == NET_SHIPS && page == PG_SCOPE) {
    refreshScopeTraffic();
  } else if ((j == NET_ZIP || j == NET_CODE) && page == PG_SETTINGS && !geoEdit) {
    drawSettings();
  } else if ((j == NET_MAP || j == NET_STORM) && page == PG_SCOPE) {
    drawScope();
  } else if (j == NET_ISS_MAP && page == PG_ISS) {
    drawIss();
  } else if (j == NET_WX && page == PG_WX) {
    drawWeather();
  } else if (j == NET_WX && page == PG_CLOCK) {
    char now[96];
    clockStampNow(now, sizeof(now));
    if (strcmp(now, clockStamp)) drawClock();
  } else if (j == NET_TZ) {
    lastTz = millis();
    if (tzDirty) {
      tzDirty = false;
      savePrefs();
      sunDay = -1;
      sunDst = -2;
      strlcpy(sunUp, "--:--", sizeof(sunUp));
      strlcpy(sunDn, "--:--", sizeof(sunDn));
    } else if (page == PG_CLOCK) drawClock();
  } else if (j == NET_LAUNCH && page == PG_LAUNCH) {
    drawLaunch();
  } else if (j == NET_OTA && page == PG_ABOUT) {
    drawAbout();
  }
  if (netPend && !netBusy && !netJob) {
    if (netPend == NET_FACTS && selected >= 0 && selected < nPlanes &&
        (haveTowns(planes[selected]) || planes[selected].origCity[0]))
      netPend = NET_NONE;
    else {
      int pend = netPend;
      netPend = NET_NONE;
      askNet(pend);
    }
  }
}

void drawScope() {
  page = PG_SCOPE;
  paintRadar();
  drawZoomBar();
  drawHomeMenu();
}

void drawBackHome() {
  chip(5, 5, 84, 20, "< BACK", false);
  chip(391, 5, 84, 20, "HOME", true);
}

void drawList() {
  page = PG_LIST;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("TRAFFIC", CX, 9);
  if (!nPlanes) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre("none in range", CX, 113);
    return;
  }
  if (listTop < 0 || listTop >= nPlanes) listTop = 0;
  int pages = (nPlanes + LIST_ROWS - 1) / LIST_ROWS;
  int pageNo = listTop / LIST_ROWS + 1;
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  char head[48];
  snprintf(head, sizeof(head), "%-8s %-5s %5s %4s %4s", "CALL", "TYPE", "ALT", "KT", "MPH");
  gfx->setCursor(24, 32);
  gfx->print(head);
  int rows = min(LIST_ROWS, nPlanes - listTop);
  for (int i = 0; i < rows; i++) {
    int idx = listTop + i;
    int y = 48 + i * 20;
    Plane &p = planes[idx];
    char line[48];
    snprintf(line, sizeof(line), "%-8s %-5s %5d %3dk %3dm", p.call, p.type, p.alt, p.gs, ktMph(p.gs));
    gfx->setTextColor(planeColor(p.alt), COL_BG);
    gfx->setCursor(24, y);
    gfx->print(line);
  }
  char foot[32];
  snprintf(foot, sizeof(foot), "NEXT PAGE  %d / %d", pageNo, pages);
  chip(110, 230, 260, 28, foot, pages > 1);
}

bool icaoMatch(const char *a, const char *b) {
  if (!a || !b || !a[0] || !b[0]) return false;
  if (!strcasecmp(a, b)) return true;
  if (a[0] == 'K' && !strcasecmp(a + 1, b)) return true;
  if (b[0] == 'K' && !strcasecmp(b + 1, a)) return true;
  int na = (int)strlen(a), nb = (int)strlen(b);
  if (na == 4 && nb == 3 && !strcasecmp(a + 1, b)) return true;
  if (nb == 4 && na == 3 && !strcasecmp(b + 1, a)) return true;
  return false;
}

const char *placeName(const char *icao, const char *city) {
  const char *tryId[2];
  int nTry = 0;
  if (icao && icao[0]) tryId[nTry++] = icao;
  if (city && city[0]) {
    int n = 0;
    while (city[n] && isalpha((unsigned char)city[n])) n++;
    if (n >= 3 && n <= 4 && !city[n] && (!icao || !icao[0] || strcasecmp(city, icao)))
      tryId[nTry++] = city;
  }
  for (int pass = 0; pass < nTry; pass++) {
    const char *id = tryId[pass];
    for (int i = 0; homeFields[i].icao; i++)
      if (icaoMatch(id, homeFields[i].icao)) return homeFields[i].city;
    static const struct {
      const char *id, *town;
    } extra[] = {{"KDTW", "Detroit"},    {"KERI", "Erie"},      {"KLAS", "Las Vegas"},
                 {"KPHX", "Phoenix"},    {"KIAH", "Houston"},   {"KHOU", "Houston"},
                 {"KMIA", "Miami"},      {"KBOS", "Boston"},    {"KBWI", "Baltimore"},
                 {"KDCA", "Washington"}, {"KIAD", "Washington"},{"KCLT", "Charlotte"},
                 {"KMSP", "Minneapolis"},{"KPDX", "Portland"},  {"PHNL", "Honolulu"},
                 {"EGLL", "London"},     {"EGKK", "London"},    {"EGSS", "London"},
                 {"LFPG", "Paris"},      {"EHAM", "Amsterdam"}, {"EDDF", "Frankfurt"},
                 {"EDDM", "Munich"},     {"LEMD", "Madrid"},    {"LIRF", "Rome"},
                 {"RJTT", "Tokyo"},      {"RJAA", "Tokyo"},     {"VHHH", "Hong Kong"},
                 {"OMDB", "Dubai"},      {"OTHH", "Doha"},      {"OOMS", "Muscat"},
                 {"VTBS", "Bangkok"},    {"VTBD", "Bangkok"},   {"WSSS", "Singapore"},
                 {"YSSY", "Sydney"},     {"YMML", "Melbourne"}, {"CYYZ", "Toronto"},
                 {"CYVR", "Vancouver"},  {"CYUL", "Montreal"},  {"MMMX", "Mexico City"},
                 {"KSLC", "Salt Lake"},  {"KSEA", "Seattle"},   {"KDEN", "Denver"},
                 {"KFAT", "Fresno"},     {"KLAX", "Los Angeles"},{"KSFO", "San Francisco"},
                 {"KORD", "Chicago"},    {"KMDW", "Chicago"},   {"KATL", "Atlanta"},
                 {"KJFK", "New York"},   {"KLGA", "New York"},  {"KEWR", "Newark"},
                 {"KDFW", "Dallas"},     {"KDAL", "Dallas"},    {"KPHL", "Philadelphia"},
                 {"KDTW", "Detroit"},    {"KMCO", "Orlando"},   {"KTPA", "Tampa"},
                 {"KFLL", "Fort Lauderdale"},{"KRSW", "Fort Myers"},{"KMSY", "New Orleans"},
                 {"KAUS", "Austin"},     {"KSAT", "San Antonio"},{"KSTL", "St Louis"},
                 {"KMCI", "Kansas City"},{"KIND", "Indianapolis"},{"KCMH", "Columbus"},
                 {"KCVG", "Cincinnati"}, {"KBNA", "Nashville"}, {"KMEM", "Memphis"},
                 {"KRDU", "Raleigh"},    {"KRIC", "Richmond"},  {"KCLE", "Cleveland"},
                 {"KPIT", "Pittsburgh"}, {"KBDL", "Hartford"},  {"KPVD", "Providence"},
                 {"KMKE", "Milwaukee"},  {"KDSM", "Des Moines"},{"KOMA", "Omaha"},
                 {"KABQ", "Albuquerque"},{"KTUS", "Tucson"},    {"KELP", "El Paso"},
                 {"KOKC", "Oklahoma City"},{"KTUL", "Tulsa"},   {"KLIT", "Little Rock"},
                 {"KBHM", "Birmingham"}, {"KSDF", "Louisville"},{"KBUF", "Buffalo"},
                 {"KROC", "Rochester"},  {"KSYR", "Syracuse"},  {"KALB", "Albany"},
                 {"KSMF", "Sacramento"}, {"KOAK", "Oakland"},   {"KSJC", "San Jose"},
                 {"KSAN", "San Diego"},  {"KBUR", "Burbank"},   {"KSNA", "Santa Ana"},
                 {"KLGB", "Long Beach"}, {"KONT", "Ontario"},   {"KPSP", "Palm Springs"},
                 {"KBFL", "Bakersfield"},{"KVIS", "Visalia"},   {"KMCE", "Merced"},
                 {"KMAE", "Madera"},     {"KPRB", "Paso Robles"},{"KSBP", "San Luis Obispo"},
                 {"KSMX", "Santa Maria"},{"KSBA", "Santa Barbara"},{"KOXR", "Oxnard"},
                 {"KCMA", "Camarillo"},  {"KWVI", "Watsonville"},{"KMRY", "Monterey"},
                 {"KSNS", "Salinas"},    {"KRDD", "Redding"},   {"KACV", "Eureka"},
                 {"KCEC", "Crescent City"},{"KMOD", "Modesto"}, {"KSCK", "Stockton"},
                 {"KCCR", "Concord"},    {"KSTS", "Santa Rosa"},{"KTVL", "South Lake Tahoe"},
                 {"KRNO", "Reno"},       {"KBJC", "Denver"},    {"KAPA", "Denver"},
                 {"KCOS", "Colorado Springs"},{"KBOI", "Boise"},{"KGEG", "Spokane"},
                 {"KPDX", "Portland"},   {"KEUG", "Eugene"},    {"KMFR", "Medford"},
                 {"PANC", "Anchorage"},  {"PAFA", "Fairbanks"}, {"PHNL", "Honolulu"},
                 {"PHOG", "Kahului"},    {"PHKO", "Kona"},      {"PHLI", "Lihue"},
                 {"TJSJ", "San Juan"},   {"TNCM", "St Maarten"},{"MWCR", "Grand Cayman"},
                 {"KABQ", "Albuquerque"},{"KBIS", "Bismarck"},  {"KFAR", "Fargo"},
                 {"KDLH", "Duluth"},     {"KGRB", "Green Bay"}, {"KMSN", "Madison"},
                 {"KGRR", "Grand Rapids"},{"KFWA", "Fort Wayne"},{"KDAY", "Dayton"},
                 {"KCAK", "Akron"},      {"KMDT", "Harrisburg"},{"KAVP", "Scranton"},
                 {"KORF", "Norfolk"},    {"KGSO", "Greensboro"},{"KCHS", "Charleston"},
                 {"KSAV", "Savannah"},   {"KJAX", "Jacksonville"},{"KPNS", "Pensacola"},
                 {"KMOB", "Mobile"},     {"KGPT", "Gulfport"},  {"KBTR", "Baton Rouge"},
                 {"KSHV", "Shreveport"}, {"KCRP", "Corpus Christi"},{"KMAF", "Midland"},
                 {"KAMA", "Amarillo"},   {"KLBB", "Lubbock"},   {"KICT", "Wichita"},
                 {"KSPS", "Wichita Falls"},{"KABI", "Abilene"}, {"KACT", "Waco"},
                 {"KTYS", "Knoxville"},  {"KCHA", "Chattanooga"},{"KTRI", "Tri Cities"},
                 {"KLEX", "Lexington"},  {"KCRW", "Charleston"},{"KHTS", "Huntington"},
                 {"KBWI", "Baltimore"},  {"KIAD", "Washington"},{"KDCA", "Washington"},
                 {"KBOS", "Boston"},     {"KMHT", "Manchester"},{"KBTV", "Burlington"},
                 {"KPWM", "Portland ME"},{"KBGR", "Bangor"},    {"KSYR", "Syracuse"},
                 {"HNL", "Honolulu"}, {"OGG", "Kahului"},    {"KOA", "Kona"},
                 {"LIH", "Lihue"},    {"ANC", "Anchorage"},  {"FAI", "Fairbanks"},
                 {"LHR", "London"},   {"LGW", "London"},     {"STN", "London"},
                 {"CDG", "Paris"},    {"AMS", "Amsterdam"},  {"FRA", "Frankfurt"},
                 {"MUC", "Munich"},   {"MAD", "Madrid"},     {"FCO", "Rome"},
                 {"HND", "Tokyo"},    {"NRT", "Tokyo"},      {"HKG", "Hong Kong"},
                 {"DXB", "Dubai"},    {"DOH", "Doha"},       {"MCT", "Muscat"},
                 {"BKK", "Bangkok"},  {"SIN", "Singapore"},  {"SYD", "Sydney"},
                 {"MEL", "Melbourne"},{"MEX", "Mexico City"},{"SJU", "San Juan"},
                 {"SXM", "St Maarten"},{"GCM", "Grand Cayman"},
                 {"KBZN", "Bozeman"}, {"KGPI", "Kalispell"}, {"KGTF", "Great Falls"},
                 {"KHLN", "Helena"}, {"KJAC", "Jackson Hole"}, {"KCPR", "Casper"},
                 {"KRAP", "Rapid City"}, {"KCOD", "Cody"}, {"KRKS", "Rock Springs"},
                 {"KGJT", "Grand Junction"}, {"KMTJ", "Montrose"}, {"KDRO", "Durango"},
                 {"KASE", "Aspen"}, {"KEGE", "Eagle"}, {"KHDN", "Hayden"},
                 {"KGUC", "Gunnison"}, {"KPUB", "Pueblo"}, {"KSAF", "Santa Fe"},
                 {"KROW", "Roswell"}, {"KSGU", "St George"}, {"KCDC", "Cedar City"},
                 {"KPVU", "Provo"}, {"KOGD", "Ogden"}, {"KTWF", "Twin Falls"},
                 {"KPIH", "Pocatello"}, {"KIDA", "Idaho Falls"}, {"KSUN", "Hailey"},
                 {"KEKO", "Elko"}, {"KRDM", "Redmond"}, {"KOTH", "North Bend"},
                 {"KLMT", "Klamath Falls"}, {"KPDT", "Pendleton"}, {"KPSC", "Pasco"},
                 {"KBLI", "Bellingham"}, {"KYKM", "Yakima"}, {"KPUW", "Pullman"},
                 {"KALW", "Walla Walla"}, {"KEAT", "Wenatchee"}, {"KAPC", "Napa"},
                 {"KCIC", "Chico"}, {"KWJF", "Lancaster"}, {"KPMD", "Palmdale"},
                 {"KSBD", "San Bernardino"}, {"KRAL", "Riverside"}, {"KTRM", "Thermal"},
                 {"KIPL", "Imperial"}, {"KNYL", "Yuma"}, {"KFLG", "Flagstaff"},
                 {"KPRC", "Prescott"}, {"KGCN", "Grand Canyon"}, {"KIFP", "Bullhead"},
                 {"KIWA", "Mesa"}, {"KSDL", "Scottsdale"}, {"KCLL", "College Station"},
                 {"KGRK", "Killeen"}, {"KSJT", "San Angelo"}, {"KGGG", "Longview"},
                 {"KTYR", "Tyler"}, {"KTXK", "Texarkana"}, {"KLRD", "Laredo"},
                 {"KBRO", "Brownsville"}, {"KHRL", "Harlingen"}, {"KMFE", "McAllen"},
                 {"KLAW", "Lawton"}, {"KJLN", "Joplin"}, {"KSGF", "Springfield MO"},
                 {"KCOU", "Columbia MO"}, {"KXNA", "Bentonville"}, {"KFSM", "Fort Smith"},
                 {"KMLU", "Monroe"}, {"KAEX", "Alexandria"}, {"KLFT", "Lafayette"},
                 {"KMGM", "Montgomery"}, {"KDHN", "Dothan"}, {"KTCL", "Tuscaloosa"},
                 {"KVPS", "Destin"}, {"KECP", "Panama City"}, {"KTLH", "Tallahassee"},
                 {"KDAB", "Daytona Beach"}, {"KSFB", "Sanford"}, {"KMLB", "Melbourne FL"},
                 {"KSRQ", "Sarasota"}, {"KPGD", "Punta Gorda"}, {"KPIE", "St Pete"},
                 {"KGNV", "Gainesville"}, {"KEYW", "Key West"}, {"KAGS", "Augusta"},
                 {"KCSG", "Columbus GA"}, {"KABY", "Albany GA"}, {"KVLD", "Valdosta"},
                 {"KMYR", "Myrtle Beach"}, {"KCAE", "Columbia SC"}, {"KGSP", "Greenville"},
                 {"KAVL", "Asheville"}, {"KFAY", "Fayetteville"}, {"KILM", "Wilmington"},
                 {"KISO", "Kinston"}, {"KROA", "Roanoke"}, {"KCHO", "Charlottesville"},
                 {"KLYH", "Lynchburg"}, {"KPHF", "Newport News"}, {"KHPN", "White Plains"},
                 {"KISP", "Islip"}, {"KSWF", "Newburgh"}, {"KABE", "Allentown"},
                 {"KACY", "Atlantic City"}, {"KELM", "Elmira"}, {"KITH", "Ithaca"},
                 {"KBGM", "Binghamton"}, {"KYNG", "Youngstown"}, {"KTOL", "Toledo"},
                 {"KEVV", "Evansville"}, {"KPAH", "Paducah"}, {"KSBN", "South Bend"},
                 {"KLAN", "Lansing"}, {"KAZO", "Kalamazoo"}, {"KMBS", "Saginaw"},
                 {"KTVC", "Traverse City"}, {"KPLN", "Pellston"}, {"KATW", "Appleton"},
                 {"KCWA", "Wausau"}, {"KLSE", "La Crosse"}, {"KRST", "Rochester MN"},
                 {"KSTC", "St Cloud"}, {"KBRD", "Brainerd"}, {"KGFK", "Grand Forks"},
                 {"KMOT", "Minot"}, {"KISN", "Williston"}, {"KSUX", "Sioux City"},
                 {"KGRI", "Grand Island"}, {"KLNK", "Lincoln"}, {"KCID", "Cedar Rapids"},
                 {"KDBQ", "Dubuque"}, {"KMLI", "Moline"}, {"KBMI", "Bloomington"},
                 {"KPIA", "Peoria"}, {"KCMI", "Champaign"}, {"KSPI", "Springfield IL"},
                 {"KALO", "Waterloo"}, {"KACK", "Nantucket"}, {"KMVY", "Vineyard"},
                 {"KHYA", "Hyannis"}, {"PHTO", "Hilo"}, {"ITO", "Hilo"},
                 {"PAJN", "Juneau"}, {"JNU", "Juneau"}, {"PASI", "Sitka"},
                 {"SIT", "Sitka"}, {"PABE", "Bethel"}, {"BET", "Bethel"},
                 {"MMUN", "Cancun"}, {"CUN", "Cancun"}, {"MYNN", "Nassau"},
                 {"NAS", "Nassau"}, {"MKJS", "Montego Bay"}, {"MBJ", "Montego Bay"},
                 {"TNCA", "Aruba"}, {"AUA", "Aruba"}, {"MROC", "San Jose"},
                 {"SJO", "San Jose"}, {"MPTO", "Panama"}, {"PTY", "Panama"},
                 {"MMSD", "Los Cabos"}, {"SJD", "Los Cabos"}, {"MMPR", "Vallarta"},
                 {"PVR", "Vallarta"}, {"MMGL", "Guadalajara"}, {"GDL", "Guadalajara"},
                 {"CYYC", "Calgary"}, {"CYOW", "Ottawa"}, {"CYHZ", "Halifax"},
                 {"CYWG", "Winnipeg"}, {"CYEG", "Edmonton"},
                 {"KVNY", "Van Nuys"}, {"KAFW", "Fort Worth"}, {"KTMB", "Tamiami"},
                 {"KPDK", "DeKalb"}, {"KPTK", "Pontiac"}, {"KGVL", "Gainesville"},
                 {"KGKT", "Gatlinburg"}, {"KBRY", "Bardstown"}, {"KJKA", "Gulf Shores"},
                 {"GUF", "Gulf Shores"}, {"KMKY", "Marco Island"}, {"MRK", "Marco Island"},
                 {"KDJT", "West Palm"}, {"DJT", "West Palm"}, {"SKTL", "Tolu"},
                 {"TLU", "Tolu"},
                 {"KHKY", "Hickory"}, {"KFMY", "Fort Myers"}, {"KORL", "Orlando Exec"},
                 {"MMTO", "Toluca"}, {"TLC", "Toluca"}, {"KUES", "Waukesha"},
                 {"NHX", "Barin"}, {"KTEB", "Teterboro"}, {"KCDW", "Caldwell"},
                 {"KMPO", "Pocono"}, {"KAUG", "Augusta"}, {"RKSI", "Seoul"},
                 {"ICN", "Seoul"}, {"KSUS", "Chesterfield"}, {"KHEF", "Manassas"},
                 {"MNZ", "Manassas"}, {"KNEW", "Lakefront"}, {"KABR", "Aberdeen"},
                 {"KVGT", "N Las Vegas"}, {"KINT", "Winston-Salem"}, {"KPWK", "Wheeling"},
                 {"KONO", "Ontario OR"}, {"KMQS", "Coatesville"}, {"CTH", "Coatesville"},
                 {"KFRG", "Farmingdale"}, {"KBED", "Bedford"}, {"KBDR", "Bridgeport"},
                 {"KHVN", "New Haven"}, {"KGON", "Groton"}, {"KDXR", "Danbury"},
                 {"KHFD", "Brainard"}, {"KEWB", "New Bedford"}, {"KBID", "Block Island"},
                 {"KWST", "Westerly"}, {"KOWD", "Norwood"}, {"KBVY", "Beverly"},
                 {"KLWM", "Lawrence"}, {"KLEB", "Lebanon"}, {"KPSM", "Portsmouth"},
                 {"KBHB", "Bar Harbor"}, {"KPQI", "Presque Isle"}, {"KTTN", "Trenton"},
                 {"KILG", "Wilmington DE"}, {"KSBY", "Salisbury"}, {"KEWN", "New Bern"},
                 {"KOAJ", "Jacksonville NC"}, {"KECG", "Elizabeth City"}, {"KJQF", "Concord NC"},
                 {"KMCN", "Macon"}, {"KAHN", "Athens"}, {"KFTY", "Fulton Co"},
                 {"KAPF", "Naples"}, {"KSPG", "St Pete"}, {"KFXE", "Ft Laud Exec"},
                 {"KOPF", "Opa Locka"}, {"KVQQ", "Cecil"}, {"KSGJ", "St Augustine"},
                 {"KVRB", "Vero Beach"}, {"KISM", "Kissimmee"}, {"KBFM", "Mobile"},
                 {"KAUO", "Auburn"}, {"KHUM", "Houma"}, {"KPIB", "Hattiesburg"},
                 {"KMEI", "Meridian"}, {"KGTR", "Columbus MS"}, {"KPQL", "Pascagoula"},
                 {"KDTN", "Shreveport"}, {"KFNT", "Flint"}, {"KYIP", "Willow Run"},
                 {"KARB", "Ann Arbor"}, {"KMKG", "Muskegon"}, {"KRHI", "Rhinelander"},
                 {"KLBF", "North Platte"}, {"KOFK", "Norfolk NE"}, {"KBFF", "Scottsbluff"},
                 {"KSTJ", "St Joseph"}, {"KMWA", "Marion"}, {"KCPS", "Cahokia"},
                 {"KOWB", "Owensboro"}, {"KBWG", "Bowling Green"}, {"KMQY", "Smyrna"},
                 {"KRWI", "Rocky Mount"}, {"KPNE", "Philly NE"}, {"KRDG", "Reading"},
                 {"KLNS", "Lancaster PA"}, {"KIPT", "Williamsport"}, {"KRME", "Rome"},
                 {"KPOU", "Poughkeepsie"}, {"KMGJ", "Montgomery NY"}, {"KOXC", "Oxford"},
                 {"KCON", "Concord NH"}, {"KRUT", "Rutland"}, {"KMPV", "Montpelier"},
                 {"KRKD", "Rockland"},
                 {nullptr, nullptr}};
    for (int i = 0; extra[i].id; i++)
      if (icaoMatch(id, extra[i].id)) return extra[i].town;
  }
  if (city && city[0] && (!icao || !icaoMatch(city, icao)) && strlen(city) > 3) return city;
  return "";
}

void drawDetail() {
  page = PG_DETAIL;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  if (selected < 0 || selected >= nPlanes) {
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setTextSize(1);
    centre("no aircraft", CX, 113);
    return;
  }
  Plane &p = planes[selected];
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre(p.call, CX, 29);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setTextSize(1);
  char line[64];
  snprintf(line, sizeof(line), "%s  %s  %s", p.mfr[0] ? p.mfr : "", p.typeName[0] ? p.typeName : (p.type[0] ? p.type : "----"),
           p.reg[0] ? p.reg : p.hex);
  centre(line, CX, 61);
  if (p.airline[0]) {
    gfx->setTextColor(COL_MID, COL_BG);
    centre(p.airline, CX, 78);
  }
  gfx->setTextColor(COL_CYN, COL_BG);
  const char *fromTown = placeName(p.orig, p.origCity);
  if (!fromTown[0]) fromTown = p.origCity[0] ? p.origCity : "----";
  const char *toTown = placeName(p.dest, p.destCity);
  if (!toTown[0]) toTown = p.destCity[0] ? p.destCity : "----";
  snprintf(line, sizeof(line), "departed from  %s", fromTown);
  centre(line, CX, 100);
  gfx->setTextColor(COL_TXT, COL_BG);
  snprintf(line, sizeof(line), "landing in  %s", toTown);
  centre(line, CX, 117);
  if (p.timesOk && p.dep[0]) {
    snprintf(line, sizeof(line), "OFF %s   LAND %s", p.dep, p.arr[0] ? p.arr : "----");
    centre(line, CX, 135);
    if (p.durMin > 0) {
      snprintf(line, sizeof(line), "DURATION  %dh %02dm", p.durMin / 60, p.durMin % 60);
      centre(line, CX, 152);
    }
  } else if (p.routeOk)
    centre("OFF ----   LAND enroute", CX, 135);
  else
    centre("OFF ----   LAND ----", CX, 135);
  snprintf(line, sizeof(line), "ALT %d    GS %d kt  %d mph  %03d", p.alt, p.gs, ktMph(p.gs), p.hdg);
  centre(line, CX, 170);
  snprintf(line, sizeof(line), "MAX %dkt %dmph   SQ %04d", p.maxGs ? p.maxGs : p.gs,
           ktMph(p.maxGs ? p.maxGs : p.gs), p.squawk);
  centre(line, CX, 187);
  if (p.engines[0]) {
    gfx->setTextColor(COL_MID, COL_BG);
    centre(p.engines, CX, 205);
  }
  if (p.seats || p.cruise) {
    snprintf(line, sizeof(line), "%d seats   cruise %dkt %dmph   FL%d", p.seats, p.cruise,
             ktMph(p.cruise), p.fl);
    gfx->setTextColor(COL_MID, COL_BG);
    centre(line, CX, 222);
  }
  if (p.owner[0]) {
    snprintf(line, sizeof(line), "owner  %s", p.owner);
    centre(line, CX, 239);
  }
}

void drawSoon(int pg, const char *title, const char *msg) {
  page = pg;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre(title, CX, 79);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  centre(msg, CX, 125);
}

void drawWeather() {
  page = PG_WX;
  if (mapOk && mapFb)
    gfx->draw16bitRGBBitmap(0, 0, mapFb, PW, MAP_H);
  else
    gfx->fillRect(0, 0, PW, MAP_H, RGB565(18, 28, 40));
  compassPill(CX, 9, "N");
  compassPill(CX, MAP_H - 16, "S");
  compassPill(14, CY - 5, "W");
  compassPill(PW - 14, CY - 5, "E");
  drawBackHome();
  chip(391, 5, 84, 20, "HOME", true);
  char line[56];
  gfx->fillRect(96, 5, 192, 16, COL_BLK);
  gfx->setTextColor(COL_HOT, COL_BLK);
  gfx->setTextSize(1);
  if (wxOk) {
    if (unitsF)
      snprintf(line, sizeof(line), "%s  %dF  %s", homeIcao, wxT, wxWord(wxCode));
    else
      snprintf(line, sizeof(line), "%s  %dC  %s", homeIcao, (int)((wxT - 32) * 5 / 9), wxWord(wxCode));
  } else
    snprintf(line, sizeof(line), "%s  waiting for live weather", homeIcao);
  gfx->setCursor(102, 7);
  gfx->print(line);
  gfx->fillRect(0, MENU_Y, PW, PH - MENU_Y, COL_BG);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setTextSize(1);
  snprintf(line, sizeof(line), "wind %d deg  %d kt  %d mph  hum %d%%", wxDir, wxKt, ktMph(wxKt), wxRh);
  gfx->setCursor(10, MENU_Y + 6);
  gfx->print(line);
  snprintf(line, sizeof(line), "air quality %d  %s", wxAqi, aqiWord(wxAqi));
  gfx->setCursor(10, MENU_Y + 23);
  gfx->print(line);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  gfx->setCursor(10, MENU_Y + 39);
  gfx->print(metarRaw[0] ? metarRaw : "no weather yet");
}

void drawField() {
  page = PG_FIELD;
  gfx->fillScreen(COL_FIELD);
  drawBackHome();
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_AMB, COL_FIELD);
  gfx->setTextSize(2);
  centre("FIELD", CX, 7);
  int known = findHomeField(homeIcao);
  gfx->fillRoundRect(10, 32, 222, 51, 3, COL_DIM);
  gfx->setTextColor(COL_YEL, COL_DIM);
  gfx->setTextSize(2);
  gfx->setCursor(17, 39);
  gfx->print(homeIcao);
  gfx->setTextColor(COL_TXT, COL_DIM);
  gfx->setTextSize(1);
  gfx->setCursor(17, 66);
  gfx->print(known >= 0 ? homeFields[known].city : "home field");

  int ord[HFS_MAXP];
  sortField(ord, nPlanes);
  gfx->fillRoundRect(242, 32, 228, 51, 3, COL_DIM);
  gfx->setTextColor(COL_AMB, COL_DIM);
  gfx->setTextSize(1);
  gfx->setCursor(252, 36);
  gfx->print("NEAREST");
  if (nPlanes) {
    Plane &n = planes[ord[0]];
    gfx->setTextColor(COL_HOT, COL_DIM);
    gfx->setCursor(252, 52);
    gfx->print(n.call);
    char line[40];
    snprintf(line, sizeof(line), "%.1fNM  %d", distNm(n), n.alt);
    gfx->setTextColor(COL_TXT, COL_DIM);
    gfx->setCursor(252, 68);
    gfx->print(line);
  } else {
    gfx->setTextColor(COL_MID, COL_DIM);
    gfx->setCursor(252, 57);
    gfx->print("none in range");
  }
  char freq[48];
  snprintf(freq, sizeof(freq), "TWR %s   ATIS %s",
           known >= 0 ? homeFields[known].twr : "--",
           known >= 0 ? homeFields[known].atis : "--");
  gfx->setTextColor(COL_CYN, COL_FIELD);
  gfx->setTextSize(1);
  centre(freq, CX, 91);
  if (!nPlanes) {
    gfx->setTextColor(COL_MID, COL_FIELD);
    centre("none in range", CX, 159);
    return;
  }
  gfx->setTextColor(COL_MID, COL_FIELD);
  gfx->setTextSize(1);
  char head[56];
  snprintf(head, sizeof(head), "%-8s %-5s %5s  %6s  %3s", "CALL", "TYPE", "ALT", "NM", "BRG");
  gfx->setCursor(14, 101);
  gfx->print(head);
  if (fieldTop < 0 || fieldTop >= nPlanes) fieldTop = 0;
  int pages = (nPlanes + FIELD_ROWS - 1) / FIELD_ROWS;
  int pageNo = fieldTop / FIELD_ROWS + 1;
  int rows = min(FIELD_ROWS, nPlanes - fieldTop);
  for (int i = 0; i < rows; i++) {
    Plane &p = planes[ord[fieldTop + i]];
    int y = 111 + i * 18;
    uint16_t bg = (i % 2) ? COL_ROW1 : COL_ROW0;
    gfx->fillRect(0, y, PW, 18, bg);
    gfx->setTextColor(COL_TXT, bg);
    gfx->setTextSize(1);
    char line[56];
    snprintf(line, sizeof(line), "%-8s %-5s %5d  %4.1fNM  %03d", p.call, p.type, p.alt, distNm(p),
             bearingTo(p));
    gfx->setCursor(14, y + 3);
    gfx->print(line);
  }
  char foot[32];
  snprintf(foot, sizeof(foot), "NEXT PAGE  %d / %d", pageNo, pages);
  chip(110, 232, 260, 28, foot, pages > 1);
}

void drawSkyTile(int x, int y, int w, int h, const char *a, const char *b) {
  gfx->fillRoundRect(x, y, w, h, 5, COL_DIM);
  gfx->setTextColor(COL_HOT, COL_DIM);
  gfx->setTextSize(2);
  if (b && b[0]) {
    centre(a, x + w / 2, y + 27);
    centre(b, x + w / 2, y + 52);
  } else {
    centre(a, x + w / 2, y + 40);
  }
}

void drawSky() {
  page = PG_SKY;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("SKY", CX, 7);
  drawSkyTile(12, 41, 222, 100, "SKY/CLOCK", "");
  drawSkyTile(246, 41, 222, 100, "TODAY'S", "LAUNCHES");
  drawSkyTile(12, 153, 222, 100, "NIGHT SKY", "");
  drawSkyTile(246, 153, 222, 100, "ISS", "");
}

bool hhmmOk(const char *s) {
  return s && s[0] >= '0' && s[0] <= '2' && s[1] >= '0' && s[1] <= '9' && s[2] == ':' &&
         s[3] >= '0' && s[3] <= '5' && s[4] >= '0' && s[4] <= '9';
}

int hhmmMin(const char *s) {
  return (s[0] - '0') * 600 + (s[1] - '0') * 60 + (s[3] - '0') * 10 + (s[4] - '0');
}

bool clockIsNight(const struct tm &ti) {
  return ti.tm_hour >= 18 || ti.tm_hour < 6;
}

void drawMoon(int cx, int cy) {
  gfx->fillCircle(cx, cy, 9, COL_YEL);
  gfx->fillCircle(cx + 4, cy - 2, 7, COL_BG);
}

bool moonVisible() {
  int pct = moonIllum();
  if (pct < 0 || pct <= 5) return false;
  for (int i = 0; i < nSkyVis; i++)
    if (!strcmp(skyVis[i].name, "Moon")) return true;
  return nSkyVis == 0;
}

void drawWxIcon(int cx, int cy, int code, bool night) {
  const char *w = wxWord(code);
  bool moon = night && moonVisible() && !strcmp(w, "clear");
  if (!strcmp(w, "thunder")) {
    gfx->fillCircle(cx - 6, cy - 2, 9, COL_GRY);
    gfx->fillCircle(cx + 7, cy - 1, 8, COL_GRY);
    gfx->fillCircle(cx + 1, cy + 2, 8, COL_MID);
    gfx->fillTriangle(cx - 2, cy + 1, cx + 6, cy + 1, cx - 1, cy + 12, COL_YEL);
    gfx->fillTriangle(cx + 1, cy + 7, cx + 8, cy + 7, cx + 2, cy + 18, COL_YEL);
  } else if (!strcmp(w, "rain") || !strcmp(w, "snow")) {
    gfx->fillCircle(cx - 7, cy - 3, 8, COL_GRY);
    gfx->fillCircle(cx + 6, cy - 2, 9, COL_MID);
    gfx->fillCircle(cx, cy, 8, COL_GRY);
    uint16_t drop = !strcmp(w, "snow") ? COL_WHT : COL_CYN;
    for (int i = -9; i <= 9; i += 5)
      gfx->fillCircle(cx + i, cy + 11, 2, drop);
  } else if (!strcmp(w, "cloudy") || !strcmp(w, "fog")) {
    gfx->fillCircle(cx - 8, cy, 9, COL_GRY);
    gfx->fillCircle(cx + 2, cy - 3, 10, COL_MID);
    gfx->fillCircle(cx + 10, cy + 1, 8, COL_GRY);
  } else if (!strcmp(w, "windy")) {
    gfx->drawLine(cx - 13, cy - 5, cx + 13, cy - 5, COL_CYN);
    gfx->drawLine(cx - 11, cy, cx + 16, cy, COL_CYN);
    gfx->drawLine(cx - 13, cy + 5, cx + 11, cy + 5, COL_CYN);
    gfx->drawCircle(cx + 14, cy - 8, 5, COL_CYN);
    gfx->drawCircle(cx + 17, cy + 8, 4, COL_CYN);
  } else if (!strcmp(w, "partly cloudy")) {
    if (night && moonVisible())
      drawMoon(cx - 6, cy - 3);
    else if (!night)
      gfx->fillCircle(cx - 6, cy - 3, 9, COL_YEL);
    gfx->fillCircle(cx + 5, cy + 2, 9, COL_GRY);
    gfx->fillCircle(cx + 12, cy + 5, 7, COL_MID);
  } else if (night) {
    if (moon) drawMoon(cx, cy);
  } else {
    gfx->fillCircle(cx, cy, 9, COL_YEL);
    for (int a = 0; a < 8; a++) {
      float r = a * 0.785398f;
      int x0 = cx + (int)(sinf(r) * 13);
      int y0 = cy - (int)(cosf(r) * 12);
      int x1 = cx + (int)(sinf(r) * 18);
      int y1 = cy - (int)(cosf(r) * 17);
      gfx->drawLine(x0, y0, x1, y1, COL_YEL);
    }
  }
}

void drawClock() {
  page = PG_CLOCK;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("CLOCK", CX, 7);
  time_t now;
  struct tm ti;
  if (!liveClock(&now)) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre("waiting for live clock", CX, 113);
    strlcpy(clockFace, "--:--", sizeof(clockFace));
    return;
  }
  clockLocal(now, &ti);
  char line[48];
  int hr = ti.tm_hour > 12 ? ti.tm_hour - 12 : (ti.tm_hour ? ti.tm_hour : 12);
  snprintf(line, sizeof(line), "%d:%02d %s", hr, ti.tm_min, ti.tm_hour >= 12 ? "PM" : "AM");
  strlcpy(clockFace, line, sizeof(clockFace));
  gfx->setTextColor(COL_YEL, COL_BG);
  gfx->setTextSize(2);
  centre(line, CX, 45);
  gfx->fillRoundRect(24, 91, 204, 45, 3, COL_DIM);
  gfx->fillRoundRect(252, 91, 204, 45, 3, COL_DIM);
  gfx->setTextColor(COL_MID, COL_DIM);
  gfx->setTextSize(1);
  centre("SUN UP", 126, 97);
  centre("SUN DOWN", 354, 97);
  gfx->setTextColor(COL_YEL, COL_DIM);
  gfx->setTextSize(2);
  centre(sunUp, 126, 116);
  centre(sunDn, 354, 116);
  bool night = clockIsNight(ti);
  drawWxIcon(54, 158, wxCode, night);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(1);
  centre(clockWxWord(wxCode, night), CX, 142);
  if (unitsF)
    snprintf(line, sizeof(line), "%dF", wxT);
  else
    snprintf(line, sizeof(line), "%dC", (int)((wxT - 32) * 5 / 9));
  gfx->setTextColor(COL_YEL, COL_BG);
  gfx->setTextSize(2);
  gfx->setCursor(372, 141);
  gfx->print(line);
  snprintf(line, sizeof(line), "high %d  low %d   %s", wxHi, wxLo, wxPop >= 40 ? "umbrella" : "dry");
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setTextSize(1);
  centre(line, CX, 180);
  snprintf(line, sizeof(line), "AIR %dF  feels %d  hum %d%%", wxT, wxFeel, wxRh);
  centre(line, CX, 201);
  snprintf(line, sizeof(line), "wind %d deg  %d kt  %d mph", wxDir, wxKt, ktMph(wxKt));
  gfx->setTextColor(COL_MID, COL_BG);
  centre(line, CX, 221);
  clockStampNow(clockStamp, sizeof(clockStamp));
}

void paintClockTime() {
  if (page != PG_CLOCK) return;
  time_t now = 0;
  char line[16];
  if (!liveClock(&now))
    strlcpy(line, "--:--", sizeof(line));
  else {
    struct tm ti;
    clockLocal(now, &ti);
    int hr = ti.tm_hour > 12 ? ti.tm_hour - 12 : (ti.tm_hour ? ti.tm_hour : 12);
    snprintf(line, sizeof(line), "%d:%02d %s", hr, ti.tm_min, ti.tm_hour >= 12 ? "PM" : "AM");
  }
  if (!strcmp(line, clockFace)) return;
  if (!clockFace[0] || clockFace[0] == '-') {
    drawClock();
    return;
  }
  strlcpy(clockFace, line, sizeof(clockFace));
  gfx->fillRect(150, 36, 180, 22, COL_BG);
  gfx->setTextColor(COL_YEL, COL_BG);
  gfx->setTextSize(2);
  centre(line, CX, 45);
}

void drawUsaCard(int y, const Launch &L) {
  gfx->fillRoundRect(10, y, PW - 19, 45, 3, COL_DIM);
  gfx->setTextColor(COL_YEL, COL_DIM);
  gfx->setTextSize(1);
  char line[56];
  snprintf(line, sizeof(line), "WHEN   %s", L.day);
  gfx->setCursor(17, y + 5);
  gfx->print(line);
  gfx->setTextColor(COL_HOT, COL_DIM);
  snprintf(line, sizeof(line), "WHERE  %s", L.pad[0] ? L.pad : "----");
  gfx->setCursor(17, y + 19);
  gfx->print(line);
  gfx->setTextColor(COL_TXT, COL_DIM);
  snprintf(line, sizeof(line), "TIME   %s    %s", L.when, L.agency);
  gfx->setCursor(17, y + 33);
  gfx->print(line);
}

void drawLaunch() {
  page = PG_LAUNCH;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("LAUNCHES", CX, 7);
  if (!wifiLinked() || !launchOk) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre(liveClock(nullptr) ? "waiting for live launches" : "waiting for live clock", CX, 125);
    return;
  }
  if (!nUsaNext) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre("no upcoming in the live list", CX, 125);
    return;
  }
  int y = 32;
  for (int i = 0; i < nUsaNext && i < 4; i++) {
    drawUsaCard(y, usaNext[i]);
    y += 49;
  }
}

void drawNight() {
  page = PG_NIGHT;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("NIGHT SKY", CX, 7);
  if (!lastAstro || millis() - lastAstro > 3600000UL) computeAstro();
  int moon = moonIllum();
  if (moon >= 0) {
    char ml[28];
    if (moon >= 95) snprintf(ml, sizeof(ml), "moon full  %d%%", moon);
    else if (moon <= 5) snprintf(ml, sizeof(ml), "moon new  %d%%", moon);
    else snprintf(ml, sizeof(ml), "moon  %d%%", moon);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre(ml, CX, 29);
  }
  {
    char sky[36];
    snprintf(sky, sizeof(sky), "sky  %s", wxOk ? wxWord(wxCode) : "waiting");
    gfx->setTextColor(COL_TXT, COL_BG);
    gfx->setTextSize(1);
    centre(sky, CX, moon >= 0 ? 43 : 29);
  }
  if (!nSkyVis) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre(liveClock(nullptr) ? "none up tonight" : "waiting for live clock", CX, 125);
    return;
  }
  int y = moon >= 0 ? 59 : 45;
  for (int i = 0; i < nSkyVis && y < 249; i++) {
    uint16_t row = (i % 2) ? COL_ROW1 : COL_ROW0;
    gfx->fillRect(0, y, PW, 32, row);
    gfx->setTextColor(COL_YEL, row);
    gfx->setTextSize(1);
    gfx->setCursor(14, y + 5);
    gfx->print(skyVis[i].name);
    gfx->setTextColor(COL_CYN, row);
    gfx->setCursor(132, y + 5);
    gfx->print(skyVis[i].look);
    gfx->setTextColor(COL_TXT, row);
    char line[40];
    snprintf(line, sizeof(line), "%s  %ddeg", skyVis[i].note, skyVis[i].el);
    gfx->setCursor(180, y + 5);
    gfx->print(line);
    gfx->setTextColor(COL_MID, row);
    gfx->setCursor(14, y + 18);
    gfx->print(skyVis[i].hours);
    y += 32;
  }
}

int issMapX(float lon) { return (int)((lon + 180.0f) * (PW / 360.0f)); }
int issMapY(float lat) { return ISS_Y0 + (int)((72.0f - lat) * ((float)ISS_H / 134.0f)); }

void updateSunPos() {
  time_t t = 0;
  if (!liveClock(&t)) {
    sunOk = false;
    return;
  }
  double jd = unixToJd(t);
  double T = (jd - 2451545.0) / 36525.0;
  double L = 280.460 + 36000.770 * T;
  double M = 357.528 + 35999.050 * T;
  wrap360(L);
  wrap360(M);
  double Mr = M * 0.01745329252;
  double lam = L + 1.915 * sin(Mr) + 0.020 * sin(2 * Mr);
  double eps = 23.439 - 0.013 * T;
  double lr = lam * 0.01745329252, er = eps * 0.01745329252;
  double ra = atan2(cos(er) * sin(lr), cos(lr));
  double dec = asin(sin(er) * sin(lr));
  double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0);
  wrap360(gmst);
  double raDeg = ra * 57.295779513;
  if (raDeg < 0) raDeg += 360.0;
  double gha = gmst - raDeg;
  wrap360(gha);
  sunLon = (float)(-gha);
  if (sunLon < -180.0f) sunLon += 360.0f;
  if (sunLon > 180.0f) sunLon -= 360.0f;
  sunLat = (float)(dec * 57.295779513);
  sunOk = true;
}

bool ensureIssShade() {
  if (issShade) return true;
  issShade = (uint16_t *)ps_malloc((size_t)PW * ISS_H * 2);
  return issShade != nullptr;
}

void blitIssSunNight() {
  if (!sunOk || !issMapOk || !issFb || !ensureIssShade()) return;
  const float deg = 0.01745329252f;
  const float sls = sinf(sunLat * deg);
  const float cls = cosf(sunLat * deg);
  static float slat[200];
  static float clat[200];
  static float cld[800];
  static bool latOk = false;
  if (!latOk) {
    for (int y = 0; y < ISS_H; y++) {
      float lat = (72.0f - (float)y * 134.0f / (float)ISS_H) * deg;
      slat[y] = sinf(lat);
      clat[y] = cosf(lat);
    }
    latOk = true;
  }
  for (int x = 0; x < PW; x++) {
    float lon = (float)x * 360.0f / (float)PW - 180.0f;
    cld[x] = cls * cosf((lon - sunLon) * deg);
  }
  for (int y = 0; y < ISS_H; y++) {
    float sy = slat[y], cy = clat[y];
    uint16_t *src = issFb + y * PW;
    uint16_t *dst = issShade + y * PW;
    for (int x = 0; x < PW; x++) {
      float c = sy * sls + cy * cld[x];
      uint16_t pix = src[x];
      int scale;
      if (c >= 0.20f)
        scale = 256;
      else if (c >= 0.0f)
        scale = 168 + (int)(c * 440.0f);
      else if (c >= -0.18f)
        scale = 140 + (int)((c + 0.18f) * 155.0f);
      else
        scale = 140;
      int r = ((pix >> 11) & 31) * scale >> 8;
      int g = ((pix >> 5) & 63) * scale >> 8;
      int b = (pix & 31) * scale >> 8;
      dst[x] = (uint16_t)((r << 11) | (g << 5) | b);
    }
    if ((y & 31) == 31) delay(0);
  }
  gfx->draw16bitRGBBitmap(0, ISS_Y0, issShade, PW, ISS_H);
}

void drawIssSun() {
  if (!sunOk) return;
  int x = issMapX(sunLon);
  int y = issMapY(sunLat);
  if (x < -5 || x >= PW + 5 || y < ISS_Y0 - 5 || y >= ISS_Y0 + ISS_H + 5) return;
  gfx->fillCircle(x, y, 5, RGB565(255, 196, 48));
  gfx->fillCircle(x, y, 2, COL_YEL);
}

void drawCoast(const int16_t *p, uint16_t edge) {
  int px = -999, py = -999;
  for (int i = 0; p[i] != -127; i += 2) {
    int x = issMapX((float)p[i]);
    int y = issMapY((float)p[i + 1]);
    if (px > -900) {
      gfx->drawLine(px, py, x, y, edge);
      gfx->drawLine(px, py + 1, x, y + 1, edge);
    }
    px = x;
    py = y;
  }
}

void drawEarth() {
  if (issMapOk && issFb) {
    gfx->draw16bitRGBBitmap(0, ISS_Y0, issFb, PW, ISS_H);
    return;
  }
  gfx->fillRect(0, ISS_Y0, PW, ISS_H, RGB565(12, 38, 72));
  static const int16_t na[] = {-168, 66, -155, 61, -141, 70, -130, 69, -125, 72, -105, 68, -95, 72, -88, 68, -80, 73, -70, 62, -66, 58, -56, 54, -68, 46, -75, 35, -81, 25, -90, 29, -97, 26, -105, 22, -111, 24, -117, 33, -125, 40, -125, 49, -135, 55, -154, 59, -168, 62, -127, 0};
  static const int16_t sa[] = {-81, 12, -70, 12, -62, 10, -51, 8, -35, 0, -35, -10, -40, -23, -48, -35, -55, -45, -62, -50, -71, -55, -75, -40, -77, -22, -80, 2, -127, 0};
  static const int16_t af[] = {-17, 32, -9, 37, 10, 37, 32, 31, 43, 11, 51, 12, 48, 0, 40, -26, 32, -30, 18, -35, 12, -18, 9, 4, -5, 5, -17, 16, -127, 0};
  static const int16_t eu[] = {-10, 36, -9, 44, -6, 51, 2, 59, 12, 61, 24, 70, 30, 60, 40, 48, 29, 41, 18, 40, 10, 38, -127, 0};
  static const int16_t as[] = {28, 36, 44, 40, 60, 50, 75, 55, 90, 62, 110, 68, 135, 71, 170, 66, 160, 50, 145, 42, 130, 38, 120, 30, 100, 20, 88, 22, 78, 28, 68, 25, 50, 25, 40, 36, -127, 0};
  static const int16_t au[] = {114, -22, 129, -14, 146, -16, 153, -28, 146, -36, 136, -35, 116, -34, -127, 0};
  static const int16_t gr[] = {-62, 76, -45, 83, -20, 80, -22, 70, -44, 60, -53, 67, -127, 0};
  static const int16_t uk[] = {-8, 58, 2, 58, 1, 50, -6, 50, -127, 0};
  static const int16_t jp[] = {131, 45, 145, 44, 142, 35, 131, 33, -127, 0};
  static const int16_t md[] = {43, -12, 50, -16, 47, -25, 43, -25, -127, 0};
  static const int16_t idn[] = {95, 6, 119, 5, 131, -3, 115, -8, 105, -6, -127, 0};
  const int16_t *sets[] = {na, sa, af, eu, as, au, gr, uk, jp, md, idn};
  uint16_t edge = RGB565(210, 210, 180);
  for (int s = 0; s < 11; s++) drawCoast(sets[s], edge);
}

void drawIss() {
  page = PG_ISS;
  gfx->fillScreen(COL_BG);
  chip(5, 5, 84, 20, "< BACK", false);
  time_t clockNow = 0;
  bool clockOk = liveClock(&clockNow);
  if (clockOk) {
    struct tm ti;
    clockLocal(clockNow, &ti);
    int hr = ti.tm_hour % 12;
    if (!hr) hr = 12;
    char clk[16];
    snprintf(clk, sizeof(clk), "%d:%02d %s", hr, ti.tm_min, ti.tm_hour >= 12 ? "PM" : "AM");
    gfx->setTextColor(COL_YEL, COL_BG);
    gfx->setTextSize(2);
    centre(clk, CX, 7);
  } else {
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setTextSize(2);
    centre("ISS", CX, 7);
  }
  drawEarth();
  updateSunPos();
  blitIssSunNight();
  int px = -999, py = -999;
  for (int i = 0; i < nIssTrack; i++) {
    int x = issMapX(issTrackLon[i]);
    int y = issMapY(issTrackLat[i]);
    if (x >= 0 && x < PW && y >= ISS_Y0 && y < ISS_Y0 + ISS_H) {
      gfx->fillCircle(x, y, 2, COL_PATH);
      if (px > -900 && abs(x - px) < 120) {
        gfx->drawLine(px, py - 1, x, y - 1, COL_PATH);
        gfx->drawLine(px, py, x, y, COL_PATH);
        gfx->drawLine(px, py + 1, x, y + 1, COL_PATH);
      }
    }
    px = x;
    py = y;
  }
  if (issOk || issTle.ok) {
    int x = issMapX(issLon);
    int y = issMapY(issLat);
    if (x >= 0 && x < PW && y >= ISS_Y0 && y < ISS_Y0 + ISS_H) {
      gfx->fillRect(x - 24, y - 3, 17, 6, COL_YEL);
      gfx->fillRect(x + 7, y - 3, 17, 6, COL_YEL);
      gfx->fillRect(x - 7, y - 6, 13, 11, COL_WHT);
      gfx->drawPixel(x, y - 7, COL_HOT);
    }
    gfx->fillCircle(issMapX(homeLon), issMapY(homeLat), 2, COL_MAG);
  }
  drawIssSun();
  time_t now = 0;
  bool soon = false;
  clockOk = liveClock(&now);
  if (clockOk)
    soon = issNextPass && issNextPass > now && (issNextPass - now) <= 20 * 60;
  if (issVisibleNow || soon) {
    gfx->fillRoundRect(10, 41, 180, 20, 2, COL_HOT);
    gfx->setTextColor(COL_BG, COL_HOT);
    gfx->setTextSize(1);
    char look[32];
    if (issVisibleNow)
      snprintf(look, sizeof(look), "LOOK %s  %ddeg", issLook, issEl);
    else {
      int sec = (int)(issNextPass - now);
      const char *from = nIssPass ? issPasses[0].from : issLook;
      snprintf(look, sizeof(look), "LOOK %s  %d:%02d", from, sec / 60, sec % 60);
    }
    centre(look, 100, 45);
  }
  gfx->fillRect(0, ISS_Y0 + ISS_H, PW, PH - (ISS_Y0 + ISS_H), COL_BG);
  gfx->setTextColor(COL_YEL, COL_BG);
  gfx->setTextSize(1);
  gfx->setCursor(14, 156);
  gfx->print("VISIBLE PASSES");
  if (!issTle.ok) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(14, 181);
    gfx->print("waiting for live ISS data");
  } else if (!clockOk) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(14, 181);
    gfx->print("waiting for live clock");
  } else if (!lastPassAt) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(14, 181);
    gfx->print("checking passes");
  } else if (!nIssPass) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(14, 181);
    gfx->print("none in the next 10 days");
  } else {
    int y = 177;
    for (int i = 0; i < nIssPass && y < 261; i++) {
      gfx->setTextColor(COL_TXT, COL_BG);
      gfx->setCursor(14, y);
      gfx->print(issPasses[i].line);
      y += 16;
    }
  }
  chip(391, 5, 84, 20, "HOME", true);
}

void openIss() {
  if (!issMapOk) fetchIssMap();
  drawIss();
  askNet(NET_ISS_MAP);
}

void openLaunch() {
  drawLaunch();
  if (!launchOk) askNet(NET_LAUNCH);
}

void applyBl() {
  pinMode(GFX_BL, OUTPUT);
  int v = (blLevel == 0) ? 70 : (blLevel == 1) ? 150 : 255;
  analogWrite(GFX_BL, v);
}

void drawSetTabs() {
  const char *lab[] = {"DISPLAY", "LOCATION", "ABOUT"};
  for (int i = 0; i < 3; i++) {
    int x = 12 + i * 154;
    bool on = (page == PG_ABOUT && i == 2) || (page == PG_SETTINGS && setPane == i);
    chip(x, 29, 149, 20, lab[i], on);
  }
}

void tapSetTabs(int x, int y) {
  if (y < 29 || y > 50) return;
  if (x < 161) {
    page = PG_SETTINGS;
    setPane = 0;
    drawSettings();
  } else if (x < 314) {
    page = PG_SETTINGS;
    setPane = 1;
    drawSettings();
  } else {
    drawAbout();
  }
}

void drawAbout() {
  page = PG_ABOUT;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  drawSetTabs();
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("HOME FIELD", CX, 63);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setTextSize(1);
  char ver[36];
  snprintf(ver, sizeof(ver), "firmware version  %s", HFS_VER);
  centre(ver, CX, 88);
  if (otaLatest[0]) {
    snprintf(ver, sizeof(ver), "github  %s", otaLatest);
    centre(ver, CX, 107);
  }
  gfx->setTextColor(COL_MID, COL_BG);
  centre("check for updates", CX, 125);
  chip(24, 143, 210, 27, "CHECK", false);
  chip(246, 143, 210, 27, "INSTALL", otaNewer);
  chip(120, 179, 240, 27, "FROM SD", false);
  drawOtaStatus();
}

void tapAbout(int x, int y) {
  if (y >= 29 && y <= 50) {
    tapSetTabs(x, y);
    return;
  }
  if (otaBusy) return;
  if (hit(x, y, 24, 143, 234, 170)) {
    askOta(1);
    drawAbout();
  } else if (hit(x, y, 246, 143, 456, 170)) {
    if (!otaNewer) otaNote("CHECK first  or use SD");
    else askOta(2);
    drawAbout();
  } else if (hit(x, y, 120, 179, 360, 206)) {
    askOta(3);
    drawAbout();
  }
}

void drawSettings() {
  page = PG_SETTINGS;
  gfx->fillScreen(COL_BG);
  drawBackHome();
  drawSetTabs();
  char line[48];
  if (setPane == 0) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    gfx->setCursor(14, 67);
    gfx->print("RANGE");
    chip(120, 61, 48, 23, "-", false);
    snprintf(line, sizeof(line), "%d NM", rangeNm);
    gfx->setTextColor(COL_YEL, COL_BG);
    gfx->setTextSize(2);
    centre(line, CX, 66);
    chip(312, 61, 48, 23, "+", false);

    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    gfx->setCursor(14, 95);
    gfx->print("MAP");
    chip(14, 111, 144, 23, "STREET", mapOn && mapStyle == 0);
    chip(168, 111, 144, 23, "SAT", mapOn && mapStyle == 1);
    chip(322, 111, 144, 23, "OFF", !mapOn);

    gfx->setCursor(14, 143);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->print("MODE");
    chip(120, 138, 108, 20, "DARK", !dayMode);
    chip(240, 138, 108, 20, "DAY", dayMode);
    chip(356, 138, 110, 20, "STORM", stormOn);

    gfx->setCursor(14, 170);
    gfx->print("LIGHT");
    chip(120, 165, 90, 20, "LOW", blLevel == 0);
    chip(218, 165, 90, 20, "MED", blLevel == 1);
    chip(317, 165, 90, 20, "HIGH", blLevel == 2);

    chip(14, 193, 120, 20, "SATELLITE", mapOn && mapStyle == 1);
    chip(144, 193, 84, 20, "MAP", mapOn && mapStyle == 0);
    chip(240, 193, 84, 20, "F", unitsF);
    chip(336, 193, 84, 20, "C", !unitsF);

    gfx->setTextSize(1);
    gfx->setCursor(14, 222);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->print("SCREEN SAVER");
    chip(108, 216, 96, 20, "MARKERS", showMarks);
    chip(212, 216, 96, 20, "NUMBERS", !showMarks);
    chip(316, 216, 150, 20, shipsOn ? "SHIPS ON" : "SHIPS", shipsOn);
    chip(14, 240, 84, 23, "OFF", saverMin == 0);
    chip(106, 240, 72, 23, "2m", saverMin == 2);
    chip(185, 240, 72, 23, "5m", saverMin == 5);
    chip(264, 240, 72, 23, "15m", saverMin == 15);
    chip(348, 240, 118, 23, "WIFI", false);
  } else {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre("ZIP IS 5 DIGITS    CODE IS 4 LETTERS", CX, 58);
    chip(8, 70, 150, 24, "TYPE ZIP", false);
    chip(164, 70, 156, 24, "TYPE CODE", false);
    chip(326, 70, 146, 24, "FIND", true);
    gfx->setTextColor(COL_TXT, COL_BG);
    centre(zipMsg[0] ? zipMsg : (zipCode[0] ? zipCode : homeIcao), CX, 100);
    if (nZipHits) {
      for (int i = 0; i < nZipHits && i < 2; i++) {
        char row[28];
        snprintf(row, sizeof(row), "%s  %s", zipHits[i].icao, zipHits[i].city);
        bool on = !strcasecmp(zipHits[i].icao, homeIcao);
        chip(80, 112 + i * 26, 320, 24, row, on);
      }
    } else {
      int known = findHomeField(homeIcao);
      gfx->setTextColor(COL_TXT, COL_BG);
      gfx->setTextSize(2);
      centre(known >= 0 ? homeFields[known].city : homeIcao, CX, 122);
      gfx->setTextSize(1);
    }
    snprintf(line, sizeof(line), "LAT  %.4f", homeLat);
    gfx->setTextColor(COL_TXT, COL_BG);
    gfx->setTextSize(1);
    gfx->setCursor(120, 170);
    gfx->print(line);
    chip(14, 164, 96, 24, "TYPE LAT", false);
    chip(288, 164, 84, 24, "N", false);
    chip(384, 164, 84, 24, "S", false);
    snprintf(line, sizeof(line), "LON  %.4f", homeLon);
    gfx->setCursor(120, 202);
    gfx->print(line);
    chip(14, 196, 96, 24, "TYPE LON", false);
    chip(288, 196, 84, 24, "W", false);
    chip(384, 196, 84, 24, "E", false);
    chip(132, 228, 216, 27, "APPLY FIELD", true);
  }
}

void paintLocationPick() {
  drawSettings();
}

void tapSettings(int x, int y) {
  if (y >= 29 && y <= 50) {
    tapSetTabs(x, y);
    return;
  }
  if (setPane == 0) {
    if (hit(x, y, 120, 61, 168, 84)) {
      rangeNm = max(10, rangeNm - 10);
      savePrefs();
      lastShips = 0;
      requestZoomRefresh();
    } else if (hit(x, y, 312, 61, 360, 84)) {
      rangeNm = min(250, rangeNm + 10);
      savePrefs();
      lastShips = 0;
      requestZoomRefresh();
    } else if (hit(x, y, 14, 111, 158, 134) || hit(x, y, 144, 193, 228, 213)) {
      mapOn = true;
      mapStyle = 0;
      savePrefs();
      mapOk = false;
      askNet(NET_MAP);
    } else if (hit(x, y, 168, 111, 312, 134) || hit(x, y, 14, 193, 134, 213)) {
      mapOn = true;
      mapStyle = 1;
      savePrefs();
      mapOk = false;
      askNet(NET_MAP);
    } else if (hit(x, y, 322, 111, 466, 134)) {
      mapOn = false;
      savePrefs();
    } else if (hit(x, y, 120, 138, 228, 159)) {
      dayMode = false;
      cols();
      savePrefs();
      if (mapOn) askNet(NET_MAP);
    } else if (hit(x, y, 240, 138, 348, 159)) {
      dayMode = true;
      cols();
      savePrefs();
      if (mapOn) askNet(NET_MAP);
    } else if (hit(x, y, 356, 138, 466, 159)) {
      stormOn = !stormOn;
      savePrefs();
      if (netPend == NET_STORM) netPend = NET_NONE;
      if (stormOn) {
        lastStorm = 0;
        if (mapOn && mapOk) askNet(NET_STORM);
        else if (mapOn) askNet(NET_MAP);
      } else if (mapOn && mapOk && jpgSrcClean) {
        rebuildMapFb();
      } else if (mapOn) {
        askNet(NET_MAP);
      }
    } else if (hit(x, y, 120, 165, 210, 186)) {
      blLevel = 0;
      applyBl();
      savePrefs();
    } else if (hit(x, y, 218, 165, 308, 186)) {
      blLevel = 1;
      applyBl();
      savePrefs();
    } else if (hit(x, y, 317, 165, 407, 186)) {
      blLevel = 2;
      applyBl();
      savePrefs();
    } else if (hit(x, y, 240, 193, 324, 213)) {
      unitsF = true;
      savePrefs();
    } else if (hit(x, y, 336, 193, 420, 213)) {
      unitsF = false;
      savePrefs();
    } else if (hit(x, y, 108, 216, 204, 236)) {
      showMarks = true;
      savePrefs();
    } else if (hit(x, y, 212, 216, 308, 236)) {
      showMarks = false;
      savePrefs();
    } else if (hit(x, y, 316, 216, 466, 236)) {
      shipsOn = !shipsOn;
      savePrefs();
      if (shipsOn) {
        lastShips = 0;
        askNet(NET_SHIPS);
      } else {
        nShips = 0;
      }
    } else if (hit(x, y, 14, 240, 98, 263)) {
      saverMin = 0;
      savePrefs();
    } else if (hit(x, y, 106, 240, 178, 263)) {
      saverMin = 2;
      savePrefs();
    } else if (hit(x, y, 185, 240, 257, 263)) {
      saverMin = 5;
      savePrefs();
    } else if (hit(x, y, 264, 240, 336, 263)) {
      saverMin = 15;
      savePrefs();
    } else if (hit(x, y, 348, 240, 466, 263)) {
      pushHist();
      openWifi();
      return;
    }
    drawSettings();
  } else {
    if (hit(x, y, 8, 70, 158, 94)) {
      openGeo(3);
      return;
    } else if (hit(x, y, 164, 70, 320, 94)) {
      openGeo(4);
      return;
    } else if (hit(x, y, 326, 70, 472, 94)) {
      if ((int)strlen(zipCode) >= 5) {
        strlcpy(zipMsg, "looking up zip", sizeof(zipMsg));
        askNet(NET_ZIP);
      } else {
        strlcpy(zipMsg, "zip needs 5 digits", sizeof(zipMsg));
      }
      drawSettings();
      return;
    }
    for (int i = 0; i < nZipHits && i < 2; i++) {
      if (hit(x, y, 80, 112 + i * 26, 400, 136 + i * 26)) {
        strlcpy(homeIcao, zipHits[i].icao, sizeof(homeIcao));
        homeLat = zipHits[i].lat;
        homeLon = zipHits[i].lon;
        lastPassAt = 0;
        dropZone();
        applyHomeTz();
        savePrefs();
        snprintf(zipMsg, sizeof(zipMsg), "%s", zipHits[i].city);
        drawSettings();
        return;
      }
    }
    if (hit(x, y, 14, 164, 110, 188) || hit(x, y, 114, 164, 282, 188)) {
      openGeo(1);
    } else if (hit(x, y, 14, 196, 110, 220) || hit(x, y, 114, 196, 282, 220)) {
      openGeo(2);
    } else if (hit(x, y, 288, 164, 372, 188)) {
      homeLat = constrain(homeLat + 0.02f, -70.0f, 70.0f);
      lastPassAt = 0;
      dropZone();
      applyHomeTz();
      savePrefs();
      drawSettings();
    } else if (hit(x, y, 384, 164, 468, 188)) {
      homeLat = constrain(homeLat - 0.02f, -70.0f, 70.0f);
      lastPassAt = 0;
      dropZone();
      applyHomeTz();
      savePrefs();
      drawSettings();
    } else if (hit(x, y, 288, 196, 372, 220)) {
      homeLon = constrain(homeLon - 0.02f, -179.0f, 179.0f);
      lastPassAt = 0;
      dropZone();
      applyHomeTz();
      savePrefs();
      drawSettings();
    } else if (hit(x, y, 384, 196, 468, 220)) {
      homeLon = constrain(homeLon + 0.02f, -179.0f, 179.0f);
      lastPassAt = 0;
      dropZone();
      applyHomeTz();
      savePrefs();
      drawSettings();
    } else if (hit(x, y, 132, 228, 348, 255)) {
      savePrefs();
      gfx->fillScreen(COL_BG);
      gfx->setTextColor(COL_HOT, COL_BG);
      gfx->setTextSize(2);
      centre("saved", CX, 120);
      delay(400);
      ESP.restart();
    }
  }
}

void drawPlacePad() {
  gfx->fillScreen(COL_BG);
  chip(5, 5, 84, 20, "< BACK", false);
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(1);
  centre(geoEdit == 3 ? "TYPE ZIP" : "TYPE CODE", CX, 9);
  gfx->fillRoundRect(48, 32, 384, 28, 3, COL_DIM);
  gfx->setTextColor(COL_YEL, COL_DIM);
  gfx->setTextSize(2);
  centre(geoBuf[0] ? geoBuf : (geoEdit == 3 ? "00000" : "XXXX"), CX, 38);
  if (geoEdit == 3) {
    const char *r0 = "1234567890";
    for (int i = 0; i < 10; i++) {
      char t[2] = {r0[i], 0};
      chip(10 + i * 47, 78, 43, 36, t, false);
    }
    chip(10, 128, 140, 36, "DEL", false);
    chip(160, 128, 150, 36, "CLEAR", false);
    chip(320, 128, 150, 36, "ENTER", true);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(1);
    centre("5 digits, then ENTER", CX, 190);
    return;
  }
  const char *rows[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  int counts[3] = {10, 9, 7};
  int y0[3] = {72, 108, 144};
  for (int r = 0; r < 3; r++) {
    int x0 = (480 - counts[r] * 46) / 2;
    for (int i = 0; i < counts[r]; i++) {
      char t[2] = {rows[r][i], 0};
      chip(x0 + i * 46, y0[r], 44, 32, t, false);
    }
  }
  chip(10, 186, 140, 32, "DEL", false);
  chip(160, 186, 150, 32, "CLEAR", false);
  chip(320, 186, 150, 32, "OK", true);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  centre("4 letters, then OK", CX, 232);
}

void tapPlace(int x, int y) {
  if (geoEdit == 3) {
    if (y >= 78 && y < 114) {
      int i = (x - 10) / 47;
      if (i >= 0 && i <= 9 && (int)strlen(geoBuf) < 5) {
        int L = (int)strlen(geoBuf);
        geoBuf[L] = (char)('0' + ((i + 1) % 10));
        geoBuf[L + 1] = 0;
      }
      drawPlacePad();
      return;
    }
    if (hit(x, y, 10, 128, 150, 164)) {
      int L = (int)strlen(geoBuf);
      if (L) geoBuf[L - 1] = 0;
      drawPlacePad();
      return;
    }
    if (hit(x, y, 160, 128, 310, 164)) {
      geoBuf[0] = 0;
      drawPlacePad();
      return;
    }
    if (hit(x, y, 320, 128, 470, 164)) applyGeo();
    return;
  }
  const char *rows[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  int counts[3] = {10, 9, 7};
  int y0[3] = {72, 108, 144};
  for (int r = 0; r < 3; r++) {
    if (y < y0[r] || y >= y0[r] + 32) continue;
    int x0 = (480 - counts[r] * 46) / 2;
    int i = (x - x0) / 46;
    if (i >= 0 && i < counts[r] && (int)strlen(geoBuf) < 4) {
      int L = (int)strlen(geoBuf);
      geoBuf[L] = rows[r][i];
      geoBuf[L + 1] = 0;
      drawPlacePad();
    }
    return;
  }
  if (hit(x, y, 10, 186, 150, 218)) {
    int L = (int)strlen(geoBuf);
    if (L) geoBuf[L - 1] = 0;
    drawPlacePad();
    return;
  }
  if (hit(x, y, 160, 186, 310, 218)) {
    geoBuf[0] = 0;
    drawPlacePad();
    return;
  }
  if (hit(x, y, 320, 186, 470, 218)) applyGeo();
}

void drawGeoPad() {
  if (geoEdit >= 3) {
    drawPlacePad();
    return;
  }
  gfx->fillScreen(COL_BG);
  chip(5, 5, 84, 20, "< BACK", false);
  chip(391, 5, 84, 20, "HOME", true);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(1);
  centre(geoEdit == 1 ? "TYPE LATITUDE" : "TYPE LONGITUDE", CX, 9);
  gfx->fillRoundRect(48, 36, 384, 32, 3, COL_DIM);
  gfx->setTextColor(COL_YEL, COL_DIM);
  gfx->setTextSize(2);
  centre(geoBuf[0] ? geoBuf : "0", CX, 45);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  centre(geoEdit == 1 ? "example   36.7762     N is +   S is -" : "example   -119.7181    W is -   E is +", CX, 79);
  const char *r0 = "1234567890";
  int n = (int)strlen(r0);
  for (int i = 0; i < n; i++) {
    char t[2] = {r0[i], 0};
    chip(10 + i * 47, 113, 43, 32, t, false);
  }
  chip(10, 159, 108, 32, ".", false);
  chip(127, 159, 108, 32, "-", false);
  chip(245, 159, 108, 32, "DEL", false);
  chip(362, 159, 108, 32, "ENTER", true);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  centre("then APPLY FIELD on location", CX, 227);
}

void openGeo(int which) {
  geoEdit = which;
  if (which == 1)
    snprintf(geoBuf, sizeof(geoBuf), "%.4f", homeLat);
  else if (which == 2)
    snprintf(geoBuf, sizeof(geoBuf), "%.4f", homeLon);
  else if (which == 3)
    strlcpy(geoBuf, zipCode, sizeof(geoBuf));
  else
    geoBuf[0] = 0;
  drawGeoPad();
}

void geoAppend(char ch) {
  int L = (int)strlen(geoBuf);
  if (L >= 16) return;
  if (ch == '.' && strchr(geoBuf, '.')) return;
  if (ch == '-') {
    if (L) return;
  }
  geoBuf[L] = ch;
  geoBuf[L + 1] = 0;
}

void applyGeo() {
  if (geoEdit == 3) {
    char z[8];
    int n = 0;
    for (int i = 0; geoBuf[i] && n < 5; i++) {
      if (isdigit((unsigned char)geoBuf[i])) z[n++] = geoBuf[i];
    }
    z[n] = 0;
    strlcpy(zipCode, z, sizeof(zipCode));
    geoEdit = 0;
    savePrefs();
    if (n == 5) {
      strlcpy(zipMsg, "looking up zip", sizeof(zipMsg));
      askNet(NET_ZIP);
    } else {
      strlcpy(zipMsg, "zip needs 5 digits", sizeof(zipMsg));
      nZipHits = 0;
    }
    drawSettings();
    return;
  }
  if (geoEdit == 4) {
    char code[6];
    int n = 0;
    for (int i = 0; geoBuf[i] && n < 4; i++) {
      char c = (char)toupper((unsigned char)geoBuf[i]);
      if (c >= 'A' && c <= 'Z') code[n++] = c;
    }
    code[n] = 0;
    geoEdit = 0;
    if (n == 4) {
      strlcpy(codeLook, code, sizeof(codeLook));
      strlcpy(zipMsg, "looking up code", sizeof(zipMsg));
      askNet(NET_CODE);
    } else {
      strlcpy(zipMsg, "code needs 4 letters", sizeof(zipMsg));
    }
    drawSettings();
    return;
  }
  if (!geoBuf[0] || geoBuf[0] == '-' || geoBuf[0] == '.') {
    drawGeoPad();
    return;
  }
  float v = atof(geoBuf);
  if (geoEdit == 1)
    homeLat = constrain(v, -70.0f, 70.0f);
  else
    homeLon = constrain(v, -179.0f, 179.0f);
  lastPassAt = 0;
  dropZone();
  applyHomeTz();
  int known = findHomeField(homeIcao);
  if (known >= 0 &&
      (fabsf(homeLat - homeFields[known].lat) > 0.08f || fabsf(homeLon - homeFields[known].lon) > 0.08f))
    strlcpy(homeIcao, "CUST", sizeof(homeIcao));
  geoEdit = 0;
  savePrefs();
  drawSettings();
}

void tapGeo(int x, int y) {
  if (geoEdit >= 3) {
    tapPlace(x, y);
    return;
  }
  if (hit(x, y, 10, 159, 118, 190)) {
    geoAppend('.');
    drawGeoPad();
    return;
  }
  if (hit(x, y, 127, 159, 235, 190)) {
    if (geoBuf[0] == '-') {
      memmove(geoBuf, geoBuf + 1, strlen(geoBuf));
    } else if ((int)strlen(geoBuf) < 16) {
      memmove(geoBuf + 1, geoBuf, strlen(geoBuf) + 1);
      geoBuf[0] = '-';
    }
    drawGeoPad();
    return;
  }
  if (hit(x, y, 245, 159, 353, 190)) {
    int L = (int)strlen(geoBuf);
    if (L) geoBuf[L - 1] = 0;
    drawGeoPad();
    return;
  }
  if (hit(x, y, 362, 159, 470, 190)) {
    applyGeo();
    return;
  }
  if (y >= 113 && y < 145) {
    int i = (x - 10) / 47;
    if (i >= 0 && i <= 9) {
      geoAppend((char)('0' + ((i + 1) % 10)));
      drawGeoPad();
    }
  }
}

void wifiAppend(char ch) {
  char *d = wifiFocus ? wifiPass : wifiSsid;
  int n = wifiFocus ? (int)sizeof(wifiPass) : (int)sizeof(wifiSsid);
  int L = (int)strlen(d);
  if (L < n - 1) {
    d[L] = ch;
    d[L + 1] = 0;
  }
}

void wifiDel() {
  char *d = wifiFocus ? wifiPass : wifiSsid;
  int L = (int)strlen(d);
  if (L) d[L - 1] = 0;
}

void drawWifiField(int focus, const char *raw, const char *empty, int y) {
  bool on = wifiFocus == focus;
  gfx->fillRoundRect(46, y, 428, 20, 3, on ? COL_HOT : COL_DIM);
  gfx->setTextColor(on ? COL_BG : COL_TXT, on ? COL_HOT : COL_DIM);
  gfx->setTextSize(1);
  char shown[40];
  if (!raw[0]) {
    strlcpy(shown, empty, sizeof(shown));
  } else {
    int L = (int)strlen(raw);
    const char *p = raw;
    if (L > 36) p = raw + (L - 36);
    strlcpy(shown, p, sizeof(shown));
  }
  gfx->setCursor(52, y + 6);
  gfx->print(shown);
}

void drawKey(int x, int y, int w, int h, const char *t) {
  gfx->fillRoundRect(x, y, w, h, 3, COL_DIM);
  gfx->setTextColor(COL_TXT, COL_DIM);
  gfx->setTextSize(1);
  centre(t, x + w / 2, y + h / 2 - 4);
}

void drawRow(const char *s, int y) {
  int n = (int)strlen(s);
  for (int i = 0; i < n; i++) {
    char t[2] = {s[i], 0};
    drawKey(i * KW + 1, y, KW - 2, 20, t);
  }
}

bool hitRow(int x, int y, const char *s, int y0) {
  if (y < y0 || y >= y0 + 20) return false;
  int i = x / KW;
  int n = (int)strlen(s);
  if (i < 0 || i >= n) return false;
  wifiAppend(s[i]);
  if (wifiShift && s[i] >= 'A' && s[i] <= 'Z') wifiShift = false;
  return true;
}

void drawWifi() {
  page = PG_WIFI;
  gfx->fillScreen(COL_BG);
  chip(5, 5, 84, 20, "< BACK", false);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(1);
  centre("JOIN WIFI", CX, 9);
  chip(391, 5, 84, 20, "SCAN", wifiScanBusy);
  chip(4, 30, 32, 22, "<", false);
  chip(444, 30, 32, 22, ">", false);
  for (int i = 0; i < 2; i++) {
    int idx = wifiScanTop + i;
    if (idx >= nWifiScan) break;
    char shown[18];
    strlcpy(shown, wifiScan[idx], sizeof(shown));
    chip(40 + i * 200, 30, 196, 22, shown, !strcmp(wifiScan[idx], wifiSsid));
  }
  if (wifiMsg[0] && nWifiScan == 0) {
    gfx->setTextColor(COL_YEL, COL_BG);
    gfx->setTextSize(1);
    centre(wifiMsg, CX, 36);
  }
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  gfx->setCursor(4, 60);
  gfx->print("SSID");
  drawWifiField(0, wifiSsid, "tap net or type", 56);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(4, 84);
  gfx->print("PASS");
  drawWifiField(1, wifiPass, "type password", 80);
  const char *r0 = "1234567890";
  const char *r1 = wifiSym ? "!@#$%^&*()" : (wifiShift ? "QWERTYUIOP" : "qwertyuiop");
  const char *r2 = wifiSym ? "-_=+[]{};'" : (wifiShift ? "ASDFGHJKL" : "asdfghjkl");
  const char *r3 = wifiSym ? ",.<>/?\\|~`" : (wifiShift ? "ZXCVBNM.@-" : "zxcvbnm.@-");
  drawRow(r0, 106);
  drawRow(r1, 128);
  drawRow(r2, 150);
  drawRow(r3, 172);
  drawKey(432, 150, 46, 20, "DEL");
  chip(4, 196, 72, 22, wifiShift ? "ABC" : "abc", wifiShift);
  chip(80, 196, 240, 22, "SPACE", false);
  chip(324, 196, 72, 22, wifiSym ? "ABC" : "!#", wifiSym);
  chip(4, 222, 154, 28, "SAVE", true);
  chip(162, 222, 154, 28, "RESET WIFI", false);
  chip(320, 222, 156, 28, "FACTORY", false);
}

void startWifiScan() {
  nWifiScan = 0;
  wifiScanTop = 0;
  wifiScanBusy = true;
  strlcpy(wifiMsg, "scanning", sizeof(wifiMsg));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  WiFi.scanDelete();
  WiFi.scanNetworks(true, false);
}

void pollWifiScan() {
  if (!wifiScanBusy) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  wifiScanBusy = false;
  if (n < 0) n = 0;
  nWifiScan = 0;
  for (int i = 0; i < n && nWifiScan < 16; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    strlcpy(wifiScan[nWifiScan], s.c_str(), 33);
    nWifiScan++;
  }
  WiFi.scanDelete();
  wifiScanTop = 0;
  if (nWifiScan)
    wifiMsg[0] = 0;
  else
    strlcpy(wifiMsg, "none found  type name", sizeof(wifiMsg));
  if (page == PG_WIFI) drawWifi();
}

void applyWifiSave() {
  if (!wifiSsid[0]) {
    strlcpy(wifiMsg, "need wifi name", sizeof(wifiMsg));
    drawWifi();
    return;
  }
  saveWifi();
  prefs.begin("mega43", true);
  String got = prefs.getString("ssid", "");
  prefs.end();
  if (got != String(wifiSsid)) {
    strlcpy(wifiMsg, "save failed", sizeof(wifiMsg));
    drawWifi();
    return;
  }
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(1);
  centre("saved", CX, 91);
  gfx->setTextColor(COL_TXT, COL_BG);
  centre(wifiSsid, CX, 113);
  centre("linking", CX, 136);
  delay(600);
  ESP.restart();
}

void openWifi() {
  wifiFocus = wifiSsid[0] ? 1 : 0;
  wifiMsg[0] = 0;
  drawWifi();
  startWifiScan();
  wifiRedrawAt = millis() ? millis() : 1;
}

void pushHist() {
  if (nHist && hist[nHist - 1] == page) return;
  if (nHist >= 8) {
    for (int i = 0; i < 7; i++) hist[i] = hist[i + 1];
    nHist = 7;
  }
  hist[nHist++] = page;
}

void paintPage(int pg) {
  if (pg == PG_SCOPE) drawScope();
  else if (pg == PG_LIST) drawList();
  else if (pg == PG_DETAIL) drawDetail();
  else if (pg == PG_WX) {
    drawWeather();
    if (wifiLinked() && millis() - lastWx > 1000UL) askNet(NET_WX);
  } else if (pg == PG_FIELD) drawField();
  else if (pg == PG_SKY) drawSky();
  else if (pg == PG_CLOCK) {
    drawClock();
    if (wifiLinked() && (clockNeedsWx() || millis() - lastWx > 60000UL)) askNet(NET_WX);
  }
  else if (pg == PG_LAUNCH) openLaunch();
  else if (pg == PG_NIGHT) drawNight();
  else if (pg == PG_ISS) openIss();
  else if (pg == PG_SETTINGS) drawSettings();
  else if (pg == PG_ABOUT) drawAbout();
  else if (pg == PG_WIFI) openWifi();
  else if (pg == PG_SAVER) return;
  else drawScope();
}

void openPage(int pg) {
  if (page != pg) pushHist();
  paintPage(pg);
}

void openDetail(int idx) {
  if (idx < 0 || idx >= nPlanes) return;
  selected = idx;
  fillTypeFacts(planes[idx]);
  factsTries = 0;
  lastFactsTry = millis();
  openPage(PG_DETAIL);
  if (wifiLinked()) askNet(NET_FACTS);
}

void goHome() {
  geoEdit = 0;
  nHist = 0;
  if (wifiSsid[0] && !wifiLinked()) {
    gfx->fillScreen(COL_BG);
    statusLine("linking");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid, wifiPass);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(50);
    wifiOk = wifiLinked();
  }
  wifiOk = wifiLinked();
  drawScope();
  if (wifiOk && !mapOk) askNet(NET_MAP);
  else if (wifiOk && millis() - lastFetch > 8000UL) {
    lastFetch = millis();
    askNet(NET_PLANES);
  }
}

void goBack() {
  if (nHist > 0) {
    paintPage(hist[--nHist]);
    return;
  }
  if (page == PG_CLOCK || page == PG_LAUNCH || page == PG_NIGHT || page == PG_ISS)
    drawSky();
  else if (page == PG_WIFI || page == PG_ABOUT)
    drawSettings();
  else
    goHome();
}

int planeAt(int x, int y) {
  for (int m = 0; m < nMarks; m++) {
    if (x >= marks[m].bx - 1 && x < marks[m].bx + marks[m].bw + 1 &&
        y >= marks[m].by - 1 && y < marks[m].by + marks[m].bh + 1)
      return marks[m].idx;
  }
  int best = -1;
  float bestD = 26;
  for (int i = 0; i < nPlanes; i++) {
    float px, py;
    llToXY(planes[i].lat, planes[i].lon, px, py);
    if (px < 0 || px > PW || py < 0 || py > MAP_H) continue;
    float d = hypotf(px - x, py - y);
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

bool bumpZoom(int dir) {
  int n = constrain(rangeNm + dir * 10, 10, 250);
  if (n == rangeNm) return false;
  rangeNm = n;
  saveRange();
  lastShips = 0;
  requestZoomRefresh();
  if (page == PG_SCOPE) {
    drawZoomBar();
    drawScope();
  }
  return true;
}

void showPage(int pg) { openPage(pg); }

int readTouch(int *xs, int *ys, int maxN) {
  if (maxN < 1 || !ts.touched()) return 0;
  TS_Point a = ts.getPoint();
  delay(4);
  if (!ts.touched()) return 0;
  TS_Point b = ts.getPoint();
  if (abs(a.x - b.x) > 20 || abs(a.y - b.y) > 20) return 0;
  int px = map(((int)a.x + (int)b.x) / 2, 3800, 170, 0, PW - 1);
  int py = map(((int)a.y + (int)b.y) / 2, 250, 3800, 0, PH - 1);
  px = PW - 1 - px;
  py = PH - 1 - py;
  xs[0] = constrain(px, 0, PW - 1);
  ys[0] = constrain(py, 0, PH - 1);
  return 1;
}

void onTap(int x, int y) {
  if (otaBusy) return;
  if (page == PG_SAVER) {
    leaveSaver();
    return;
  }
  if (geoEdit) {
    if (hit(x, y, 391, 5, 475, 25)) {
      geoEdit = 0;
      goHome();
      return;
    }
    if (hit(x, y, 5, 5, 89, 25)) {
      geoEdit = 0;
      drawSettings();
      return;
    }
    tapGeo(x, y);
    return;
  }
  if (page != PG_SCOPE && page != PG_WIFI && hit(x, y, 391, 5, 475, 25)) {
    goHome();
    return;
  }
  if (page != PG_SCOPE && hit(x, y, 5, 5, 89, 25)) {
    goBack();
    return;
  }
  if (page == PG_SETTINGS) {
    tapSettings(x, y);
    return;
  }
  if (page == PG_ABOUT) {
    tapAbout(x, y);
    return;
  }
  if (page == PG_SKY) {
    if (hit(x, y, 12, 41, 234, 141)) openPage(PG_CLOCK);
    else if (hit(x, y, 246, 41, 468, 141))
      openPage(PG_LAUNCH);
    else if (hit(x, y, 12, 153, 234, 253))
      openPage(PG_NIGHT);
    else if (hit(x, y, 246, 153, 468, 253))
      openPage(PG_ISS);
    return;
  }
  if (page == PG_SCOPE) {
    if (y >= ZOOM_Y && y < MENU_Y) {
      if (x < 106) bumpZoom(1);
      else if (x > PW - 106) bumpZoom(-1);
      return;
    }
    if (y >= MENU_Y) {
      int b = x / (PW / 5);
      if (b == 0) openPage(PG_WX);
      else if (b == 1) openPage(PG_LIST);
      else if (b == 2) openPage(PG_FIELD);
      else if (b == 3) openPage(PG_SKY);
      else {
        setPane = 1;
        openPage(PG_SETTINGS);
      }
      return;
    }
    int i = planeAt(x, y);
    if (i >= 0) openDetail(i);
    return;
  }
  if (page == PG_FIELD) {
    if (nPlanes > FIELD_ROWS && y >= 224) {
      fieldTop = (fieldTop + FIELD_ROWS < nPlanes) ? fieldTop + FIELD_ROWS : 0;
      drawField();
    }
    return;
  }
  if (page == PG_LIST) {
    if (!nPlanes) return;
    if (y >= 220) {
      listTop = (listTop + LIST_ROWS < nPlanes) ? listTop + LIST_ROWS : 0;
      drawList();
    } else if (y >= 48 && y < 208) {
      int row = (y - 48) / 20;
      int idx = listTop + row;
      if (row >= 0 && row < LIST_ROWS && idx < nPlanes) openDetail(idx);
    }
    return;
  }
  if (page != PG_WIFI) return;
  if (hit(x, y, 391, 5, 475, 25)) {
    startWifiScan();
    drawWifi();
    return;
  }
  if (hit(x, y, 4, 30, 36, 52)) {
    if (nWifiScan > 1) {
      if (wifiScanTop > 0) wifiScanTop--;
      else wifiScanTop = nWifiScan - 1;
      drawWifi();
    }
    return;
  }
  if (hit(x, y, 444, 30, 476, 52)) {
    if (nWifiScan > 1) {
      if (wifiScanTop + 1 < nWifiScan) wifiScanTop++;
      else wifiScanTop = 0;
      drawWifi();
    }
    return;
  }
  for (int i = 0; i < 2; i++) {
    int idx = wifiScanTop + i;
    if (idx >= nWifiScan) break;
    if (hit(x, y, 40 + i * 200, 30, 235 + i * 200, 52)) {
      strlcpy(wifiSsid, wifiScan[idx], sizeof(wifiSsid));
      wifiFocus = 1;
      wifiMsg[0] = 0;
      drawWifi();
      return;
    }
  }
  if (hit(x, y, 46, 56, 474, 76)) {
    wifiFocus = 0;
    drawWifi();
    return;
  }
  if (hit(x, y, 46, 80, 474, 100)) {
    wifiFocus = 1;
    drawWifi();
    return;
  }
  if (hit(x, y, 4, 222, 158, 250)) {
    applyWifiSave();
    return;
  }
  if (hit(x, y, 162, 222, 316, 250)) {
    clearWifi();
    wifiSsid[0] = wifiPass[0] = 0;
    strlcpy(wifiMsg, "wifi cleared", sizeof(wifiMsg));
    drawWifi();
    return;
  }
  if (hit(x, y, 320, 222, 476, 250)) {
    factoryReset();
    delay(200);
    ESP.restart();
    return;
  }
  if (hit(x, y, 4, 196, 76, 218)) {
    wifiShift = !wifiShift;
    wifiSym = false;
    drawWifi();
    return;
  }
  if (hit(x, y, 80, 196, 320, 218)) {
    wifiAppend(' ');
    drawWifi();
    return;
  }
  if (hit(x, y, 324, 196, 396, 218)) {
    wifiSym = !wifiSym;
    drawWifi();
    return;
  }
  if (hit(x, y, 432, 150, 478, 170)) {
    wifiDel();
    drawWifi();
    return;
  }
  const char *r0 = "1234567890";
  const char *r1 = wifiSym ? "!@#$%^&*()" : (wifiShift ? "QWERTYUIOP" : "qwertyuiop");
  const char *r2 = wifiSym ? "-_=+[]{};'" : (wifiShift ? "ASDFGHJKL" : "asdfghjkl");
  const char *r3 = wifiSym ? ",.<>/?\\|~`" : (wifiShift ? "ZXCVBNM.@-" : "zxcvbnm.@-");
  if (hitRow(x, y, r0, 106) || hitRow(x, y, r1, 128) || hitRow(x, y, r2, 150) || hitRow(x, y, r3, 172))
    drawWifi();
}

void connectWifi() {
  loadPrefs();
  if (wifiSsid[0] == 0) {
    openWifi();
    return;
  }

  gfx->fillScreen(COL_BG);
  statusLine("linking");
#if CONFIG_LWIP_IPV6
  WiFi.enableIPv6(false);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(50);
  wifiOk = wifiLinked();
  if (!wifiOk) {
    drawScope();
    return;
  }
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  WiFi.setDNS(IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
  applyHomeTz();
  lastFetch = 0;
  lastMapTry = 0;
  lastWx = 0;
  lastLaunch = 0;
  drawScope();
  askNet(NET_WX);
  askNet(NET_TZ);
  lastTz = millis();
  askNet(NET_PLANES);
}

void setup() {
  pinMode(GFX_BL, OUTPUT);
  applyBl();
  Serial.begin(115200);
  SPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(0);

  static uint8_t *screenGapBefore = nullptr;
  static uint8_t *screenGapAfter = nullptr;
  if (!screenGapBefore) screenGapBefore = (uint8_t *)psAlloc(1024);
  rgbBus = new Arduino_ESP32RGBPanel(
      PIN_DE, PIN_VSYNC, PIN_HSYNC, PIN_PCLK,
      45, 48, 47, 21, 14,
      5, 6, 7, 15, 16, 4,
      8, 3, 46, 9, 1,
      0, 8, 4, 43,
      0, 8, 4, 12,
      1, 9000000, false, 0, 0, 272 * 20);
  gfx = new Arduino_RGB_Display(PW, PH, rgbBus, 2, true);
  if (!gfx->begin()) {
    Serial.println("gfx begin failed");
    return;
  }
  if (!screenGapAfter) screenGapAfter = (uint8_t *)psAlloc(1024);
  gfx->setTextWrap(false);
  gfx->fillScreen(COL_BG);
  delay(80);
  gfx->fillScreen(COL_BG);

  httpLockInit();
  initSd();
  lastTouch = lastIdle = millis();
  xTaskCreatePinnedToCore(netTask, "hfsnet", 32768, nullptr, 1, nullptr, 0);
  connectWifi();
  applyBl();
  lastIdle = millis();
}

void loop() {
  pollWifiScan();
  if (wifiRedrawAt && page == PG_WIFI && millis() - wifiRedrawAt > 250UL) {
    wifiRedrawAt = 0;
    drawWifi();
  }
  int xs[2], ys[2];
  int nTouch = readTouch(xs, ys, 2);
  if (nTouch) lastIdle = millis();
  if (nTouch >= 2 && page == PG_SCOPE && ys[0] < ZOOM_Y && ys[1] < ZOOM_Y) {
    float d = hypotf((float)(xs[1] - xs[0]), (float)(ys[1] - ys[0]));
    if (pinchDist < 1) pinchDist = d;
    else if (d > pinchDist + 29) {
      bumpZoom(-1);
      pinchDist = d;
    } else if (d < pinchDist - 29) {
      bumpZoom(1);
      pinchDist = d;
    }
  } else {
    pinchDist = 0;
    if (nTouch == 0) stormHeld = false;
    else if (nTouch == 1) {
      bool onStorm = page == PG_SETTINGS && setPane == 0 && hit(xs[0], ys[0], 356, 138, 466, 159);
      uint32_t wait = (page == PG_WIFI || page == PG_SKY || page == PG_CLOCK || page == PG_LAUNCH ||
                       page == PG_NIGHT || page == PG_ISS)
                          ? 110UL
                          : 180UL;
      if (!(onStorm && stormHeld) && millis() - lastTouch > wait) {
        lastTouch = millis();
        if (onStorm) stormHeld = true;
        onTap(xs[0], ys[0]);
      }
    }
  }
  tickSweepIfDue();
  takeNetResult();
  if (otaBusy) {
    lastIdle = millis();
    if (page == PG_ABOUT && millis() - lastOtaDraw > 400UL) {
      lastOtaDraw = millis();
      drawOtaStatus();
    }
    delay(8);
    return;
  }
  tickSaver();
  if (!wifiLinked()) {
    wifiOk = false;
    delay(8);
    return;
  }
  wifiOk = true;
  if (holdNet()) {
    if (netPend != NET_ZIP && netPend != NET_CODE) netPend = NET_NONE;
    delay(8);
    return;
  }
  if (zoomNeed && millis() - zoomNeed > 900UL) {
    zoomNeed = 0;
    lastFetch = millis();
    if (mapOn) {
      lastMapTry = millis();
      askNet(NET_MAP);
    }
    askNet(NET_PLANES);
  } else if (page == PG_DETAIL && selected >= 0 && selected < nPlanes &&
             !haveTowns(planes[selected]) && !planes[selected].origCity[0] && factsTries < 2 &&
             millis() - lastFactsTry > 8000UL) {
    askNet(NET_FACTS);
  } else if (page == PG_ISS && !issMapOk && !zoomNeed && millis() - lastIssMapTry > 4000UL) {
    lastIssMapTry = millis();
    askNet(NET_ISS_MAP);
  } else if (page == PG_SCOPE && mapOn && !mapOk && !zoomNeed && millis() - lastMapTry > 8000UL) {
    lastMapTry = millis();
    askNet(NET_MAP);
  } else if (page == PG_SCOPE && !zoomNeed && millis() - lastFetch > 10000UL) {
    lastFetch = millis();
    askNet(NET_PLANES);
  }
  if (page == PG_ISS && millis() - lastIssTry > 60000UL) {
    lastIssTry = millis();
    askNet(NET_ISS_MAP);
  }
  if (page == PG_ISS && issOk && lastIssPos && millis() - lastIssPos > 180000UL) {
    issOk = false;
    drawIss();
  }
  if (page == PG_WX && millis() - lastWx > 600000UL) askNet(NET_WX);
  if (wifiLinked() && clockNeedsWx() && millis() - lastWx > 8000UL) askNet(NET_WX);
  else if (page == PG_CLOCK && wifiLinked() && millis() - lastWx > 60000UL) askNet(NET_WX);
  else if (wifiLinked() && wxDue()) askNet(NET_WX);
  if (wifiLinked() && (!lastTz || (uint32_t)(millis() - lastTz) > 43200000UL)) {
    askNet(NET_TZ);
    if (netJob == NET_TZ || netPend == NET_TZ) lastTz = millis();
  }
  tickSun();
  if (page == PG_LAUNCH &&
      ((!launchOk && millis() - lastLaunchTry > 8000UL) ||
       (launchOk && millis() - lastLaunch > 60000UL)))
    askNet(NET_LAUNCH);
  if (page == PG_NIGHT && (!lastAstro || millis() - lastAstro > 3600000UL)) {
    computeAstro();
    drawNight();
  }
  if (page == PG_SCOPE && mapOn && mapOk && !zoomNeed && millis() - lastMap > 1800000UL) {
    lastMapTry = millis();
    askNet(NET_MAP);
  }
  if (stormOn && page == PG_SCOPE && mapOn && mapOk && !zoomNeed && millis() - lastStorm > 300000UL) {
    askNet(NET_STORM);
  }
  if (shipsOn && page == PG_SCOPE && !zoomNeed && millis() - lastShips > 60000UL) {
    lastShips = millis();
    askNet(NET_SHIPS);
  }
  if (page == PG_ISS && !issMapOk && !zoomNeed && millis() - lastIssMapTry > 8000UL) {
    lastIssMapTry = millis();
    askNet(NET_ISS_MAP);
  }
  if (page == PG_CLOCK && millis() - lastClock > 10000UL) {
    lastClock = millis();
    paintClockTime();
  }
  delay(8);
}
