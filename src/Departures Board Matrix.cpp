/*
 * Matrix Departures Board (c) 2026 Gadec Software
 *
 * https://github.com/gadec-uk/matrix-departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 *
 * Requires Waveshare ESP32-S3 RGB Matrix Driver Board with 3x 64x32 HUB75 LED Matrix Panels
 *
 */
#include <Arduino.h>
#include <bspPins.h>
#include <Wire.h>
#include <FS.h>
#include <SD_MMC.h>
#include "ESP32-VirtualMatrixPanel-I2S-DMA.h"
#include "esp_psram.h"
#include "es8311.h"
#include <cstring>
#include <cctype>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "es8311.h"

#include <colourThemes.h>
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <customFonts.h>
#include <gfx/graphics.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <StreamString.h>
#include <Ticker.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <HTTPUpdateGitHub.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <SimpleFTPServer.h>
#include <weatherClient.h>
#include <sharedDataStructs.h>
#include <responseCodes.h>
#include <raildataXmlClient.h>
#include <rdmRailClient.h>
#include <TfLdataClient.h>
#include <busDataClient.h>
#include <githubClient.h>
#include <rssClient.h>
#include <touchSensor.h>
#include <webgui/upload.h>
#include <webgui/update.h>
#include <webgui/success.h>
#include <webgui/webgraphics.h>
#include <webgui/index.h>
#include <webgui/keys.h>
#include <webgui/editrss.h>
#include <webgui/rss.h>
#include <time.h>

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 3

#define SCREEN_WIDTH 192 // Panel display width, in pixels
#define SCREEN_HEIGHT 32 // Panel display height, in pixels

class ExternalCanvas16 : public GFXcanvas16 {
  public:
    ExternalCanvas16(uint16_t w, uint16_t h, uint16_t* user_buffer)
        : GFXcanvas16(w, h, false) { // Tell base class NOT to allocate internal RAM
        buffer = user_buffer;        // Assign your our PSRAM buffer to the protected pointer
    }
};

uint16_t* mainCanvasBuffer = (uint16_t*) ps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 2);
ExternalCanvas16 main_canvas(SCREEN_WIDTH, SCREEN_HEIGHT,mainCanvasBuffer);

uint16_t* clipCanvasBuffer = (uint16_t*) ps_malloc(SCREEN_WIDTH * 10 * 2);
ExternalCanvas16 clip_canvas(SCREEN_WIDTH, 10 ,clipCanvasBuffer);

U8G2_FOR_ADAFRUIT_GFX u8g2_main;
U8G2_FOR_ADAFRUIT_GFX u8g2_clip;

MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel  *panel = nullptr;

// Define a few useful colours
#define RGB565_WHITE  0xFFFF
#define RGB565_BLACK  0x0
#define RGB565_BLUE   0x001F
#define RGB565_RED    0xF800
#define RGB565_ORANGE 0xFE00
#define RGB565_CYAN   0x07FF
#define RGB565_LIME   0xC7E0
#define RGB565_GREEN  0x07E0
#define RGB565_YELLOW 0xFFE0
#define RGB565_LIGHTBLUE 0x2BFF

// National Rail & Bus board layout (9 pixel fonts)
#define LINE1 1
#define LINE2 12
#define LINE3 23

// London Underground board layout (10/9 pixel fonts)
#define ULINE1 0
#define ULINE2 11
#define ULINE3 22

#define CLIP_HEIGHT 10
byte FONT_HEIGHT;

// Audio
#define MAX_QUEUE_SIZE 60
#define SAMPLE_RATE 22050

ES8311 es;
File wavFile;
I2SStream i2s;
MP3DecoderHelix MP3Decoder;
Delay delayEffect(80, 0.3, 0.1, SAMPLE_RATE);
AudioEffectStream effectsStream(i2s);
EncodedAudioStream decoderStream(&effectsStream, &MP3Decoder);
StreamCopy copier;

QueueHandle_t audioQueue;

struct AudioCommand {
    char filename[32];
    uint16_t delay;
};

#define msDay 86400000 // 86400000 milliseconds in a day
#define msHour 3600000 // 3600000 milliseconds in an hour
#define msMin 60000 // 60000 milliseconds in a second

static AsyncWebServer server(80); // Hosting the Web GUI
FtpServer* ftpSvr = nullptr; // FTP server for card management

// Shorthand for response formats
static const char contentTypeJson[] = "application/json";
static const char contentTypeText[] = "text/plain";
static const char contentTypeHtml[] = "text/html";

// Using NTP to set and maintain the clock
static struct tm timeinfo;
static const char ukTimezone[] = "GMT0BST,M3.5.0/1,M10.5.0";

// Default hostname
static const char defaultHostname[] = "DeparturesBoard";

static Ticker restartTimer; // used to schedule reboots

// Service attribution texts
static const char nrAttribution[] = "Powered by National Rail Enquiries";
static const char rdgAttribution[] = "Powered by Rail Delivery Group";
static const char btAttribution[] = "Powered by bustimes.org";

// National Rail problem reason hints. The order must not be changed.
static const char* problemReasons[] = {
  "operational incident",
  "obstruction on the",
  "shortage of train drivers",
  "shortage of train crew",
  "fault on the train",
  "signalling",
  "fire next to",
  "hitting an obstruction",
  "trespassers",
  "late running train",
  "train crew being delayed",
  "engineering works",
  "fault on this train",
  "tree blocking",
  "fault with barriers",
  "overcrowding",
  "congestion",
  "police",
  "speed restriction",
  "train crew being unavailable",
  "urgent repairs",
  "safety inspection",
  "shortage of train managers",
  "severe weather",
  "broken down train",
  "passenger being taken ill",
  "train driver being taken ill",
  "fault on a train",
  "animals on the railway",
  "problem currently under",
  "points failure",
  "damage to the overhead",
  "communication alarm",
  "emergency services",
  "change to the timetable",
  "more trains than usual needing",
  "colliding with a bridge",
  "fire at a station"
};

static const size_t NUM_REASONS = sizeof(problemReasons) / sizeof(problemReasons[0]);

#define SCREENSAVERINTERVAL 8000      // How often the screen is changed in sleep mode (ms - 8 seconds)
#define DATAUPDATEINTERVAL 60000      // How often we fetch data from National Rail (ms - 1 mins) - "default" option
#define FASTDATAUPDATEINTERVAL 35000  // How often we fetch data from National Rail (ms - 35 secs) - "fast" option
#define UGDATAUPDATEINTERVAL 30000    // How often we fetch data from TfL (ms - 30 secs)
#define BUSDATAUPDATEINTERVAL 45000   // How often we fetch data from bustimes.org (ms - 45 secs)
#define RSSUPDATEINTERVAL 600000      // How often to refresh the RSS feed (ms - 10 mins)
#define WEATHERUPDATEINTERVAL 1200000 // How often to update the weather forecast (ms - 20 mins)

// Reusable data transfer structures (will be in PSRAM)
rdiStation* xfrStation = nullptr;
stnMessages* xfrMessages = nullptr;
busTubeStation* xfrBusTubeStation = nullptr;
sharedBufferSpace* jsonKeyBuffer = nullptr;

// Station Data (shared, will be in PSRAM)
rdStation* station = nullptr;
// Station Messages (shared, will be in PSRAM)
stnMessages* messages = nullptr;

// Data transfer clients
rdmRailClient* rdmRailData = nullptr;
raildataXmlClient* darwinRailData = nullptr;
TfLdataClient* tfldata = nullptr;
busDataClient* busdata = nullptr;
weatherClient* currentWeather = nullptr;
rssClient* rss = nullptr;
github* ghUpdate = nullptr;

static char* weatherMsg = nullptr;         // Weather message (will be in PSRAM)

// Bit and bobs
static unsigned long timer = 0;
static bool isSleeping = false;            // Is the screen sleeping (showing the "screensaver")
static bool sleepEnabled = false;          // Is overnight sleep enabled?
static bool forcedSleep = false;           // Is the system in manual sleep mode?
static bool forcedAwake = false;           // Was the system woken by touch sensor?
static int stayAwakeSeconds = 300;         // How long to force stay awake since last tap
static bool useNSEclockForSleep = false;   // Use the large NSE clock in "sleep" mode?
static bool longPressClock = false;        // Long press switches to NSE clock mode
static bool showClockNoServices = false;   // Show the NSE clock when no train services at location
static bool noServiceClockIsActive = false;// NSE clock is active due to no services at location
static bool NSEclockIsActive = false;      // Is the large NSE clock active
static bool softResetNeeded = false;       // Is a soft reset pending?
static bool manualUpdateCheck = false;     // Has the GUI requested a firmware update check
static bool weatherEnabled = false;        // Showing weather at station location.
static bool enableBus = false;             // Include Bus services on the board?
static bool firmwareUpdates = false;       // Check for and install firmware updates automatically at boot?
static bool dailyUpdateCheck = false;      // Check for and install firmware updates at midnight?
static byte sleepStarts = 0;               // Hour at which the overnight sleep (screensaver) begins
static byte sleepEnds = 6;                 // Hour at which the overnight sleep (screensaver) ends
static int brightness = 20;                // Initial brightness level of the panels
static unsigned long lastWiFiReconnect=0;  // Last WiFi reconnection time (millis)
static bool firstLoad = true;              // Are we loading for the first time (no station config)?
static int prevProgressBarPosition=0;      // Used for progress bar smooth animation
static int startupProgressPercent;         // Initialisation progress
static bool wifiConnected = false;         // Connected to WiFi?
volatile unsigned long nextDataUpdate = 0; // Next National Rail update time (millis)
static int dataLoadSuccess = 0;            // Count of successful data downloads
static int dataLoadFailure = 0;            // Count of failed data downloads
static unsigned long lastLoadFailure = 0;  // When the last failure occurred
static bool noDataLoaded = true;           // True if no data received for the location
static unsigned long lastDataLoadTime = 0; // Timestamp of last data load
static long apiRefreshRate = DATAUPDATEINTERVAL; // User selected refresh rate for National Rail API (60/35 secs)
static bool noScrolling = false;           // Suppress all horizontal scrolling
static bool flipScreen = false;            // Rotate screen 180deg
static bool swapBG = false;                // Swap the blue/green pixels
static bool mx6126 = false;                // Configure FM6126A chip
static String timezone = "";               // custom (non UK) timezone for the clock
static bool hidePlatform = false;          // Hide platform numbers on Rail board?
static bool hideOrdinals = false;          // Hide service ordinals (2nd, 3rd, 4th etc.)
static bool showLastSeen = false;          // Include last reported arrival after the calling at list
static bool showFullCalling = true;        // Wait for the "Calling at" list to finish scrolling before changing the primary service
static bool showFullMsgs = true;           // Wait for the current service message or RSS feed to finish scrolling before changing primary service
static bool showServiceMsgs = true;        // Show station and service messages (rail/tube)
static bool padPlatform;                   // Used to align platform nos when no scroll is enabled
static bool showTubeCurrentLocation=false; // Show the current location of the primary tube service
static int nrTimeOffset = 0;               // Offset minutes for Rail departures display
static int prevUpdateCheckDay;             // Day of the month the last daily firmware update check was made
static unsigned long fwUpdateCheckTimer=0; // Next time to check if the day has rolled over for firmware update check
static bool apiKeys = false;               // Does apikeys.json exist?
static bool showStationName = true;        // Show the current station name on the board
static boardFonts stationNameFont;         // Which font to use to display the location name
static bool touchEnabled = false;          // TTP223 Touch Sensor enabled?
static bool useRDMclient = false;          // Use the new Rail Data Marketplace API instead of Darwin Lite
static bool enableScheduler = false;       // Scheduler mode is active
static bool enableCarousel = false;        // Carousel mode is active
static bool cardReaderReady = false;       // Is the SD card reader ready with a card installed?
static bool enableFTP = false;             // Should we enable the FTP server
static bool enableAudioAnnouncements = false;  // Audio announcements are active
static bool enableAudioCallingList = true; // Announce the "calling at" stations list
static bool enableReverb = true;           // Add the station reverb effect into the audio pipeline
static int audioVolume = 160;              // Audio volume
static char audioVoice[6] = "/V1";         // Selected audio announcer speechpack
static uint8_t cardType = 100;             // SD card type (for info)
static int numCarouselSlots = 0;
static int currentCarouselSlot = 0;
static int numScheduleSlots = 0;
static int currentScheduleSlot = 0;
static unsigned long nextSchedulerCheck = 0;
static char hostname[33];                  // Network hostname (mDNS)
static char myUrl[24];                     // Stores the board's own url
static char currentServiceId[MAXSERVICEIDSIZE] = ""; // Stores the service ID of the currently announced service


enum tubeAnnouncements {
  NONE = 0,
  NEXT_TRAIN = 1,
  APPROACHING = 2,
  AT_PLATFORM = 3
};

// Keeping track of which announcements have been made for various tube services
struct tubeServices {
  char serviceId[MAXSERVICEIDSIZE];
  tubeAnnouncements lastAnnouncement;
};

#define MAXTUBEAUDIOHISTORY (MAXBOARDSERVICES * 2)
tubeServices tubeAudioHistory[MAXTUBEAUDIOHISTORY];
int tubeAudioHistoryIndex;

// WiFi Manager status
static bool wifiConfigured = false;        // Has WiFi Manager used the captive portal

static char* locationCode = nullptr;         // CRS, Naptan or Atco code of active location
static char* locationName = nullptr;         // Station/Bus stop long name
static char* locationFilter = nullptr;
static char* locationCleanFilter = nullptr;
static float locationLat=0;
static float locationLon=0;
static bool railIsSet = false;
static bool tubeIsSet = false;
static bool busIsSet = false;
static bool schedulerActive = false;
static bool carouselActive = false;
static int activeSlotEventTime;
static int nextSlotEventTime;

static char nrToken[37] = "";              // National Rail Darwin Lite Tokens are in the format nnnnnnnn-nnnn-nnnn-nnnn-nnnnnnnnnnnn, where each 'n' represents a hexadecimal character (0-9 or a-f).
static String rdmDeparturesApiKey = "";    // RDM Consumer key for DeparturesBoard API
static String rdmServiceApiKey = "";       // RDM Consumer key for ServiceDetails API
static char tflAppKey[33] = "";            // TfL app_key (not usually needed)
static char callingCrsCode[4] = "";        // Station code to filter routes on
static char callingStation[45] = "";       // Calling filter station friendly name
static char lineId[33];                    // Underground line to filter on
static char lineDirection[9];              // Underground direction filter
static int busDestX;                       // Variable margin for bus destination

enum boardModes {
  MODE_LOADCONFIG = -1,
  MODE_NEXTMODE = -2,
  MODE_NEXTSCHEDULE = -3,
  MODE_RAIL = 0,
  MODE_TUBE = 1,
  MODE_BUS = 2
};
boardModes boardMode = MODE_RAIL;

// National Rail entry point
#define MAXHOSTSIZE 48                     // Maximum size of the wsdl Host
#define MAXAPIURLSIZE 48                   // Maximum size of the wsdl url
static char wsdlHost[MAXHOSTSIZE];         // wsdl Host name
static char wsdlAPI[MAXAPIURLSIZE];        // wsdl API url

// Coach class availability
static const char firstClassSeating[] = " First class seating only.";
static const char standardClassSeating[] = " Standard class seating only.";
static const char dualClassSeating[] = " First and Standard class seating available.";

// Animation
#define frameTimeRail 25
#define frameTimeTube 18
#define frameTimeBus 40
static int numMessages=0;
static int lastSvcDescMessage=0;
static int scrollStopsXpos = 0;
static int scrollStopsYpos = 0;
static int scrollStopsLength = 0;
static bool isScrollingStops = false;
static bool isShowingCalling = false;
static int currentMessage = 0;
static int prevMessage = 0;
static int prevScrollStopsLength = 0;
static long delayMs;
typedef char Line2Row[MAXCALLINGSIZE + 12];
static Line2Row* line2 = nullptr; // Will be allocated in PSRAM

// Line 3 (additional services)
static int line3Service = 0;
static int scrollServiceYpos = 0;
static bool isScrollingService = false;
static int prevService = -2;
static bool isShowingVia=false;
static unsigned long serviceTimer=0;
static unsigned long viaTimer=0;
static bool showingMessage = false;

// TfL/bus specific animation
static int scrollPrimaryYpos = 0;
static bool isScrollingPrimary = false;
static bool attributionScrolled = false;

static char displayedTime[9] = "";        // The currently displayed time
static char currentTime[9] = "";          // The current time (keep updated in loop)
static unsigned long lastTimeUpdate = 0;
static unsigned long refreshTimer = 0;
static bool timeIsDisplayed = false;      // Clock is visible on the board

// Weather Stuff
static unsigned long nextWeatherUpdate = 0;            // When the next weather update is due
static char* openWeatherMapApiKey = nullptr;           // If no OWM API key is provided, we use Open-Meteo weather data (PSRAM)

// RSS Client
static bool rssEnabled = false;                        // Add RSS feed to the messages
static bool rssPriority = false;                       // Prioritise RSS feed
static unsigned long nextRssUpdate = 0;                // When the next RSS update is due
static String rssURL;                                  // RSS URL to use
static String rssName;                                 // Name of feed for atrribution
static char* rssMessage = nullptr;                     // Holds the current, formatted, RSS message (PSRAM)

// Optional TTP223 touch sensor / push button
touchSensor button(GPIO_NUM_45);

// FreeRTOS Task Handle and Status Flags
TaskHandle_t fetchTaskHandle = NULL;
volatile bool fetchComplete = false;
volatile bool fetchInProgress = false;
volatile bool rssFetchComplete = false;
volatile bool weatherFetchComplete = false;
volatile int lastUpdateResult = UPD_SUCCESS;
volatile int lastWeatherUpdateResult = UPD_SUCCESS;
volatile int lastRssUpdateResult = UPD_SUCCESS;

enum fetchModes {
  FETCH_BOARD = 0,
  FETCH_WEATHER = 1,
  FETCH_RSS = 2
};
fetchModes fetchMode = FETCH_BOARD;

// Create the various large data structures in PSRAM
void createSharedDataStructures() {

  xfrStation = (rdiStation*) heap_caps_calloc(1, sizeof(rdiStation), MALLOC_CAP_SPIRAM);
  xfrMessages = (stnMessages*) heap_caps_calloc(1, sizeof(stnMessages), MALLOC_CAP_SPIRAM);
  xfrBusTubeStation = (busTubeStation*) heap_caps_calloc(1, sizeof(busTubeStation), MALLOC_CAP_SPIRAM);
  jsonKeyBuffer = (sharedBufferSpace*) heap_caps_calloc(1, sizeof(sharedBufferSpace), MALLOC_CAP_SPIRAM);
  station = (rdStation*) heap_caps_calloc(1, sizeof(rdStation), MALLOC_CAP_SPIRAM);
  messages = (stnMessages*) heap_caps_calloc(1, sizeof(stnMessages), MALLOC_CAP_SPIRAM);
  line2 = (Line2Row*) heap_caps_calloc(MAXBOARDMESSAGES+7, sizeof(Line2Row), MALLOC_CAP_SPIRAM);
  weatherMsg = (char*) heap_caps_calloc(MAXWEATHERSIZE, sizeof(char), MALLOC_CAP_SPIRAM);
  rssMessage = (char*) heap_caps_calloc(MAXMESSAGESIZE, sizeof(char), MALLOC_CAP_SPIRAM);
  locationCode = (char*) heap_caps_calloc(MAXLOCATIONCODESIZE, sizeof(char), MALLOC_CAP_SPIRAM);
  locationName = (char*) heap_caps_calloc(MAXLOCATIONSIZE, sizeof(char), MALLOC_CAP_SPIRAM);
  locationFilter = (char*) heap_caps_calloc(MAXFILTERSIZE, sizeof(char), MALLOC_CAP_SPIRAM);
  locationCleanFilter = (char*) heap_caps_calloc(MAXFILTERSIZE, sizeof(char), MALLOC_CAP_SPIRAM);
  openWeatherMapApiKey = (char*) heap_caps_calloc(OWMKEYSIZE, sizeof(char), MALLOC_CAP_SPIRAM);
}

// Create the various data clients
void createDataClients() {
  rdmRailData = new rdmRailClient(xfrStation,xfrMessages,jsonKeyBuffer);
  darwinRailData = new raildataXmlClient(xfrStation,xfrMessages,jsonKeyBuffer);
  tfldata = new TfLdataClient(xfrBusTubeStation,xfrMessages,jsonKeyBuffer);
  busdata = new busDataClient(xfrBusTubeStation,jsonKeyBuffer);
  currentWeather = new weatherClient(jsonKeyBuffer);
  rss = new rssClient(jsonKeyBuffer);
  ghUpdate = new github(jsonKeyBuffer);
}

bool saveFile(String fName, String fData);  //forward dec

bool setupCardReader() {

  SD_MMC.setPins(BSP_SD_CLK, BSP_SD_CMD, BSP_SD_D0);
  if (!SD_MMC.begin("/sdcard", true)) {
    cardType = 99;
    return false;
  }
  cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE) return false;

  return true;
}


bool setupAudioSystem() {

    if(!es.begin(BSP_I2C_SDA, BSP_I2C_SCL, 400000)) {
        log_e("ES8311 begin failed");
        return false;
    }
    es.setVolume(66);
    es.setBitsPerSample(16);
    es.setSampleRate(SAMPLE_RATE);

    auto config = i2s.defaultConfig(TX_MODE);
    config.pin_bck  = BSP_I2S_SCLK;
    config.pin_ws   = BSP_I2S_LCLK;
    config.pin_data = BSP_I2S_DOUT;
    config.pin_mck  = BSP_I2S_MCLK;

    config.sample_rate     = SAMPLE_RATE;
    config.bits_per_sample = 16;
    config.channels        = 1;

    i2s.begin(config);

    // Initialise audio pipeline
    if (enableReverb) effectsStream.addEffect(delayEffect);
    effectsStream.begin();
    decoderStream.begin();

    // Turn on amp
    pinMode(BSP_POWER_AMP_IO, OUTPUT);
    digitalWrite(BSP_POWER_AMP_IO, HIGH);

    return true;
}

//
// addAudioToQueue
//  - announcer: root path to the voice files - e.g. /V1
//  - audioFilename: path/filename of .mp3 file (without extension)
//  - verifyOnly: if true, simply verifies that the file exists on the SD card
//  - delay: ms delay before audio file is played
//
bool addAudioToQueue(const char *announcer, const char *audioFilename, bool verifyOnly, uint16_t delay = 0) {

  if (!enableAudioAnnouncements) return false;

  if (strcmp(audioFilename,"SN/?/???") == 0) return true; // Sometimes NR return ??? as CRS code, ignore these

  char missingFile[24];
  AudioCommand cmd;
  snprintf(cmd.filename, sizeof(cmd.filename), "%s/%s.mp3", announcer, audioFilename, ".mp3");
  cmd.delay = delay;

  if (verifyOnly) {
    // Only check that the file exists on the SD card
    if (SD_MMC.exists(cmd.filename)) {
      return true;
    } else {
      log_e("Audio file '%s' not found. Calling list saved to LittleFS",cmd.filename);
      strlcpy(missingFile,audioFilename,sizeof(missingFile));
      for (int i=0;i<strlen(missingFile);++i) if (missingFile[i]=='/') missingFile[i]='~';  // replace / in path
      String logFilename = "/" + String(missingFile) + ".txt";
      if (!LittleFS.exists(logFilename)) saveFile(logFilename, String(station->calling)+"\n\r"+String(station->callingCrs));
      return false;
    }
  } else {
    // Push to queue. Wait up to 100ms if queue is temporarily full
    if (xQueueSend(audioQueue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      log_e("Warning: Audio queue is full! Could not send '%s'",cmd.filename);
      return false;
    } else {
      // Audio queued OK
      return true;
    }
  }
}

bool addNumberToAudioQueue(const char *announcer, int num, bool checkPath, uint16_t delay = 0) {
  char speechFile[20];
  char digits[3];
  bool result;

  if (!enableAudioAnnouncements) return false;

  if (num<0 || num>99) return false;

  if (num<60) {
    // number
    if (num==0) strcpy(speechFile,"NU/ZO"); // "zero"
    else sprintf(speechFile,"NU/%d",num);
    result = addAudioToQueue(announcer, speechFile, checkPath, delay);
  } else {
    // "20,30,40,50"
    sprintf(digits,"%02d",num);
    sprintf(speechFile,"NU/%c0",digits[0]);
    result = result && addAudioToQueue(announcer, speechFile, checkPath, delay);
    if (digits[1] != '0') {
      sprintf(speechFile,"NU/%c",digits[1]);
      result = result && addAudioToQueue(announcer, speechFile, checkPath, delay);
    }
  }

  return result;
}

// Reset/forget previous tube service announcement history
void resetTubeAudioHistory() {
  for (int i=0;i<MAXTUBEAUDIOHISTORY;++i) {
    tubeAudioHistory[i].serviceId[0] = '\0';
    tubeAudioHistory[i].lastAnnouncement = NONE;
  }
  tubeAudioHistoryIndex = 0;
}

// Load a colour theme
bool loadTheme(const char* themeName) {

  // Load default theme by default
  for (int i=0;i<MAXTHEMEELEMENTS;++i) colours[i] = defaultTheme[i];

  // Check if this is a "system" theme
  if (strcmp(themeName,"ct_Default")==0) return true;
  else if (strcmp(themeName,"ct_Cool_Ice")==0) {
    for (int i=0;i<MAXTHEMEELEMENTS;++i) colours[i] = coolIceTheme[i];
    return true;
  } else if (strcmp(themeName,"ct_Just_Orange")==0) {
    for (int i=0;i<MAXTHEMEELEMENTS;++i) colours[i] = justOrangeTheme[i];
    return true;
  }

  // Try to load the user theme from the file system
  char filename[42];
  snprintf(filename,sizeof(filename),"/%s.json",themeName);
  File file = LittleFS.open(filename, "r");
  if (!file || file.isDirectory()) {
    // Theme file missing
    return false;
  }

  // Set the colours to white by default
  for (int i=0;i<MAXTHEMEELEMENTS;++i) colours[i] = 0xFFFF;

  // Define the filter for reading the json
  JsonDocument filter;
  filter[0]["id"] = true;
  filter[0]["colour"] = true;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc,file,DeserializationOption::Filter(filter));
  file.close();

  if (error) {
    log_e("JSON deserialisation failed reading %s: %s", filename, error);
    return false;
  }

  // Iterate over the JSON array and populate the colours array
  JsonArray array = doc.as<JsonArray>();
  for (JsonObject item : array) {
    if (item["id"].is<uint16_t>() && item["colour"].is<const char*>()) {
      uint16_t id = item["id"];
      const char* hexStr = item["colour"];

      // Verify bounds check against MAXTHEMEELEMENTS
      if (id < MAXTHEMEELEMENTS) {
        // Convert HEX string to 16-bit integer (e.g. "FFFF" -> 0xFFFF)
        colours[id] = (uint16_t) strtoul(hexStr, nullptr, 16);
      } else {
        log_w("Warning: Colour theme %s contains id %u, ignored!",themeName,id);
      }
    }
  }

  return true;
}

/*
 * Graphics helper functions panel
*/

void setupMatrix() {

  HUB75_I2S_CFG::i2s_pins _pins = {
    R1_PIN, G1_PIN, B1_PIN,
    R2_PIN, G2_PIN, B2_PIN,
    A_PIN,  B_PIN,  C_PIN, D_PIN, E_PIN,
    LAT_PIN, OE_PIN, CLK_PIN
  };

  if (swapBG) {
      _pins.g1 = B1_PIN;
      _pins.b1 = G1_PIN;
      _pins.g2 = B2_PIN;
      _pins.b2 = G2_PIN;
  }

  HUB75_I2S_CFG mxconfig(
      PANEL_RES_X,   // Module width
      PANEL_RES_Y,   // Module height
      PANEL_CHAIN,   // Chain length
      _pins
  );

  // Change this if you see pixels showing up shifted wrongly by one column the left or right.
  mxconfig.clkphase = false;
  mxconfig.setPixelColorDepthBits(5);
  mxconfig.double_buff = false;
  if (mx6126) mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Instantiate and allocate resources
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) log_e("HUB75 memory allocation failed!!");
  dma_display->setBrightness(brightness);
  dma_display->clearScreen();

  // Create a virtual display 1 row, 3 columns of 64x32 panels
  panel = new VirtualMatrixPanel(
      *dma_display,
      1,              // rows
      3,              // columns
      PANEL_RES_X,
      PANEL_RES_Y,
      CHAIN_TOP_LEFT_DOWN
  );

  panel->setRotation(2);    // Rotate 180°
}

void setBoardFont(boardFonts fontId, U8G2_FOR_ADAFRUIT_GFX &u8g2Engine) {
  switch (fontId) {
    case RAIL9:
      u8g2Engine.setFont(NatRail9x5);
      FONT_HEIGHT = 9;
      break;
    case RAIL9W:
      u8g2Engine.setFont(NatRail9x5W);
      FONT_HEIGHT = 9;
      break;
    case TUBE10:
      u8g2Engine.setFont(Underground10);
      FONT_HEIGHT = 10;
      break;
    case TUBECLOCK8:
      u8g2Engine.setFont(UndergroundClock8);
      FONT_HEIGHT = 8;
      break;
    case RAILCLOCK9:
      u8g2Engine.setFont(NatRailClockLarge9);
      FONT_HEIGHT = 9;
      break;
    case RAILCLOCK7:
      u8g2Engine.setFont(NatRailClockSmall7);
      FONT_HEIGHT = 7;
      break;
  }
}

void sendBuffer() {
  panel->drawRGBBitmap(0, 0, main_canvas.getBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
}

void clearCanvas(GFXcanvas16 &theCanvas = main_canvas) {
  theCanvas.fillScreen(RGB565_BLACK);
}

void clearDisplay() {
  clearCanvas();
  sendBuffer();
}

void blankArea(int x, int y, int w, int h) {
  main_canvas.fillRect(x,y,w,h,RGB565_BLACK);
}

void pushClipAtLine(int y) {
  main_canvas.drawRGBBitmap(0, y, clip_canvas.getBuffer(), SCREEN_WIDTH, CLIP_HEIGHT);
}

int getStringWidth(const char *message, U8G2_FOR_ADAFRUIT_GFX &u8g2Engine = u8g2_clip) {
  return u8g2Engine.getUTF8Width(message);
}

void drawStr(int x, int y, uint16_t colour, const char *buff, boardFonts fontId, U8G2_FOR_ADAFRUIT_GFX &u8g2Engine = u8g2_clip) {
    u8g2Engine.setForegroundColor(colour);
    setBoardFont(fontId, u8g2Engine);
    u8g2Engine.setCursor(x,y+FONT_HEIGHT);
    u8g2Engine.print(buff);
}

void drawTruncatedText(const char *message, int y, int x, uint16_t colour, boardFonts fontId, U8G2_FOR_ADAFRUIT_GFX &u8g2Engine = u8g2_clip) {
  char buff[strlen(message)+4];
  int maxWidth = SCREEN_WIDTH - 6 - x;
  strcpy(buff,message);
  int i = strlen(buff);
  setBoardFont(fontId, u8g2Engine);
  while (u8g2Engine.getUTF8Width(buff)>maxWidth && i) buff[i--] = '\0';
  strcat(buff,".\x85"); // ...
  drawStr(x,y,colour,buff,fontId,u8g2Engine);
}

void centreText(const char *message, int y, uint16_t colour, boardFonts fontId, U8G2_FOR_ADAFRUIT_GFX &u8g2Engine = u8g2_clip, int margin=0, int maxWidth = SCREEN_WIDTH) {
  setBoardFont(fontId,u8g2Engine);
  int width = u8g2Engine.getUTF8Width(message);
  if (width<=maxWidth) drawStr(((maxWidth-width)/2)+margin,y,colour,message,fontId,u8g2Engine);
  else drawTruncatedText(message,y,0,colour,fontId,u8g2Engine);
}

void centreFmtText(const char *message, int y, uint16_t colour, boardFonts fontId, U8G2_FOR_ADAFRUIT_GFX &u8g2Engine = u8g2_clip, int margin=0, int maxWidth = SCREEN_WIDTH) {
  char plainMessage[strlen(message)+1];
  int p=0;
  bool firstCharacter = true;

  for (int i=0;i<strlen(message);++i) {
    if (message[i] == '~') i++;
    else plainMessage[p++] = message[i];
  }
  plainMessage[p] = '\0';

  setBoardFont(fontId,u8g2Engine);
  u8g2Engine.setForegroundColor(colour);
  int width = u8g2Engine.getUTF8Width(plainMessage);
  for (int i=0;i<strlen(message);++i) {
    if (message[i] == '~') {
      i++;
      switch (message[i]) {
        case 'w':
          u8g2Engine.setForegroundColor(RGB565_WHITE);
          break;
        case 'b':
          u8g2Engine.setForegroundColor(RGB565_LIGHTBLUE);
          break;
        case 'g':
          u8g2Engine.setForegroundColor(RGB565_GREEN);
          break;
        case 'r':
          u8g2Engine.setForegroundColor(RGB565_RED);
          break;
        case 'y':
          u8g2Engine.setForegroundColor(RGB565_YELLOW);
          break;
        case 'c':
          u8g2Engine.setForegroundColor(RGB565_CYAN);
          break;
        default:
          u8g2Engine.setForegroundColor(RGB565_WHITE);
          break;
      }
    } else {
      if (firstCharacter) {
        u8g2Engine.setCursor(((maxWidth-width)/2)+margin,y+FONT_HEIGHT);
        firstCharacter = false;
      }
      u8g2Engine.print(message[i]);
    }
  }
}

void drawProgressBar(int percent) {
  int newPosition = (percent*160)/100;

  clearCanvas(clip_canvas);
  clip_canvas.drawRect(15,0,162,CLIP_HEIGHT,RGB565_WHITE);
  if (prevProgressBarPosition>newPosition) {
    for (int i=prevProgressBarPosition;i>=newPosition;i--) {
      clip_canvas.fillRect(16,1,160,CLIP_HEIGHT-2,RGB565_BLACK);
      clip_canvas.fillRect(16,1,i,CLIP_HEIGHT-2,RGB565_BLUE);
      pushClipAtLine(LINE2);
      sendBuffer();
      delay(5);
    }
  } else {
    for (int i=prevProgressBarPosition;i<=newPosition;i++) {
      clip_canvas.fillRect(16,1,160,CLIP_HEIGHT-2,RGB565_BLACK);
      clip_canvas.fillRect(16,1,i,CLIP_HEIGHT-2,RGB565_BLUE);
      pushClipAtLine(LINE2);
      sendBuffer();
      delay(5);
    }
  }
  prevProgressBarPosition=newPosition;
}

void progressBar(const char *text, int percent) {
  blankArea(0,LINE1,SCREEN_WIDTH,CLIP_HEIGHT);
  centreText(text,LINE1,RGB565_WHITE,RAIL9,u8g2_main);
  drawProgressBar(percent);
}

void drawFirmware() {
  char firmware[16];
  sprintf(firmware,"B%d.%d-W%d.%d",VERSION_MAJOR,VERSION_MINOR,WEBAPPVER_MAJOR,WEBAPPVER_MINOR);
  drawStr(0,LINE3,RGB565_WHITE,firmware,RAIL9,u8g2_main);
}

void drawStartupHeading() {
  char ipBuff[17];
  if (!cardReaderReady) drawStr(SCREEN_WIDTH-48,LINE3,RGB565_RED,"No SD Card",RAIL9,u8g2_main);
  drawFirmware();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.localIP().toString().toCharArray(ipBuff,sizeof(ipBuff));   // Get the IP address of the ESP32
    centreText(ipBuff,LINE3,RGB565_WHITE,RAIL9,u8g2_main); // Display the IP address
  }
}

// Draw a 7-segment digit at x,y with height h
void draw7Segment8(int x, int y, int h, char digit, uint16_t colour) {
    int w = (h * 7) / 10;    // Width is 70% of height
    int t = (h * 15) / 100;  // Thickness of the segments

    // Calculate uniform gap sizing between segments
    int g = (h >= 40) ? (h / 40) : 1;
    int half_h = h / 2;
    int gh1 = g / 2;
    int gh2 = g - gh1;
    if (!gh1) gh1 = 1;

    // Helper lambda to draw a filled quadrilateral using two triangles.
    auto drawQuad = [&](int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
        main_canvas.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
        main_canvas.fillTriangle(x0, y0, x2, y2, x3, y3, colour);
    };

    // Top Segment
    if (strchr("02356789",digit) != nullptr)
        drawQuad(x + g, y,
             x + w - g, y,
             x + w - t - g, y + t,
             x + t + g, y + t);

    // Top-Right Segment
    if (strchr("01234789",digit) != nullptr)
      drawQuad(x + w, y + g,
             x + w, y + half_h - gh1,
             x + w - t, y + half_h - gh1,
             x + w - t, y + t + g);

    // Bottom-Right Segment
    if (strchr("013456789",digit) != nullptr)
      drawQuad(x + w, y + half_h + gh2,
             x + w, y + h - g,
             x + w - t, y + h - t - g,
             x + w - t, y + half_h + gh2);

    // Bottom Segment
    if (strchr("0235689",digit) != nullptr)
      drawQuad(x + g, y + h,
             x + w - g, y + h,
             x + w - t - g, y + h - t,
             x + t + g, y + h - t);

    // Bottom-Left Segment
    if (strchr("0268",digit) != nullptr)
      drawQuad(x, y + half_h + gh2,
             x + t, y + half_h + gh2,
             x + t, y + h - t - g,
             x, y + h - g);

    // Top-Left Segment
    if (strchr("045689",digit) != nullptr)
      drawQuad(x, y + g,
             x + t, y + t + g,
             x + t, y + half_h - gh1,
             x, y + half_h - gh1);

    // Middle Segment
    if (strchr("2345689",digit) != nullptr)
      drawQuad(x + t + g + 1, y + half_h - t / 2,
             x + w - t - g - 1, y + half_h - t / 2,
             x + w - t - g - 1, y + half_h + t / 2,
             x + t + g + 1, y + half_h + t / 2);
}

// Draws the full screen, Network SouthEast style clock
void drawNSEclock(uint16_t colour) {
  char clockdigits[7];

  sprintf(clockdigits,"%02d%02d%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  clearCanvas();
  draw7Segment8(10,0,32,clockdigits[0],colour);
  draw7Segment8(40,0,32,clockdigits[1],colour);
  draw7Segment8(79,0,32,clockdigits[2],colour);
  draw7Segment8(109,0,32,clockdigits[3],colour);
  draw7Segment8(147,12,19,clockdigits[4],colour);
  draw7Segment8(167,12,19,clockdigits[5],colour);
  main_canvas.fillEllipse(70,23,2,2,colour);
  main_canvas.fillEllipse(70,9,2,2,colour);
  main_canvas.fillEllipse(139,16,2,2,colour);

  sendBuffer();
}

void showSetupScreen() {
  clearCanvas();
  centreText("To configure WiFi, please connect to",LINE1,RGB565_WHITE,RAIL9,u8g2_main);
  centreFmtText("the ~y\"Departures Board\"~w network and",LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  centreFmtText("open ~bhttp://192.168.4.1~w in a browser.",LINE3,RGB565_WHITE,RAIL9,u8g2_main);
  sendBuffer();
}

void showNoDataScreen() {
  noServiceClockIsActive = false;
  clearCanvas();
  char msg[60];
  switch (boardMode) {
    case MODE_RAIL:
      sprintf(msg,"No data for station ~r\"%s\"~w.",locationCode);
      break;
    case MODE_TUBE:
      strcpy(msg,"No data for selected station.");
      break;
    case MODE_BUS:
      strcpy(msg,"No data for selected bus stop.");
      break;
  }
  centreFmtText(msg,LINE1,RGB565_WHITE,RAIL9,u8g2_main);
  centreText("Choose a valid location below:",LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  centreText(myUrl,LINE3,RGB565_LIGHTBLUE,RAIL9,u8g2_main);
  sendBuffer();
}

void showSetupKeysHelpScreen() {
  clearCanvas();
  char msg[60];
  centreText("Next, enter your API keys.",LINE1,RGB565_WHITE,RAIL9,u8g2_main);
  centreText("Please go to the URL below to start:",LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  centreText(myUrl,LINE3,RGB565_LIGHTBLUE,RAIL9,u8g2_main);
  sendBuffer();
}

void showSetupCrsHelpScreen() {
  clearCanvas();
  char msg[60];
  centreText("Next, you must choose a location.",LINE1,RGB565_WHITE,RAIL9,u8g2_main);
  centreText("Choose a station at the url below:",LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  centreText(myUrl,LINE3,RGB565_LIGHTBLUE,RAIL9,u8g2_main);
  sendBuffer();
}

void showWsdlFailureScreen() {
  clearCanvas();
  centreText("WDSL entry point not available!",LINE1,RGB565_WHITE,RAIL9,u8g2_main);
  centreText("Departures Board cannot be loaded.",LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  centreText("Please try again later. :(",LINE3,RGB565_WHITE,RAIL9,u8g2_main);
  sendBuffer();
}

void showTokenErrorScreen() {
  char msg[60];
  noServiceClockIsActive = false;
  clearCanvas();
  switch (boardMode) {
    case MODE_RAIL:
      if (useRDMclient) {
        centreText("Access to the RDG api denied.",LINE1,RGB565_RED,RAIL9,u8g2_main);
      } else {
        centreText("Access to NatRail database denied.",LINE1,RGB565_RED,RAIL9,u8g2_main);
        strcpy(nrToken,"");
      }
      break;
    case MODE_TUBE:
      centreText("Access to TfL denied.",LINE1,RGB565_RED,RAIL9,u8g2_main);
      break;
    case MODE_BUS:
      centreText("Access to bustimes denied.",LINE1,RGB565_RED,RAIL9,u8g2_main);
      break;
  }
  centreText("Enter a valid api key below:",LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  sprintf(msg,"%s/keys.htm",myUrl);
  centreText(msg,LINE3,RGB565_LIGHTBLUE,RAIL9,u8g2_main);
  sendBuffer();
}

void showCRSErrorScreen() {
  noServiceClockIsActive = false;
  clearCanvas();
  char msg[60];
  switch (boardMode) {
    case MODE_RAIL:
      sprintf(msg,"Station ~y\"%s\"~r is not valid.",locationCode);
      break;
    case MODE_TUBE:
      strcpy(msg,"Underground station not valid");
      break;
    case MODE_BUS:
      sprintf(msg,"Atco ~y\"%s\"~r is not valid.",locationCode);
      break;
  }
  centreFmtText(msg,LINE1,RGB565_RED,RAIL9,u8g2_main);
  centreText("Please selected a valid location",LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  sprintf(msg,"at ~b%s",myUrl);
  centreFmtText(msg,LINE3,RGB565_WHITE,RAIL9,u8g2_main);
  sendBuffer();
}

void showFirmwareUpdateWarningScreen(const char *msg) {
  char countdown[60];
  int x=SCREEN_WIDTH;
  int secs=30;
  unsigned long ticks,tocks;

  clearCanvas();
  centreText("Firmware Update Available",LINE1,RGB565_YELLOW,RAIL9,u8g2_main);
  int msgWidth = getStringWidth(msg);
  if (msgWidth < SCREEN_WIDTH) centreText(msg,LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  else {
    strcpy(rssMessage,msg);
    while (strlen(rssMessage) + strlen(msg) + 1 < MAXMESSAGESIZE) {
      strcat(rssMessage,"\x90");
      strcat(rssMessage,msg);
    }
    msgWidth = getStringWidth(rssMessage);
    secs=45;
  }
  while (secs>=0) {
    sprintf(countdown,"The update will begin in ~c%d~w seconds.",secs);
    blankArea(0,LINE3,SCREEN_WIDTH,FONT_HEIGHT);
    centreFmtText(countdown,LINE3,RGB565_WHITE,RAIL9,u8g2_main);
    if (msgWidth < SCREEN_WIDTH) {
      sendBuffer();
      delay(1000);
      secs--;
    } else {
      ticks = millis()+1000;
      while (millis()<ticks) {
        tocks = millis() + 40;
        blankArea(0,LINE2,SCREEN_WIDTH,FONT_HEIGHT);
        drawStr(x,LINE2,RGB565_WHITE,rssMessage,RAIL9,u8g2_main);
        sendBuffer();
        x--;
        if (x < -msgWidth) x=SCREEN_WIDTH;
        while (millis()<tocks) delay(1);
      }
      secs--;
    }
  }
}

void showFirmwareUpdateProgress(int percent) {
  //clearCanvas();
  centreText("* DO NOT REMOVE THE POWER *",LINE3+1,RGB565_RED,RAIL9,u8g2_main);
  sendBuffer();
  progressBar("Updating Firmware",percent);
}

void showUpdateCompleteScreen(const char *msg1, const char *msg2, int secs, bool showReboot) {
  char countdown[60];
  int x=SCREEN_WIDTH;
  int ttg=secs;
  unsigned long ticks,tocks;

  clearCanvas();
  centreText(msg1,LINE1,showReboot?RGB565_GREEN:RGB565_RED,RAIL9,u8g2_main);
  // sanitise msg2
  int p=0;
  for (int i=0;i<strlen(msg2);++i) {
    if (msg2[i]>=32) line2[0][p++]=msg2[i];
  }
  line2[0][p] = '\0';

  int msgWidth = getStringWidth(line2[0]);
  if (msgWidth < SCREEN_WIDTH) centreText(line2[0],LINE2,RGB565_WHITE,RAIL9,u8g2_main);
  else {
    strcpy(rssMessage,line2[0]);
    while (strlen(rssMessage) + strlen(line2[0]) + 1 < MAXMESSAGESIZE) {
      strcat(rssMessage,"\x90");
      strcat(rssMessage,line2[0]);
    }
    msgWidth = getStringWidth(rssMessage);
    ttg=(secs*15)/10;
  }
  while (ttg>=0) {
    if (showReboot) sprintf(countdown,"The board will restart in ~c%d~w seconds.",ttg);
    else sprintf(countdown,"The board will continue in ~c%d~w seconds.",ttg);
    blankArea(0,LINE3,SCREEN_WIDTH,FONT_HEIGHT);
    centreFmtText(countdown,LINE3,RGB565_WHITE,RAIL9,u8g2_main);
    if (msgWidth < SCREEN_WIDTH) {
      sendBuffer();
      delay(1000);
      ttg--;
    } else {
      ticks = millis()+1000;
      while (millis()<ticks) {
        tocks = millis() + 40;
        blankArea(0,LINE2,SCREEN_WIDTH,FONT_HEIGHT);
        drawStr(x,LINE2,RGB565_WHITE,rssMessage,RAIL9,u8g2_main);
        sendBuffer();
        x--;
        if (x < -msgWidth) x=SCREEN_WIDTH;
        while (millis()<tocks) delay(1);
      }
      ttg--;
    }
  }
}

void showSwitchScreen() {
  clearCanvas();
  if (carouselActive) centreText("Moving to next carousel slot",LINE1+5,RGB565_WHITE,RAIL9,u8g2_main);
  else if (schedulerActive) centreText("Moving to next scheduler slot",LINE1+5,RGB565_WHITE,RAIL9,u8g2_main);
  else centreText("Switching modes",LINE1+5,RGB565_WHITE,RAIL9,u8g2_main);
  centreText("Waiting for background processes",LINE3,RGB565_WHITE,RAIL9,u8g2_main);
  sendBuffer();
}

/*
 * Utility functions
*/

// Saves a file (string) to the FFS
bool saveFile(String fName, String fData) {
  File f = LittleFS.open(fName,"w");
  if (f) {
    f.println(fData);
    f.close();
    return true;
  } else return false;
}

// Loads a file (string) from the FFS
String loadFile(String fName) {
  File f = LittleFS.open(fName,"r");
  if (f) {
    String result = f.readString();
    f.close();
    return result;
  } else return "";
}

// Get the Build Timestamp of the running firmware
String getBuildTime() {
  char timestamp[22];
  char buildtime[11];
  struct tm tm = {};

  sprintf(timestamp,"%s %s",__DATE__,__TIME__);
  strptime(timestamp,"%b %d %Y %H:%M:%S",&tm);
  sprintf(buildtime,"%02d%02d%02d%02d%02d",tm.tm_year-100,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min);
  return String(buildtime);
}

void checkPostWebUpgrade() {
  JsonDocument doc;
  char prevFirmware[15] = "B0.0-W0.0";
  char prevGUI[8];
  char currentGUI[8];

  if (LittleFS.exists("/fw.json")) {
    File file = LittleFS.open("/fw.json", "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings["fw"].is<const char*>()) {
          strlcpy(prevFirmware,settings["fw"],sizeof(prevFirmware));
        }
      }
      file.close();
    }
  }

  if (prevFirmware[0]) {
    sscanf(prevFirmware,"%*[^ -]-%s",prevGUI);
    sprintf(currentGUI,"W%d.%d",WEBAPPVER_MAJOR,WEBAPPVER_MINOR);
    if (strcmp(prevGUI,currentGUI)) {
      // clean up old/dev files
      progressBar("Cleaning up following upgrade",45);
      LittleFS.remove("/index_d.htm");
      LittleFS.remove("/index.htm");
      LittleFS.remove("/keys.htm");
      LittleFS.remove("/nrelogo.webp");
      LittleFS.remove("/rdglogo.webp");
      LittleFS.remove("/tfllogo.webp");
      LittleFS.remove("/btlogo.webp");
      LittleFS.remove("/tube.webp");
      LittleFS.remove("/nr.webp");
      LittleFS.remove("/favicon.svg");
      LittleFS.remove("/favicon.png");
    }
  }
}

// Returns true if sleep mode is enabled and we're within the sleep period
bool isSnoozing() {
  if (forcedSleep || NSEclockIsActive) return true;
  if (forcedAwake) {
    if (button.secsSinceLastTap() >= stayAwakeSeconds) forcedAwake = false;
    else return false;
  }
  if (!sleepEnabled) return false;
  byte myHour = timeinfo.tm_hour;
  if (sleepStarts > sleepEnds) {
    if ((myHour >= sleepStarts) || (myHour < sleepEnds)) return true; else return false;
  } else {
    if ((myHour >= sleepStarts) && (myHour < sleepEnds)) return true; else return false;
  }
}

// Stores/updates the url of our Web GUI
void updateMyUrl() {
  IPAddress ip = WiFi.localIP();
  snprintf(myUrl,sizeof(myUrl),"http://%u.%u.%u.%u",ip[0],ip[1],ip[2],ip[3]);
}

/*
 * Start-up configuration functions
 */

// Load the API keys from the file system (if they exist)
void loadApiKeys() {
  JsonDocument doc;

  if (LittleFS.exists("/apikeys.json")) {
    File file = LittleFS.open("/apikeys.json", "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        if (settings["rdmDepKey"].is<const char*>()) {
          rdmDeparturesApiKey = settings["rdmDepKey"].as<String>();
        }

        if (settings["rdmSvcKey"].is<const char*>()) {
          rdmServiceApiKey = settings["rdmSvcKey"].as<String>();
        }

        if (settings["nrToken"].is<const char*>()) {
          strlcpy(nrToken, settings["nrToken"], sizeof(nrToken));
        }

        if (settings["owmToken"].is<const char*>()) {
          strlcpy(openWeatherMapApiKey, settings["owmToken"], OWMKEYSIZE);
        }

        if (settings["appKey"].is<const char*>()) {
          strlcpy(tflAppKey,settings["appKey"],sizeof(tflAppKey));
        }
        apiKeys = true;
      }
      file.close();
    }
  }
}

void resetLocationIds() {
  strcpy(locationCode,"");
  railIsSet = false;
  tubeIsSet = false;
  busIsSet = false;
}

void saveFirmwareInfo() {
  String fw = "{\"fw\":\"B" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "-W" + String(WEBAPPVER_MAJOR) + "." + String(WEBAPPVER_MINOR) + "\"}";
  saveFile("/fw.json",fw);
}

// Write a default config file so that the Web GUI works initially (force Tube mode if no NR token)
void writeDefaultConfig() {
  String defaultConfig = "{\"crs\":\"\",\"station\":\"\",\"lat\":0,\"lon\":0,\"weather\":true,\"sleep\":false,\"showBus\":false,\"update\":true,\"sleepStarts\":23,\"sleepEnds\":8,\"brightness\":20,\"volume\":152,\"theme\":\"ct_Default\",\"fastRefresh\":true,\"tubeId\":\"\",\"tubeName\":\"\",\"mode\":" + String((!nrToken[0] && rdmDeparturesApiKey=="")?"1":"0") + "}";
  saveFile("/config.json",defaultConfig);
  resetLocationIds();
  saveFirmwareInfo();
}

bool pruneFromPhrase(char* input, const char* target) {
  // Find the first occurance of the target word or phrase
  char* pos = strstr(input,target);
  // If found, prune from here
  if (pos) {
      input[pos - input] = '\0';
      return true;
  }
  return false;
}

int getTimeInMinutes() {
  return (timeinfo.tm_hour * 60 + timeinfo.tm_min);
}

void loadSlot(JsonObjectConst slot, bool isDefault, boardModes requestedMode) {
  if (requestedMode == MODE_NEXTMODE) {
    switch (boardMode) {
      case MODE_RAIL:
        if (tubeIsSet) boardMode = MODE_TUBE;
        else if (busIsSet) boardMode = MODE_BUS;
        break;
      case MODE_TUBE:
        if (busIsSet) boardMode = MODE_BUS;
        else if (railIsSet) boardMode = MODE_RAIL;
        break;
      case MODE_BUS:
        if (railIsSet) boardMode = MODE_RAIL;
        else if (tubeIsSet) boardMode = MODE_TUBE;
        break;
    }
  } else if (slot["mode"].is<int>()) boardMode = slot["mode"];

  switch (boardMode) {
    case MODE_RAIL:
      if (slot["crs"].is<const char*>())              strlcpy(locationCode, slot["crs"], MAXLOCATIONCODESIZE);
      if (slot["platformFilter"].is<const char*>())   strlcpy(locationFilter, slot["platformFilter"], MAXFILTERSIZE);
      if (slot["callingCrs"].is<const char*>())       strlcpy(callingCrsCode, slot["callingCrs"], sizeof(callingCrsCode));
      if (slot["callingStation"].is<const char*>())   strlcpy(callingStation, slot["callingStation"], sizeof(callingStation));
      if (slot["lat"].is<float>())                    locationLat = slot["lat"];
      if (slot["lon"].is<float>())                    locationLon = slot["lon"];
      if (isDefault) {
        if (slot["station"].is<const char*>())        strlcpy(locationName, slot["station"], MAXLOCATIONSIZE);
      } else {
        if (slot["name"].is<const char*>())           strlcpy(locationName, slot["name"], MAXLOCATIONSIZE);
      }
      break;

    case MODE_TUBE:
      if (slot["tubeId"].is<const char*>())     strlcpy(locationCode, slot["tubeId"], MAXLOCATIONCODESIZE);
      if (slot["lineid"].is<const char*>())     strlcpy(lineId, slot["lineid"], sizeof(lineId));
      if (slot["direction"].is<const char*>())  strlcpy(lineDirection, slot["direction"], sizeof(lineDirection));
      if (isDefault) {
        if (slot["tubeName"].is<const char*>()) strlcpy(locationName, slot["tubeName"], MAXLOCATIONSIZE);
        if (slot["tubeLat"].is<float>())      locationLat = slot["tubeLat"];
        if (slot["tubeLon"].is<float>())      locationLon = slot["tubeLon"];
        pruneFromPhrase(locationName," Underground Station");
        pruneFromPhrase(locationName," DLR Station");
        pruneFromPhrase(locationName," (H&C Line)");
      } else {
        if (slot["name"].is<const char*>()) strlcpy(locationName, slot["name"], MAXLOCATIONSIZE);
        if (slot["lat"].is<float>())          locationLat = slot["lat"];
        if (slot["lon"].is<float>())          locationLon = slot["lon"];
      }
      break;

    case MODE_BUS:
      if (slot["busId"].is<const char*>())      strlcpy(locationCode, slot["busId"], MAXLOCATIONCODESIZE);
      if (slot["busFilter"].is<const char*>())  strlcpy(locationFilter, slot["busFilter"], MAXFILTERSIZE);
      if (isDefault) {
        if (slot["busName"].is<const char*>())    strlcpy(locationName, slot["busName"], MAXLOCATIONSIZE);
        if (slot["busLat"].is<float>())           locationLat = slot["busLat"];
        if (slot["busLon"].is<float>())           locationLon = slot["busLon"];
      } else {
        if (slot["name"].is<const char*>())   strlcpy(locationName, slot["name"], MAXLOCATIONSIZE);
        if (slot["lat"].is<float>())          locationLat = slot["lat"];
        if (slot["lon"].is<float>())          locationLon = slot["lon"];
      }
      break;

  }
}

// Load the configuration settings (if they exist, if not create a default set for the Web GUI page to read)
void loadConfig(bool coldBoot = false, boardModes requestedMode = MODE_LOADCONFIG) {
  JsonDocument doc;

  // Set defaults
  strcpy(hostname,defaultHostname);
  strcpy(lineId,"all");
  strcpy(lineDirection,"");

  timezone = String(ukTimezone);
  resetLocationIds();

  schedulerActive = false;
  carouselActive = false;

  if (LittleFS.exists("/config.json")) {
    File file = LittleFS.open("/config.json", "r");
    if (file) {
      DeserializationError error = deserializeJson(doc, file);
      file.close();
      if (!error) {
        JsonObject settings = doc.as<JsonObject>();

        // Load common settings
        if (settings["crs"].is<const char*>() && strlen(settings["crs"])) railIsSet = true; else railIsSet = false;
        if (settings["tubeId"].is<const char*>() && strlen(settings["tubeId"])) tubeIsSet = true; else tubeIsSet = false;
        if (settings["busId"].is<const char*>() && strlen(settings["busId"])) busIsSet = true; else busIsSet = false;

        if (settings["hostname"].is<const char*>())   strlcpy(hostname, settings["hostname"], sizeof(hostname));
        if (settings["wsdlHost"].is<const char*>())   strlcpy(wsdlHost, settings["wsdlHost"], sizeof(wsdlHost));
        if (settings["wsdlAPI"].is<const char*>())    strlcpy(wsdlAPI, settings["wsdlAPI"], sizeof(wsdlAPI));
        if (settings["enableAudio"].is<bool>())       enableAudioAnnouncements = settings["enableAudio"];
        if (settings["enableReverb"].is<bool>())      enableReverb = settings["enableReverb"];
        if (settings["enableAudioCalling"].is<bool>()) enableAudioCallingList = settings["enableAudioCalling"];
        if (settings["voice"].is<const char*>())      strlcpy(audioVoice, settings["voice"], sizeof(audioVoice));
        if (settings["volume"].is<int>())             audioVolume = settings["volume"];
        if (settings["showBus"].is<bool>())           enableBus = settings["showBus"];
        if (settings["showLocation"].is<bool>())      showStationName = settings["showLocation"];
        if (settings["showFullCalling"].is<bool>())   showFullCalling = settings["showFullCalling"];
        if (settings["showFullMsgs"].is<bool>())      showFullMsgs = settings["showFullMsgs"];
        if (settings["showClockNoServices"].is<bool>()) showClockNoServices = settings["showClockNoServices"];
        if (settings["sleep"].is<bool>())             sleepEnabled = settings["sleep"];
        if (settings["fastRefresh"].is<bool>())       apiRefreshRate = settings["fastRefresh"] ? FASTDATAUPDATEINTERVAL : DATAUPDATEINTERVAL;
        if (settings["weather"].is<bool>())           weatherEnabled = settings["weather"];
        if (settings["update"].is<bool>())            firmwareUpdates = settings["update"];
        if (settings["updateDaily"].is<bool>())       dailyUpdateCheck = settings["updateDaily"];
        if (settings["sleepStarts"].is<int>())        sleepStarts = settings["sleepStarts"];
        if (settings["sleepEnds"].is<int>())          sleepEnds = settings["sleepEnds"];
        if (settings["brightness"].is<int>())         brightness = settings["brightness"];
        if (settings["longPressClock"].is<bool>())    longPressClock = settings["longPressClock"];
        if (settings["sleepUseBigClock"].is<bool>())  useNSEclockForSleep = (settings["sleepUseBigClock"]);

        if (settings["noScroll"].is<bool>())          noScrolling = settings["noScroll"];
        if (settings["flip"].is<bool>())              flipScreen = settings["flip"];
        if (settings["swapBG"].is<bool>())            swapBG = settings["swapBG"];
        if (settings["mx6126"].is<bool>())            mx6126 = settings["mx6126"];
        if (settings["ftp"].is<bool>())               enableFTP = settings["ftp"];
        if (settings["touch"].is<bool>())             touchEnabled = settings["touch"];
        if (settings["forceWakeTime"].is<int>())      stayAwakeSeconds = settings["forceWakeTime"];
        if (settings["TZ"].is<const char*>())         timezone = settings["TZ"].as<String>();
        if (settings["nrTimeOffset"].is<int>())       nrTimeOffset = settings["nrTimeOffset"];
        if (settings["hidePlatform"].is<bool>())      hidePlatform = settings["hidePlatform"];
        if (settings["hideOrdinals"].is<bool>())      hideOrdinals = settings["hideOrdinals"];
        if (settings["showLastSeen"].is<bool>())      showLastSeen = settings["showLastSeen"];
        if (settings["showTubeLocation"].is<bool>())  showTubeCurrentLocation = settings["showTubeLocation"];
        if (settings["showServiceMsgs"].is<bool>())   showServiceMsgs = settings["showServiceMsgs"];

        if (settings["enableScheduler"].is<bool>())   enableScheduler = settings["enableScheduler"];
        if (settings["enableCarousel"].is<bool>())    enableCarousel = settings["enableCarousel"];

        if (settings["rssUrl"].is<const char*>())     rssURL = settings["rssUrl"].as<String>();
        if (settings["rssName"].is<const char*>())    rssName = settings["rssName"].as<String>();
        if (rssURL != "") rssEnabled = true; else rssEnabled = false;
        if (settings["rssPriority"].is<bool>())       rssPriority = settings["rssPriority"];

        if (requestedMode != MODE_NEXTMODE && settings["mode"].is<int>()) boardMode = settings["mode"];

        if (settings["dataSource"].is<int>())         useRDMclient = (settings["dataSource"]?1:0);
        // validate the data source against which api keys are available
        if (nrToken[0] && rdmDeparturesApiKey=="") useRDMclient = false;
        else if (!nrToken[0] && rdmDeparturesApiKey!="") useRDMclient = true;

        if (coldBoot) {
          // Just load base parameters at boot, clock not set yet so exit
          return;
        }

        if (settings["theme"].is<const char*>()) loadTheme(settings["theme"]);
        if (!cardReaderReady) {
          enableAudioAnnouncements = false; // No SD card so no audio
          enableFTP = false;  // No CD card so no FTP server
        }

        // Work out what board mode we're in
        JsonArray scheduler = settings["scheduler"].as<JsonArray>();
        JsonArray carousel = settings["carousel"].as<JsonArray>();

        if (enableScheduler && !scheduler.isNull() && scheduler.size() > 0 && (requestedMode==MODE_LOADCONFIG || requestedMode==MODE_NEXTSCHEDULE)) {
          if (requestedMode == MODE_LOADCONFIG) {
            int currentMins = getTimeInMinutes();
            int activeIndex = -1;

            // Iterate through the sorted schedule to find the last entry that has already started
            for (int i = 0; i < scheduler.size(); i++) {
              const char* timeStr = scheduler[i]["time"];
              if (timeStr) {
                int h, m;
                if (sscanf(timeStr, "%d:%d", &h, &m) == 2) {
                  int entryMins = h * 60 + m;
                  if (entryMins <= currentMins) {
                    activeIndex = i;
                  } else {
                    // Since the array is chronologically sorted, the first entry strictly
                    // greater than the current time implies we've passed the active one.
                    break;
                  }
                }
              }
            }

            // If activeIndex is still -1, it means the current time is before the first entry
            // of the day. Because of midnight wrap-around, the active entry is the LAST entry
            // from the previous day.
            if (activeIndex == -1) {
              activeIndex = scheduler.size() - 1;
            }

            // Save the active entry index (for touch)
            currentScheduleSlot = activeIndex;
            numScheduleSlots = scheduler.size();

            // The next entry simply follows the active one, wrapping back to 0 at the end of the array
            int nextIndex = (activeIndex + 1) % scheduler.size();

            JsonObject currentSchedule = scheduler[activeIndex];
            loadSlot(currentSchedule,false,requestedMode);
            schedulerActive = true;

            // Save the time of the active entry
            const char* timeStrActive = scheduler[activeIndex]["time"];
            if (timeStrActive) {
              int h, m;
              if (sscanf(timeStrActive, "%d:%d", &h, &m) == 2) {
                activeSlotEventTime = h * 60 + m; // The time (in minutes) of the active event
              }
            }

            // Save the time of the next entry (for the loop scheduler)
            const char* timeStr = scheduler[nextIndex]["time"];
            if (timeStr) {
              int h, m;
              if (sscanf(timeStr, "%d:%d", &h, &m) == 2) {
                nextSlotEventTime = h * 60 + m; // The time (in minutes) of the next change
              }
            }
          } else {
            // Push on to the next schedule slot via touch
            currentScheduleSlot = (currentScheduleSlot + 1) % numScheduleSlots;
            JsonObject currentSchedule = scheduler[currentScheduleSlot];
            loadSlot(currentSchedule,false,requestedMode);
            schedulerActive = true;
          }
        } else if (enableCarousel && !carousel.isNull() && carousel.size() > 0 && requestedMode == MODE_LOADCONFIG) {
          numCarouselSlots = carousel.size();
          if (currentCarouselSlot >= numCarouselSlots) currentCarouselSlot = 0;
          JsonObject currentCarousel = carousel[currentCarouselSlot];
          loadSlot(currentCarousel,false,requestedMode);
          carouselActive = true;

          // Work out when the next change occurs
          activeSlotEventTime = getTimeInMinutes();
          if (currentCarousel["duration"].is<int>()) nextSlotEventTime = currentCarousel["duration"];
          nextSlotEventTime = (activeSlotEventTime + nextSlotEventTime) % 1440;

        } else {
          // Plain board mode as defined by the user
          loadSlot(settings,true,requestedMode);
        }
      }
    }
  } else if (apiKeys) writeDefaultConfig();
}

void buildRssMessage() {
  if (rss->numRssTitles>0) {
    sprintf(rssMessage,"%s: %s",rssName.c_str(),rss->rssTitle[0]);
    for (int i=1;i<rss->numRssTitles;i++) {
      if (strlen(rssMessage) + strlen(rss->rssTitle[i]) + 1 < MAXMESSAGESIZE) {
        strcat(rssMessage,"\x90");
        strcat(rssMessage,rss->rssTitle[i]);
      } else {
        break;
      }
    }
  } else {
    rssMessage[0] = '\0';
  }
}

void updateRssFeed() {
  if (lastRssUpdateResult=rss->loadFeed(rssURL); lastRssUpdateResult == UPD_SUCCESS) {
    nextRssUpdate = millis() + RSSUPDATEINTERVAL; // update every ten minutes
    buildRssMessage();
  }
  else nextRssUpdate = millis() + (RSSUPDATEINTERVAL/2); // Failed so try again in 5 minutes
}

// Update the current weather message if weather updates are enabled and we have a lat/lon for the selected location
void updateCurrentWeather(float latitude, float longitude) {
  nextWeatherUpdate = millis() + WEATHERUPDATEINTERVAL;
  if (!latitude || !longitude) return; // No location co-ordinates
  weatherMsg[0]='\0';
  lastWeatherUpdateResult = currentWeather->updateWeather(openWeatherMapApiKey, latitude, longitude);
  if (lastWeatherUpdateResult == UPD_SUCCESS) strlcpy(weatherMsg,currentWeather->currentWeatherMessage,MAXWEATHERSIZE);
}

void checkWeatherUpdate(float prevLat, float prevLon) {
  if (weatherEnabled && (prevLat!=locationLat || prevLon!=locationLon)) {
    prevProgressBarPosition = 80;
    drawProgressBar(60);
    updateCurrentWeather(locationLat,locationLon);
  }
}

// Soft reset/reload the board.
void softResetBoard(boardModes requestedMode) {
  boardModes previousMode = boardMode;
  String prevRssUrl = rssURL;
  float prevLat = locationLat;
  float prevLon = locationLon;
  bool prevWeatherEnabled = weatherEnabled;

  // Reload the settings
  loadConfig(false,requestedMode);
  if (flipScreen) panel->setRotation(0); else panel->setRotation(2);
  if (timezone!="") {
    setenv("TZ",timezone.c_str(),1);
  } else {
    setenv("TZ",ukTimezone,1);
  }
  tzset();
  if (enableAudioAnnouncements) {
    // Clear the audio queue
    xQueueReset(audioQueue);
    // set the volume
    es.WriteReg(0x32,audioVolume);
    resetTubeAudioHistory();
  }

  clearCanvas();
  drawStartupHeading();
  sendBuffer();

  // Force an update asap
  nextDataUpdate = 0;
  nextWeatherUpdate = millis()+60000; // Ensure the weather is updated after the data feed
  nextRssUpdate += 30000;
  isScrollingService = false;
  isScrollingStops = false;
  isScrollingPrimary = false;
  noServiceClockIsActive = false;
  isSleeping = false;
  forcedSleep = false;
  firstLoad = true;
  noDataLoaded = true;
  timeIsDisplayed = false;
  viaTimer = 0;
  timer = 0;
  serviceTimer = 0;
  prevProgressBarPosition = 100;
  startupProgressPercent = 70;
  currentMessage = 0;
  prevMessage = 0;
  prevScrollStopsLength = 0;
  isShowingVia = false;
  line3Service = 0;
  prevService = -2;
  fetchComplete = false;
  currentServiceId[0] = '\0';
  nextSchedulerCheck = millis()+10000;
  if (!weatherEnabled) weatherMsg[0] = '\0';
  else if (!prevWeatherEnabled) {
    // force a weather update, even if the location hasn't changed
    prevLat = 0;
    prevLon = 0;
  }

  setBoardFont(stationNameFont = RAIL9W, u8g2_clip);
  if (getStringWidth(locationName) > SCREEN_WIDTH) stationNameFont = RAIL9;
  centreText(locationName,LINE1,RGB565_WHITE,stationNameFont,u8g2_main);
  sendBuffer();

  if (rssEnabled && prevRssUrl != rssURL) {
    rssMessage[0] = '\0';
    if (boardMode == MODE_RAIL || boardMode == MODE_TUBE) {
      prevProgressBarPosition = 60;
      drawProgressBar(50);
      updateRssFeed();
    }
  } else if (rssEnabled && previousMode != boardMode && boardMode != MODE_BUS) {
    buildRssMessage();
  }

  switch (boardMode) {
    case MODE_RAIL:
      checkWeatherUpdate(prevLat,prevLon);
      // Create a cleaned platform filter (if any)
      rdmRailData->cleanFilter(locationFilter,locationCleanFilter,MAXFILTERSIZE);
      drawProgressBar(70);
      if (!useRDMclient) {
        // Using legacy XML client
        int res = darwinRailData->init(wsdlHost, wsdlAPI);
        if (res != UPD_SUCCESS) {
          showWsdlFailureScreen();
          while (true) { delay(1);}
        }
      }
      break;

    case MODE_TUBE:
      checkWeatherUpdate(prevLat,prevLon);
      drawProgressBar(70);
      break;

    case MODE_BUS:
      checkWeatherUpdate(prevLat,prevLon);
      drawProgressBar(70);
      // Create a cleaned filter
      busdata->cleanFilter(locationFilter,locationCleanFilter,MAXFILTERSIZE);
      break;
  }
  station->numServices=0;
  messages->numMessages=0;
}

// Handle switching to next board mode or carousel/scheduler slot (touch sensor)
void switchToNextMode() {
  if ((carouselActive && numCarouselSlots<2) || (schedulerActive && numScheduleSlots<2)) return;  // Nothing to switch to

  if (fetchInProgress) {
    // Wait for the background fetch to finish before we soft reset
    showSwitchScreen();
    while (fetchInProgress) delay(50);
  }

  if (carouselActive) {
    currentCarouselSlot = (currentCarouselSlot + 1) % numCarouselSlots;
    softResetBoard(MODE_LOADCONFIG);
  }
  else if (schedulerActive) softResetBoard(MODE_NEXTSCHEDULE);
  else if (railIsSet+tubeIsSet+busIsSet > 1) softResetBoard(MODE_NEXTMODE); // Check there's at least two configured modes
}

// WiFiManager callback, entered config mode
void wmConfigModeCallback (WiFiManager *myWiFiManager) {
  showSetupScreen();
  wifiConfigured = true;
}

/*
 * Firmware / Web GUI Update functions
*/
bool isFirmwareUpdateAvailable() {
  int releaseMajor = ghUpdate->releaseId.substring(1,ghUpdate->releaseId.indexOf(".")).toInt();
  int releaseMinor = ghUpdate->releaseId.substring(ghUpdate->releaseId.indexOf(".")+1,ghUpdate->releaseId.indexOf("-")).toInt();
  if (VERSION_MAJOR > releaseMajor) return false;
  if ((VERSION_MAJOR == releaseMajor) && (VERSION_MINOR >= releaseMinor)) return false;
  return true;
}

// Callback function for displaying firmware update progress
void update_progress(int cur, int total) {
  int percent = ((cur * 100)/total);
  showFirmwareUpdateProgress(percent);
}

// Attempts to install newer firmware if available
bool checkForFirmwareUpdate() {
  bool result = true;

  if (!isFirmwareUpdateAvailable()) return result;

  // Check that we found the firmware.bin file in the release assets
  if (ghUpdate->firmwareURL.length()==0) return result;

  showFirmwareUpdateWarningScreen(ghUpdate->releaseDescription.c_str());
  clearCanvas();
  prevProgressBarPosition=0;
  showFirmwareUpdateProgress(0);  // So we don't have a blank screen
  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.onProgress(update_progress);
  httpUpdate.rebootOnUpdate(false); // Don't auto reboot, we'll handle it

  HTTPUpdateResult ret = httpUpdate.handleUpdate(client, ghUpdate->firmwareURL);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      char msg[300];
      snprintf(msg,sizeof(msg),"The update failed, error %d (%s).",httpUpdate.getLastError(), httpUpdate.getLastErrorString());
      result=false;
      showUpdateCompleteScreen("Firmware Update Failed",msg,30,false);
      break;

    case HTTP_UPDATE_NO_UPDATES:
      showUpdateCompleteScreen("Firmware Update","No updates were available.",10,false);
      break;

    case HTTP_UPDATE_OK:
      showUpdateCompleteScreen("Firmware Updated Successfully","The changelog will be shown next time you access the web configuration.",20,true);
      ESP.restart();
      break;
  }
  clearCanvas();
  drawStartupHeading();
  sendBuffer();
  return result;
}

/*
 * Web GUI functions
 */

// Helper function for returning text status messages
void sendResponse(int code, String msg, AsyncWebServerRequest *request) {
  request->send(code,contentTypeText,msg);
}

// Return the correct MIME type for a file name
String getContentType(String filename) {
  if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".txt")) {
    return "text/plain";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".json")) {
    return "application/json";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  } else if (filename.endsWith(".svg")) {
    return "image/svg+xml";
  } else if (filename.endsWith(".webp")) {
    return "image/webp";
  }
  return "text/html";
}

// Stream a file from the file system
bool handleStreamFile(String filename, AsyncWebServerRequest *request) {
  if (LittleFS.exists(filename)) {
    String contentType = getContentType(filename);
    request->send(LittleFS,filename,contentType);
    return true;
  } else return false;
}

// Stream a file stored in flash
void handleStreamFlashFile(String filename, const uint8_t *filedata, size_t contentLength, AsyncWebServerRequest *request) {
  String contentType = getContentType(filename);
  AsyncWebServerResponse *response = request->beginResponse(200, contentType, filedata, contentLength);
  response->addHeader("Cache-Control", "public,max-age=3600,s-maxage=3600");
  request->send(response);
}

void handleStreamGzipFlashFile(String filename, const uint8_t *filedata, size_t contentLength, AsyncWebServerRequest *request) {
  String contentType = getContentType(filename);
  AsyncWebServerResponse *response = request->beginResponse(200, contentType, filedata, contentLength);
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
}

void handleStreamSystemTheme(const uint16_t *theme, AsyncWebServerRequest *request) {
  char hexValue[5];
  String resp = "[";

  for (int i=0;i<MAXTHEMEELEMENTS;++i) {
    if (i) resp+=",\n";
    sprintf(hexValue,"%04X",theme[i]);
    resp+="{\"name\":\"" + String(themeTags[i]) + "\",\"colour\":\"" + String(hexValue) + "\",\"id\":" + String(i) + "}";
  }

  resp+="]";
  request->send(200,contentTypeJson,resp);
}

/*
 * Expose the file system via the Web GUI with some basic functions for directory browsing, file reading and deletion.
 */

// Return storage information
String getFSInfo() {
  char info[70];

  sprintf(info,"Total: %d bytes, Used: %d bytes\n",LittleFS.totalBytes(), LittleFS.usedBytes());
  return String(info);
}

// Send a basic directory listing to the browser
void handleFileList(AsyncWebServerRequest *request) {
  String path;
  if (!request->hasParam("dir")) path="/"; else path = request->getParam("dir")->value();
  File root = LittleFS.open(path);

  String output="<html><body style=\"font-family:Helvetica,Arial,sans-serif\"><h2>Departures Board File System</h2>";
  if (!root) {
    output+="<p>Failed to open directory</p>";
  } else if (!root.isDirectory()) {
    output+="<p>Not a directory</p>";
  } else {
    output+="<table>";
    File file = root.openNextFile();
    while (file) {
      output+="<tr><td>";
      if (file.isDirectory()) {
        output+="[DIR]</td><td><a href=\"/rmdir?f=" + String(file.path()) + "\" title=\"Delete\">X</a></td><td><a href=\"/dir?dir=" + String(file.path()) + "\">" + String(file.name()) + "</a></td></tr>";
      } else {
        output+=String(file.size()) + "</td><td><a href=\"/del?f="+ String(file.path()) + "\" title=\"Delete\">X</a></td><td><a href=\"/cat?f=" + String(file.path()) + "\">" + String(file.name()) + "</a></td></tr>";
      }
      file = root.openNextFile();
    }
  }

  output += "</table><br>";
  output += getFSInfo() + "<p><a href=\"/upload\">Upload</a> a file</p></body></html>";
  request->send(200,contentTypeHtml,output);
}

// Stream a file to the browser
void handleCat(AsyncWebServerRequest *request) {
  if (request->hasParam("f")) {
    String filename = request->getParam("f")->value();
    handleStreamFile(filename,request);
  } else sendResponse(404,"Not found",request);
}

// Delete a file from the file system
void handleDelete(AsyncWebServerRequest *request) {
  if (request->hasParam("f")) {
    String filename = request->getParam("f")->value();
    if (LittleFS.remove(filename)) {
      // Successfully removed go back to directory listing
      request->redirect("/dir");
    } else sendResponse(400,"Failed to delete file",request);
  } else sendResponse(404,"Not found",request);
}

// Format the file system
void handleFormatFFS(AsyncWebServerRequest *request) {
  String message;

  if (LittleFS.format()) {
    message="File System was successfully formatted\n\n";
    message+=getFSInfo();
  } else message="File System could not be formatted!";
  sendResponse(200,message,request);
}

/*
 * Web GUI handlers
 */

// Fallback function for browser requests
void handleNotFound(AsyncWebServerRequest *request) {
  if ((LittleFS.exists(request->url())) && (request->method() == HTTP_GET)) handleStreamFile(request->url(),request);
  else if (request->url() == "/keys.htm") handleStreamGzipFlashFile(request->url(), keyshtm, sizeof(keyshtm),request);
  else if (request->url() == "/index.htm") handleStreamGzipFlashFile(request->url(), indexhtm, sizeof(indexhtm),request);
  else if (request->url() == "/editrss.htm") handleStreamGzipFlashFile(request->url(), editrsshtm, sizeof(editrsshtm),request);
  else if (request->url() == "/nrelogo.webp") handleStreamFlashFile(request->url(), nrelogo, sizeof(nrelogo),request);
  else if (request->url() == "/rdglogo.webp") handleStreamFlashFile(request->url(), rdglogo, sizeof(nrelogo),request);
  else if (request->url() == "/tfllogo.webp") handleStreamFlashFile(request->url(), tfllogo, sizeof(tfllogo),request);
  else if (request->url() == "/btlogo.webp") handleStreamFlashFile(request->url(), btlogo, sizeof(btlogo),request);
  else if (request->url() == "/tube.webp") handleStreamFlashFile(request->url(), tubeicon, sizeof(tubeicon),request);
  else if (request->url() == "/nr.webp") handleStreamFlashFile(request->url(), nricon, sizeof(nricon),request);
  else if (request->url() == "/ibus.webp") handleStreamFlashFile(request->url(), ibus, sizeof(ibus),request);
  else if (request->url() == "/irail.webp") handleStreamFlashFile(request->url(), irail, sizeof(irail),request);
  else if (request->url() == "/itube.webp") handleStreamFlashFile(request->url(), itube, sizeof(itube),request);
  else if (request->url() == "/favicon.png") handleStreamFlashFile(request->url(), faviconpng, sizeof(faviconpng),request);
  else if (request->url() == "/rss.json") handleStreamGzipFlashFile(request->url(), rssjson, sizeof(rssjson),request);
  else if (request->url() == "/ct_Default.json") handleStreamSystemTheme(defaultTheme,request);
  else if (request->url() == "/ct_Cool_Ice.json") handleStreamSystemTheme(coolIceTheme,request);
  else if (request->url() == "/ct_Just_Orange.json") handleStreamSystemTheme(justOrangeTheme,request);
  else sendResponse(404,"Not Found",request);
}

String getResultCodeText(int resultCode) {
  switch (resultCode) {
    case UPD_SUCCESS:
      return "SUCCESS";
      break;
    case UPD_NO_CHANGE:
      return "SUCCESS (NO CHANGES)";
      break;
    case UPD_SEC_CHANGE:
      return "SUCCESS (SECONDARY CHANGES)";
      break;
    case UPD_DATA_ERROR:
      return "DATA ERROR";
      break;
    case UPD_UNAUTHORISED:
      return "UNAUTHORISED";
      break;
    case UPD_HTTP_ERROR:
      return "HTTP ERROR";
      break;
    case UPD_INCOMPLETE:
      return "INCOMPLETE DATA RECEIVED";
      break;
    case UPD_NO_RESPONSE:
      return "NO RESPONSE FROM SERVER";
      break;
    case UPD_TIMEOUT:
      return "TIMEOUT WAITING FOR SERVER";
      break;
    default:
      return "OTHER ERROR";
      break;
  }
}

// Send some useful system & station information to the browser
void handleInfo(AsyncWebServerRequest *request) {
  unsigned long uptime = millis();
  char sysUptime[30];
  int days = uptime / msDay ;
  int hours = (uptime % msDay) / msHour;
  int minutes = ((uptime % msDay) % msHour) / msMin;

  sprintf(sysUptime,"%d days, %d hrs, %d min", days,hours,minutes);

  String message = "Free Heap: " + String(ESP.getFreeHeap()) + "\nMin Heap: " + String(ESP.getMinFreeHeap()) + "\nLargest free block: " + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)) + "\nFree PSRAM: " + String(ESP.getFreePsram()) + "\nHostname: " + String(hostname) + "\nFirmware version: v" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + " " + getBuildTime() + "\nSystem uptime: " + String(sysUptime) + "\nFree LittleFS space: " + String(LittleFS.totalBytes() - LittleFS.usedBytes());
  message+="\nCore Plaform: " + String(ESP.getCoreVersion()) + "\nCPU speed: " + String(ESP.getCpuFreqMHz()) + "MHz\nCPU Temperature: " + String(temperatureRead()) + "\nWiFi network: " + String(WiFi.SSID()) + "\nWiFi signal strength: " + String(WiFi.RSSI()) + "dB";

  sprintf(sysUptime,"%02d:%02d:%02d %02d/%02d/%04d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year+1900);
  message+="\nSystem clock: " + String(sysUptime);
  if (ghUpdate->releaseId.length()) {
    message+="\nGithub: " + ghUpdate->releaseId;
  }

  message+="\nSD Card type: ";
  switch (cardType) {
    case 100:
      message+="Not used";
      break;
    case 99:
      message+="Failed to initialise";
      break;
    case CARD_NONE:
      message+="None";
      break;
    case CARD_MMC:
      message+="MMC";
      break;
    case CARD_SD:
      message+="SD";
      break;
    case CARD_SDHC:
      message+="SDHC";
      break;
    default:
      message+="Unknown";
      break;
  }

  if (schedulerActive) message+="\nScheduler active, next event at " + String(nextSlotEventTime);
  else if (carouselActive) message+="\nCarousel active, next event at " + String(nextSlotEventTime);

  message+="\nCurrent location code: " + String(locationCode) + "\nCurrent location name: " + String(locationName) + "\nSuccessful: " + String(dataLoadSuccess) + "\nFailures: " + String(dataLoadFailure) + "\nTime since last data load: " + String((int)((millis()-lastDataLoadTime)/1000)) + " seconds";
  if (dataLoadFailure) message+="\nTime since last failure: " + String((int)((millis()-lastLoadFailure)/1000)) + " seconds";

  message+="\nFetch in progress: ";
  if (fetchInProgress) message+="true"; else message+="false";

  message+="\nLast Result: ";
  switch (boardMode) {
    case MODE_RAIL:
      if (useRDMclient) message+="RDMClient: " + String(jsonKeyBuffer->lastResultMessage);
      else message+="darwinClient: " + String(jsonKeyBuffer->lastResultMessage);
      break;

    case MODE_TUBE:
      message+=String(jsonKeyBuffer->lastResultMessage);
      break;

    case MODE_BUS:
      message+=String(jsonKeyBuffer->lastResultMessage);
      break;
  }
  message+="\nUpdate result code: ";
  message+=getResultCodeText(lastUpdateResult);
  message+="\nServices: " + String(station->numServices) + "\nMessages: ";
  int nMsgs = messages->numMessages;
  if (boardMode == MODE_TUBE) nMsgs--;
  message+=String(nMsgs) + "\n";

  if (rssEnabled) {
    message+="Last RSS result: " + getResultCodeText(lastRssUpdateResult) + "\nNext RSS update: " + String(nextRssUpdate-millis()) + "ms\n\n";
  }

  if (weatherEnabled) {
    message+="Last weather result: " + getResultCodeText(lastWeatherUpdateResult) + "\nNext weather update: " + String(nextWeatherUpdate-millis()) + "ms";
  }

  sendResponse(200,message,request);
}

// Stream the index.htm page unless we're in first time setup and need the api keys
void handleRoot(AsyncWebServerRequest *request) {
  if (!apiKeys) {
    if (LittleFS.exists("/keys.htm")) handleStreamFile("/keys.htm",request); else handleStreamGzipFlashFile("/keys.htm",keyshtm,sizeof(keyshtm),request);
  } else {
    if (LittleFS.exists("/index_d.htm")) handleStreamFile("/index_d.htm",request); else handleStreamGzipFlashFile("/index.htm",indexhtm,sizeof(indexhtm),request);
  }
}

// Send the firmware version to the client (called from index.htm)
void handleFirmwareInfo(AsyncWebServerRequest *request) {
  String response = "{\"firmware\":\"B" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "-W" + String(WEBAPPVER_MAJOR) + "." + String(WEBAPPVER_MINOR) + "\"}";
  request->send(200,contentTypeJson,response);
}

// Force a reboot of the ESP32
void handleReboot(AsyncWebServerRequest *request) {
  sendResponse(200,"The Departures Board is restarting...",request);
  restartTimer.once(1, []() { ESP.restart(); });
}

// Erase the stored WiFiManager credentials
void handleEraseWiFi(AsyncWebServerRequest *request) {
  sendResponse(200,"Erasing stored WiFi settings.\n\nYou will need to connect to the \"Departures Board\" network and use WiFi Manager to reconfigure the settings.",request);
  restartTimer.once(1, []() { WiFiManager wm; wm.resetSettings(); ESP.restart();});
}

// "Factory reset" the app - delete WiFi, format file system and reboot
void handleFactoryReset(AsyncWebServerRequest *request) {
  sendResponse(200,"Factory reseting the Departures Board...",request);
  restartTimer.once(1, []() { WiFiManager wm; wm.resetSettings(); LittleFS.format(); ESP.restart();});
}

// Interactively change the brightness of the OLED panel (called from index.htm)
void handleBrightness(AsyncWebServerRequest *request) {
  if (request->hasParam("b")) {
    int level = request->getParam("b")->value().toInt();
    if (level>0 && level<256) {
      dma_display->setBrightness(brightness);
      brightness = level;
      int rptDelay = 1;
      if (request->hasParam("d")) {
        rptDelay = request->getParam("d")->value().toInt();
      }
      if (rptDelay) restartTimer.once(rptDelay, []() { dma_display->setBrightness(brightness); });
      sendResponse(200,"OK",request);
      return;
    }
  }
  sendResponse(200,"invalid request",request);
}

// Web GUI has requested updates be installed
void handleOtaUpdate(AsyncWebServerRequest *request) {
  sendResponse(200,"Update initiated - check the Departures Board display for progress.",request);
  manualUpdateCheck = true;
}

void doManualOtaCheck() {
  clearCanvas();
  centreText("Getting firmware info from GitHub",LINE1,RGB565_WHITE,RAIL9,u8g2_main);
  sendBuffer();

  if (ghUpdate->getLatestRelease()==UPD_SUCCESS) {
    checkForFirmwareUpdate();
  } else {
    char msg[200];
    sprintf(msg,"Unable to get the latest release information (%s)",jsonKeyBuffer->lastResultMessage);
    showUpdateCompleteScreen("Firmware Update Aborted",msg,15,false);
  }
  // Always restart
  ESP.restart();
}

// Endpoint for controlling sleep mode
void handleControl(AsyncWebServerRequest *request) {
  String resp = "{\"sleeping\":";
  if (request->hasParam("sleep")) {
    if (request->getParam("sleep")->value() == "1") forcedSleep=true; else forcedSleep=false;
  }
  if (request->hasParam("clock")) {
    if (request->getParam("clock")->value() == "1") useNSEclockForSleep=true; else useNSEclockForSleep=false;
  }
  resp += (isSleeping || forcedSleep) ? "true":"false";
  resp += ",\"display\":";
  resp += (useNSEclockForSleep || (!isSleeping && !forcedSleep)) ? "true":"false";
  resp += "}";
  request->send(200, contentTypeJson, resp);
}

void handleVoices(AsyncWebServerRequest *request) {

  String resp = "[";
  bool firstEntry = true;

  if (cardReaderReady) {
    File root = SD_MMC.open("/");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (file.isDirectory()) {
          String path = file.name();
          String name = path.startsWith("/") ? path.substring(1) : path;
          if (name.startsWith("V")) {
            File voicejson = SD_MMC.open("/" + name + "/voice.json");
            if (voicejson) {
              if (!firstEntry) {
                resp += ",\n{\"path\": \"/" + name + "\",\n\"config\": \n" + voicejson.readString() + "\n}";
              } else {
                resp += "\n{\"path\": \"/" + name + "\",\n\"config\": \n" + voicejson.readString() + "\n}";
                firstEntry = false;
              }
              voicejson.close();
            }
          }
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }
  }
  resp += "]";
  request->send(200, contentTypeJson, resp);
}

void handleVolume(AsyncWebServerRequest *request) {
  if (request->hasParam("v") && enableAudioAnnouncements) {
    int v = request->getParam("v")->value().toInt();
    if (v>=0 && v<=191) {
      // set the DAC gain directly
      es.WriteReg(0x32,v);
    }
    // Reset the queue
    xQueueReset(audioQueue);
    addAudioToQueue(audioVoice,"AN/$",false);  // Play the bell for feedback
  }
  request->send(200, contentTypeText, "OK");
}

void handleGetThemes(AsyncWebServerRequest *request) {
  // System themes
  String resp = "[{\"name\":\"Default\",\"file\":\"ct_Default\",\"user\":false},{\"name\":\"Cool Ice\",\"file\":\"ct_Cool_Ice\",\"user\":false},{\"name\":\"Just Orange\",\"file\":\"ct_Just_Orange\",\"user\":false}";

  File root = LittleFS.open("/");
  if (root) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory() && strncmp(f.path(),"/ct_",4)==0) {
        // User theme, so add to json
        String themeName = String(f.path()).substring(4,strlen(f.path())-5);
        resp+=",{\"name\":\"" + themeName + "\",\"file\":\"ct_" + themeName + "\",\"user\":true}";
      }
      f = root.openNextFile();
    }
    f.close();
    root.close();
  }
  resp+="]";
  request->send(200,contentTypeJson,resp);
}

// Loads the specified theme immediately
void handleLoadTheme(AsyncWebServerRequest *request) {
  char themeName[40];

  if (request->hasParam("t")) {
    loadTheme(request->getParam("t")->value().c_str());
    request->send(200,contentTypeText,"OK");
  } else {
    request->send(200,contentTypeText,"Missing theme");
  }
}

// Call the National Rail Station Picker (called from index.htm)
void handleStationPicker(AsyncWebServerRequest *request)
{
  if (!request->hasParam("q")) {
    sendResponse(400,"Missing Query",request);
    return;
  }

  String query = request->getParam("q")->value();
  if (query.length() <= 2) {
    sendResponse(400,"Query too short",request);
    return;
  }

  const char* host = "stationpicker.nationalrail.co.uk";
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(4000);

  if (!client.connect(host, 443)) {
    sendResponse(408, "NR Connect Timeout",request);
    return;
  }

  client.print(String("GET /stationPicker/") + query + " HTTP/1.0\r\n"
               "Host: stationpicker.nationalrail.co.uk\r\n"
               "Referer: https://www.nationalrail.co.uk\r\n"
               "Origin: https://www.nationalrail.co.uk\r\n"
               "Connection: close\r\n\r\n");

  int requestTimer = 0;
  while (!client.available() && requestTimer<1000) {
    requestTimer++;
    delay(1);
  }

  if (!client.available()) {
    client.stop();
    sendResponse(408,"NRQ Timeout",request);
  }

  String statusLine = client.readStringUntil('\n');

  if (statusLine.indexOf("200") == -1) {
    client.stop();
    sendResponse(503, statusLine, request);
    return;
  }

  // Skip the remaining headers
  while (client.connected() || client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Start sending response
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  uint8_t buffer[512];
  unsigned long timeout = millis() + 5000UL;
  while ((client.connected() || client.available()) && millis() < timeout) {
    int len = client.read(buffer, sizeof(buffer));
    if (len > 0) {
      response->write(buffer, len);
      delay(1);
    }
  }

  client.stop();
  request->send(response);
}


/*
 * Station Board functions - pulling updates and animating the Departures Board main display
 */

// Draw the primary service line
void drawPrimaryService(bool showVia) {
  int destPos;
  char clipDestination[MAXLOCATIONSIZE+5];
  char etd[16];
  char plat[4];
  uint16_t drawColour;

  clip_canvas.fillScreen(RGB565_BLACK);
  drawStr(0,0,colours[RAIL_DEPARTURETIME],station->service[0].sTime,RAIL9);
  destPos = getStringWidth(station->service[0].sTime);
  if (isDigit(station->service[0].etd[0])) {
    sprintf(etd,"Exp %s",station->service[0].etd);
    drawColour = colours[RAIL_EXPECTED];
  } else {
    strcpy(etd,station->service[0].etd);
    if (strcmp(etd,"On time")==0) drawColour = colours[RAIL_ONTIME];
    else drawColour = colours[RAIL_CANCELLED];
  }
  int etdWidth = getStringWidth(etd) + (etd[strlen(etd)-1]=='1'?1:0);
  drawStr(SCREEN_WIDTH - etdWidth,0,drawColour,etd,RAIL9);
  strcpy(plat,"--");
  if (!hidePlatform && station->platformAvailable && station->service[0].platform[0] && station->service[0].serviceType == TRAIN && strcmp(station->service[0].platform,"TBC")) {
    strlcpy(plat,station->service[0].platform,sizeof(plat));
  } else if (!hidePlatform && station->service[0].serviceType == BUS) {
    strcpy(plat,"\x86"); // Bus Icon
  }
  if (!hidePlatform) {
    destPos+=strlen(plat)==3?18:13;
    if (padPlatform && hideOrdinals && strlen(plat)<3) destPos+=5;
    drawStr(destPos-getStringWidth(plat)-1,0,colours[RAIL_PLATFORM],plat,RAIL9);
  }
  destPos+=4; // Gap to destination name

  // Space available for destination name
  int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 3;

  if (showVia) {
    strcpy(clipDestination,station->service[0].via);
    drawColour = colours[RAIL_VIA];
  } else {
    strcpy(clipDestination,station->service[0].destination);
    drawColour = colours[RAIL_DESTINATION];
  }
  if (getStringWidth(clipDestination) > spaceAvailable) {
    while (getStringWidth(clipDestination) > (spaceAvailable - 6)) {
      clipDestination[strlen(clipDestination)-1] = '\0';
    }
    // check if there's a trailing space left
    if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
    strcat(clipDestination,"\x85");  // 3dots
  }
  drawStr(destPos,0,drawColour,clipDestination,RAIL9);
  // Copy to the main canvas
  pushClipAtLine(LINE1);
}

// Draw the secondary service line
void drawServiceLine(int line, int y, int offset = 0) {
  char clipDestination[MAXLOCATIONSIZE+5];
  char ordinal[5];
  char plat[4];
  char timeSeg[7];
  int destPos;
  uint16_t drawColour;

  timeIsDisplayed = false;

  switch (line) {
    case 1:
      strcpy(ordinal,"2nd");
      break;
    case 2:
      strcpy(ordinal,"3rd");
      break;
    default:
      sprintf(ordinal,"%dth",line+1);
      break;
  }

  clip_canvas.fillRect(0,y+offset,SCREEN_WIDTH,CLIP_HEIGHT,RGB565_BLACK);

  if (line<station->numServices) {
    if (hideOrdinals) {
      drawStr(0,offset,colours[RAIL_DEPARTURETIME],station->service[line].sTime,RAIL9);
      destPos = getStringWidth(station->service[line].sTime);
    } else {
      drawStr(0,offset,colours[RAIL_ORDINAL],ordinal,RAIL9);
      drawStr(19,offset,colours[RAIL_DEPARTURETIME],station->service[line].sTime,RAIL9);
      destPos = getStringWidth(station->service[line].sTime) + 19;
    }
    char etd[16];
    if (isDigit(station->service[line].etd[0])) {
      sprintf(etd,"Exp %s",station->service[line].etd);
      drawColour = colours[RAIL_EXPECTED];
    } else {
      strcpy(etd,station->service[line].etd);
      if (strcmp(etd,"On time")==0) drawColour = colours[RAIL_ONTIME];
      else drawColour = colours[RAIL_CANCELLED];
    }
    int etdWidth = getStringWidth(etd) + (etd[strlen(etd)-1]=='1'?1:0);
    drawStr(SCREEN_WIDTH - etdWidth,offset,drawColour,etd,RAIL9);

    strcpy(plat,"--");
    if (!hidePlatform && station->platformAvailable && station->service[line].platform[0] && station->service[line].serviceType == TRAIN && strcmp(station->service[line].platform,"TBC")) {
      strlcpy(plat,station->service[line].platform,sizeof(plat));
    } else if (!hidePlatform && station->service[line].serviceType == BUS) {
      strcpy(plat,"\x86"); // Bus Icon
    }
    if (!hidePlatform) {
      destPos+=strlen(plat)==3?18:13;
      if (padPlatform && strlen(plat)<3) destPos+=5;
      drawStr(destPos-getStringWidth(plat)-1,offset,colours[RAIL_PLATFORM],plat,RAIL9);
    }

    destPos+=4; // Gap to destination name

    // Space available for destination name
    int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 3;

    // work out if we need to clip the destination
    strcpy(clipDestination,station->service[line].destination);
    if (getStringWidth(clipDestination) > spaceAvailable) {
      while (getStringWidth(clipDestination) > spaceAvailable - 5) {
        clipDestination[strlen(clipDestination)-1] = '\0';
      }
      // check if there's a trailing space left
      if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
      strcat(clipDestination,"\x85");  // 3dots
    }
    drawStr(destPos,offset,colours[RAIL_DESTINATION],clipDestination,RAIL9);
  } else {
    if (line == station->numServices) {
      // Show the time
      strlcpy(timeSeg,currentTime,7);
      drawStr(64,offset,colours[RAIL_CLOCK],timeSeg,RAILCLOCK9);
      strcpy(timeSeg,currentTime+6);
      drawStr(112,offset+2,colours[RAIL_CLOCK],timeSeg,RAILCLOCK7);
      timeIsDisplayed = true;
    } else if (showStationName && line==station->numServices+1) {
      // Show the current location
      centreText(station->location,offset,colours[RAIL_STATION],stationNameFont);
    } else if ((!showStationName && weatherEnabled && weatherMsg[0] && line==station->numServices+1) || (showStationName && weatherEnabled && weatherMsg[0] && line==station->numServices+2)) {
      // Show the weather
      centreText(weatherMsg,offset,colours[RAIL_WEATHER],RAIL9);
    } else {
      // We're showing the mandatory attribution
      centreText(useRDMclient?rdgAttribution:nrAttribution,offset,colours[RAIL_MESSAGES],RAIL9);
    }
  }
}

// Draw the initial Departures Board
void drawStationBoard() {
  if (showClockNoServices && station->numServices == 0) {
    if (!noServiceClockIsActive) firstLoad = true;
    noServiceClockIsActive = true;
  } else {
    if (noServiceClockIsActive) firstLoad = true;
    noServiceClockIsActive = false;
  }
  numMessages=0;
  padPlatform = false;
  if (firstLoad) {
    // Clear the entire screen for the first load since boot up/wake from sleep
    clearCanvas();
    dma_display->setBrightness(brightness);
    restartTimer.once(1, []() { dma_display->setBrightness(brightness); });
    firstLoad=false;
    line3Service = noScrolling ? 1 : 0;
  } else {
    // Clear the top lines
    blankArea(0,LINE1,SCREEN_WIDTH,CLIP_HEIGHT);
  }

  if (!noServiceClockIsActive) {
    if (noScrolling && station->numServices>1) {
      // Check for the longest platform no
      int fc = hideOrdinals?0:1;
      for (int i=fc;i<station->numServices;++i) {
        if (strlen(station->service[i].platform) > 2) {
          padPlatform = true;
          break;
        }
      }
    }
    // Draw the primary service line
    isShowingVia=false;
    viaTimer=millis()+300000;  // effectively don't check for via
    if (station->numServices) {
      drawPrimaryService(false);
      if (station->service[0].via[0]) viaTimer=millis()+4000;
      if (station->service[0].isCancelled) {
        // This train is cancelled
        if (station->serviceMessage[0]) {
          strcpy(line2[0],station->serviceMessage);
          numMessages=1;
        }
      } else {
        // The train is not cancelled
        if (station->service[0].isDelayed && station->serviceMessage[0]) {
          // The train is delayed and there's a reason
          strcpy(line2[0],station->serviceMessage);
          numMessages++;
        }
        if (station->calling[0]) {
          // Add the calling stops message
          sprintf(line2[numMessages],"Calling at: %s",station->calling);
          numMessages++;
        }
        if (strcmp(station->origin, station->location)==0) {
          // Service originates at this station
          if (station->service[0].opco[0]) {
            sprintf(line2[numMessages],"This %s service starts here.",station->service[0].opco);
          } else {
            strcpy(line2[numMessages],"This service starts here.");
          }
          // Add the seating if available
          switch (station->service[0].classesAvailable) {
            case 1:
              strcat(line2[numMessages],firstClassSeating);
              break;
            case 2:
              strcat(line2[numMessages],standardClassSeating);
              break;
            case 3:
              strcat(line2[numMessages],dualClassSeating);
              break;
          }
          numMessages++;
        } else {
          // Service originates elsewhere
          strcpy(line2[numMessages],"");
          if (station->service[0].opco[0]) {
            if (station->origin[0]) {
              sprintf(line2[numMessages],"This is the %s service from %s.",station->service[0].opco,station->origin);
            } else {
              sprintf(line2[numMessages],"This is the %s service.",station->service[0].opco);
            }
          } else {
            if (station->origin[0]) {
              sprintf(line2[numMessages],"This service originated at %s.",station->origin);
            }
          }
          // Add the seating if available
          switch (station->service[0].classesAvailable) {
            case 1:
              strcat(line2[numMessages],firstClassSeating);
              break;
            case 2:
              strcat(line2[numMessages],standardClassSeating);
              break;
            case 3:
              strcat(line2[numMessages],dualClassSeating);
              break;
          }
          if (line2[numMessages][0]) numMessages++;
        }
        if (station->service[0].trainLength) {
          // Add the number of carriages message
          sprintf(line2[numMessages],"This train is formed of %d coaches.",station->service[0].trainLength);
          numMessages++;
        }
      }

      if (noScrolling && station->numServices>1) {
        drawServiceLine(1,0,0);
        pushClipAtLine(LINE2);
      }
    } else {
      blankArea(0,LINE2,SCREEN_WIDTH,SCREEN_HEIGHT-LINE2);
      centreText("No scheduled services at this station.",LINE1,colours[RAIL_NOSERVICES],RAIL9,u8g2_main);
    }
  }

  lastSvcDescMessage = numMessages-1;

  // Check if RSS should be inserted before nrcc messages
  if (rssEnabled && rssPriority && rssMessage[0]) {
    strcpy(line2[numMessages++],rssMessage);
  }

  // Add any nrcc messages
  for (int i=0;i<messages->numMessages;i++) {
    strcpy(line2[numMessages],messages->messages[i]);
    numMessages++;
  }

  // Check if RSS should be added after nrcc messages
  if (rssEnabled && !rssPriority && rssMessage[0]) {
    strcpy(line2[numMessages++],rssMessage);
  }

  // Setup for the first message to rollover to
  isScrollingStops=false;
  currentMessage=numMessages-1;

  sendBuffer();
}

void updateRailDepartures() {
  if (useRDMclient) rdmRailData->loadDepartures(station,messages);
  else darwinRailData->loadDepartures(station,messages);
  lastDataLoadTime = millis();
  noDataLoaded = false;
  dataLoadSuccess++;
}

void waitForFirstLoad() {
  // Wait for the first data load
  while (!fetchComplete) {
    delay(250);
    if (startupProgressPercent<95) {
      startupProgressPercent+=5;
      drawProgressBar(startupProgressPercent);
    }
  }
  drawProgressBar(100);
}

/*
 *
 * London Underground Board
 *
 */
void updateArrivals() {
  tfldata->loadArrivals(station,messages);
  lastDataLoadTime = millis();
  noDataLoaded = false;
  dataLoadSuccess++;
}

void drawUndergroundService(int serviceId, int y, int offset, bool isShowingCurrentLocation = false) {
  char serviceData[MAXLOCATIONSIZE];
  char serviceOrdinal[4];
  int usedSpace = 4;
  uint16_t drawColour;

  clip_canvas.fillRect(0,offset,SCREEN_WIDTH,CLIP_HEIGHT,RGB565_BLACK);

  if (serviceId < station->numServices) {
    if (!station->service[serviceId].atPlatform) {
      if (station->service[serviceId].timeToStation <= 40) {
        usedSpace += 18;
        drawStr(SCREEN_WIDTH-18,offset,colours[TUBE_DUE],"Due",TUBE10);
      } else {
        int mins = (station->service[serviceId].timeToStation + 30) / 60; // Round to nearest minute
        sprintf(serviceData,"%d",mins);
        if (mins==1) drawStr(SCREEN_WIDTH-21,offset,colours[TUBE_TTS],"min",TUBE10); else drawStr(SCREEN_WIDTH-21,offset,colours[TUBE_TTS],"mins",TUBE10);
        drawStr(SCREEN_WIDTH-26-(strlen(serviceData)*7),offset,colours[TUBE_TTS],serviceData,TUBE10);
        usedSpace += (strlen(serviceData)*7) + 26;
      }
    }

    sprintf(serviceOrdinal,"%d ",serviceId+1);
    if (isShowingCurrentLocation) {
      strcpy(serviceData,station->service[serviceId].currentLocation);
      drawColour = colours[TUBE_CURRENTLOCATION];
    } else {
      strcpy(serviceData,station->service[serviceId].destination);
      drawColour = colours[TUBE_DESTINATION];
    }
    if (getStringWidth(serviceData) > SCREEN_WIDTH-usedSpace-12) {
      while (getStringWidth(serviceData) > SCREEN_WIDTH-usedSpace-6-12) {
        serviceData[strlen(serviceData)-1] = '\0';
      }
      if (serviceData[strlen(serviceData)-1] == ' ') serviceData[strlen(serviceData)-1] = '\0'; // remove any trailing space
      strcat(serviceData,"\x81");
    }
    drawStr(0,offset,colours[TUBE_ORDINAL],serviceOrdinal,TUBE10);
    drawStr(12,offset,drawColour,serviceData,TUBE10);
  }
}

// Draw/update the Underground Arrivals Board
void drawUndergroundBoard() {

  if (lastUpdateResult == UPD_SUCCESS) {
    if (line3Service==0) line3Service=1;
    attributionScrolled=false;
  }
  if (firstLoad) {
    // Clear the entire screen for the first load since boot up/wake from sleep
    clearCanvas();
    dma_display->setBrightness(brightness);
    restartTimer.once(1, []() { dma_display->setBrightness(brightness); });
    firstLoad=false;
  } else {
      if (lastUpdateResult == UPD_SEC_CHANGE) {
        // Clear the top two lines
        blankArea(0,ULINE1,SCREEN_WIDTH,ULINE3-ULINE1);
      } else {
        clearCanvas();
      }
  }

  if (station->boardChanged) {
    isShowingVia = false;
    if (station->service[0].currentLocation[0]) viaTimer=millis()+6000; else viaTimer=millis()+300000;
    // prepare to scroll up primary services
    scrollPrimaryYpos = 11;
    isScrollingPrimary = true;
    // reset line3
    line3Service = 99;
    prevScrollStopsLength = 0;
    scrollStopsLength = 0;
    currentMessage=99;
    serviceTimer=0;
  } else if (lastUpdateResult == UPD_SEC_CHANGE) {
    // Reset the top two services only
    isShowingVia = false;
    if (station->service[0].currentLocation[0]) viaTimer=millis()+6000; else viaTimer=millis()+300000;
    // prepare to scroll up primary services
    scrollPrimaryYpos = 11;
    isScrollingPrimary = true;
  } else {
    // Draw the primary service line(s)
    if (station->numServices) {
      drawUndergroundService(0,0,0);
      pushClipAtLine(ULINE1);
      if (station->numServices>1) { drawUndergroundService(1,0,0); pushClipAtLine(ULINE2); }
    } else {
      centreText("No scheduled services at this station.",ULINE1,colours[TUBE_NOSERVICES],TUBE10,u8g2_main);
    }
  }

  numMessages = 0;

  if (showStationName) {
    strcpy(line2[numMessages],locationName);
    numMessages++;
  }

  strcpy(line2[numMessages],"88:88:88"); // Flag for showing the clock
  numMessages++;

  // Add weather message if enabled and available
  if (weatherEnabled && weatherMsg[0]) {
    strcpy(line2[numMessages],"%");
    numMessages++;
  }

  // Check if RSS should be inserted before TfL messages
  if (rssEnabled && rssPriority && rssMessage[0] && !noScrolling) {
    strcpy(line2[numMessages],rssMessage);
    numMessages++;
  }

  // Add any TfL messages
  for (int i=0;i<messages->numMessages;i++) {
    strcpy(line2[numMessages],messages->messages[i]);
    numMessages++;
  }

  // Check if RSS should be added after TfL messages
  if (rssEnabled && !rssPriority && rssMessage[0] && !noScrolling) {
    strcpy(line2[numMessages],rssMessage);
    numMessages++;
  }
  sendBuffer();
}


/*
 *
 * Bus Departures Board
 *
 */
void drawBusService(int serviceId, int offset, int destPos) {
  char clipDestination[MAXLOCATIONSIZE];
  char etd[16];

  if (serviceId < station->numServices) {
    clip_canvas.fillRect(0,offset,SCREEN_WIDTH,CLIP_HEIGHT,RGB565_BLACK);
    drawStr(0,offset,colours[BUS_SERVICE],station->service[serviceId].via,RAIL9);
    int etdWidth = 21;
    if (isDigit(station->service[serviceId].etd[0])) {
      sprintf(etd,"Exp %s",station->service[serviceId].etd);
      etdWidth = 41;
      drawStr(SCREEN_WIDTH - etdWidth,offset,colours[BUS_EXPECTED],etd,RAIL9);
    } else {
      strcpy(etd,station->service[serviceId].sTime);
      drawStr(SCREEN_WIDTH - etdWidth,offset,colours[BUS_DEPARTURETIME],etd,RAIL9);
    }

    // work out if we need to clip the destination
    strcpy(clipDestination,station->service[serviceId].destination);
    int spaceAvailable = SCREEN_WIDTH - destPos - etdWidth - 6;
    if (getStringWidth(clipDestination) > spaceAvailable) {
      while (getStringWidth(clipDestination) > spaceAvailable - 5) {
        clipDestination[strlen(clipDestination)-1] = '\0';
      }
      // check if there's a trailing space left
      if (clipDestination[strlen(clipDestination)-1] == ' ') clipDestination[strlen(clipDestination)-1] = '\0';
      strcat(clipDestination,"\x85");  // 3dots
    }
    drawStr(destPos,offset,colours[BUS_DESTINATION],clipDestination,RAIL9);
  }
}

// Draw/update the Bus Departures Board
void drawBusDeparturesBoard() {

  if (line3Service==0) line3Service=1;
  if (firstLoad) {
    // Clear the entire screen for the first load since boot up/wake from sleep
    clearCanvas();
    dma_display->setBrightness(brightness);
    restartTimer.once(1, []() { dma_display->setBrightness(brightness); });
    firstLoad=false;
  } else {
      // Clear the top two lines
      blankArea(0,LINE1,SCREEN_WIDTH,LINE3-LINE1);
  }

  if (station->boardChanged) {
    // prepare to scroll up primary services
    scrollPrimaryYpos = 11;
    isScrollingPrimary = true;
    // reset line3
    if (station->numServices>2) {
      line3Service=2;
    } else {
      line3Service=99;
    }
    currentMessage = -1;
    blankArea(0,LINE3,SCREEN_WIDTH,CLIP_HEIGHT);
    serviceTimer=0;
  } else {
    // Draw the primary service line(s)
    if (station->numServices) {
      drawBusService(0,LINE1,busDestX);
      pushClipAtLine(LINE1);
      if (station->numServices>1) { drawBusService(1,LINE2,busDestX); pushClipAtLine(LINE2); }
    } else {
      centreText("No scheduled services at this stop",LINE1,colours[BUS_NOSERVICES],RAIL9,u8g2_main);
    }
  }
  sendBuffer();
}

void updateBusDepartures() {
  busdata->loadDepartures(station);
  lastDataLoadTime = millis();
  noDataLoaded = false;
  dataLoadSuccess++;
  // Work out the max column size for service numbers
  busDestX=0;
  setBoardFont(RAIL9,u8g2_clip);
  for (int i=0;i<station->numServices;i++) {
    int svcWidth = getStringWidth(station->service[i].via);
    busDestX = (busDestX > svcWidth) ? busDestX : svcWidth;
  }
  busDestX+=5;

  int msgIndex = 0;
  if (showStationName) strcpy(line2[msgIndex++],locationName);
  if (weatherEnabled && weatherMsg[0]) strcpy(line2[msgIndex++],"%");
  strcpy(line2[msgIndex++],"88:88:88");
  strcpy(line2[msgIndex++],btAttribution);
  messages->numMessages=msgIndex;
}


/*
 * Setup / Loop functions
*/


bool playPrimaryService(const char *announcer, bool checkPath = true) {

  char speechFile[24];
  int hour, minute;

  if (!enableAudioAnnouncements || station->numServices==0) return false;

  bool result = addAudioToQueue(announcer, "AN/$", checkPath); // ding dong
  if (station->service[0].isCancelled) {
    // "operating company"
    sprintf(speechFile,"OP/%s",station->service[0].opcoId);
    result = result && addAudioToQueue(announcer, speechFile, checkPath);
    // "regret to announce the cancellation of the"
    result = result && addAudioToQueue(announcer, "AN/C", checkPath);
  } else {
    if (station->platformAvailable && station->service[0].platform[0] && (isdigit(station->service[0].platform[0]) || (station->service[0].platform[0] >= 'A' && station->service[0].platform[0] <= 'D')) && strcmp(station->service[0].platform,"BUS")) {
      // "The next train departing from platform "
      result = result && addAudioToQueue(announcer, "AN/D", checkPath);
      // "##"
      sprintf(speechFile,"NU/%s",station->service[0].platform);
      result = result && addAudioToQueue(announcer, speechFile, checkPath);
      // "will be the"
      result = result && addAudioToQueue(announcer, "AN/W", checkPath);
    } else {
      // No platform information available
      if (station->service[0].serviceType == BUS) {
        //"The next service departing will be the"
        result = result && addAudioToQueue(announcer, "AN/X", checkPath);
      } else {
        // "The next train departing will be the"
        result = result && addAudioToQueue(announcer, "AN/E", checkPath);
      }
    }
  }
  sscanf(station->service[0].sTime, "%d:%d", &hour, &minute);
  sprintf(speechFile,"NU/%d",hour);
  // "hour"
  result = result && addAudioToQueue(announcer, speechFile, checkPath);
  // "minute"
  sprintf(speechFile,"NU/%02d",minute);
  result = result && addAudioToQueue(announcer, speechFile, checkPath);

  if (!station->service[0].isCancelled) {
    //"operating company service to"
    sprintf(speechFile,"OP/%s$",station->service[0].opcoId);
    result = result && addAudioToQueue(announcer, speechFile, checkPath);
  } else {
    // "service to"
    result = result && addAudioToQueue(announcer, "AN/S", checkPath);
  }

  // "destination"
  sprintf(speechFile,"SN/%c/%s",station->service[0].destinationCrs[0],station->service[0].destinationCrs);
  result = result && addAudioToQueue(announcer, speechFile, checkPath);

  if (station->service[0].viaCrs[0]) {
    // "via"
    result = result && addAudioToQueue(announcer, "AN/V", checkPath);
    // "destination"
    sprintf(speechFile,"SN/%c/%s",station->service[0].viaCrs[0],station->service[0].viaCrs);
    result = result && addAudioToQueue(announcer, speechFile, checkPath);
    if (station->service[0].viaCrs2[0]) {
      result = result && addAudioToQueue(announcer, "AN/&", checkPath);
      sprintf(speechFile,"SN/%c/%s",station->service[0].viaCrs2[0],station->service[0].viaCrs2);
      result = result && addAudioToQueue(announcer, speechFile, checkPath);
    }
  }

  if (!station->service[0].isCancelled) {
    if (station->service[0].serviceType == BUS) {
      // "This is a bus service"
      result = result && addAudioToQueue(announcer, "AN/B", checkPath,1000);
    }
    if (enableAudioCallingList) {
      const char *p = station->callingCrs;
      int count = 0;
      // "calling at"
      result = result && addAudioToQueue(announcer, "AN/@", checkPath,500);

      while (*p) {
        // Skip separators
        while (*p == ' ' || *p == ',') p++;
        if (*p == '\0') break;

        // Start of code
        const char *codeStart = p;

        // Find end of code
        while (*p && *p != ',' && *p != ' ') p++;

        const char *codeEnd = p;
        size_t len = codeEnd - codeStart;

        // Only accept 3‑letter CRS codes
        if (len == 3) {
          sprintf(speechFile, "SN/%c/%.3s",codeStart[0],codeStart);
          count++;

          // Peek ahead: is there another code after this?
          const char *q = p;
          while (*q == ' ' || *q == ',') q++;

          bool hasMore = (*q != '\0');

          if (hasMore) {
            // Normal item
            result = result && addAudioToQueue(announcer, speechFile, checkPath,500);
          } else {
            // Last item
            if (count > 1) {
              result = result && addAudioToQueue(announcer, "AN/&", checkPath,500);   // "and"
              result = result && addAudioToQueue(announcer, speechFile, checkPath,500);
            } else {
              result = result && addAudioToQueue(announcer, speechFile, checkPath,500);
              result = result && addAudioToQueue(announcer, "AN/O", checkPath,500);   // "only"
            }
          }
        }
      }
    }

    if (station->service[0].trainLength) {
      // "This train is formed of"
      result = result && addAudioToQueue(announcer, "AN/T", checkPath);
      // number of coaches
      result = result && addNumberToAudioQueue(announcer, station->service[0].trainLength, checkPath);
      // "coaches"
      result = result && addAudioToQueue(announcer, "AN/R", checkPath);
    }

    switch (station->service[0].classesAvailable) {
      case 1:
        // "first class seating only"
        result = result && addAudioToQueue(announcer, "AN/1", checkPath);
        break;
      case 2:
        // "standard class seating only"
        result = result && addAudioToQueue(announcer, "AN/2", checkPath);
        break;
      case 3:
        // "first and standard class seating is available"
        result = result && addAudioToQueue(announcer, "AN/3", checkPath);
        break;
    }
  } else {
    // Train is cancelled, do we have a matching explanation
    if (station->serviceMessage[0]) {
      for (int i=0;i<NUM_REASONS;++i) {
        if (std::strstr(station->serviceMessage,problemReasons[i])) {
          sprintf(speechFile,"RE/C%02d",i);
          result = result && addAudioToQueue(announcer, speechFile, checkPath);
          break;
        }
      }
    }
  }

  int delayMins = 0;
  if (!station->service[0].isCancelled && isDigit(station->service[0].etd[0])) {
    delayMins = rdmRailData->timeDiff(station->service[0].etd,station->service[0].sTime);
  }
  if ((delayMins > 5 || station->service[0].isDelayed || strcmp(station->service[0].etd,"Delayed")==0) && !station->service[0].isCancelled) {
    // "operating company"
    sprintf(speechFile,"OP/%s",station->service[0].opcoId);
    result = result && addAudioToQueue(announcer, speechFile, checkPath,500);
    // "apologises for the late running of this service"
    result = result && addAudioToQueue(announcer, "AN/Y", checkPath);
    // Check if we have a reason
    if (station->serviceMessage[0]) {
      for (int i=0;i<NUM_REASONS;++i) {
        if (std::strstr(station->serviceMessage,problemReasons[i])) {
          sprintf(speechFile,"RE/C%02d",i);
          result = result && addAudioToQueue(announcer, speechFile, checkPath);
          break;
        }
      }
    }
  }
  return result;
}


bool playTubeService(const char *announcer, int svcIndex, int announcementNumber, bool checkPath = true) {

  char speechFile[20];
  char lineFilename[9];

  if (!enableAudioAnnouncements || station->numServices==0) return false;

  // Common start of speech
  bool result = true;
  if (!announcementNumber) result = result && addAudioToQueue(announcer, "AN/$", checkPath); // ding dong
  if (station->service[svcIndex].platform[0]) {
    result = result && addAudioToQueue(announcer, "AN/P", checkPath, announcementNumber?1000:0); // Platform
    sprintf(speechFile,"NU/%s",station->service[svcIndex].platform);
    result = result && addAudioToQueue(announcer, speechFile, checkPath);
  }
  strlcpy(lineFilename,station->service[svcIndex].lineId,sizeof(lineFilename));
  sprintf(speechFile,"TL/%s",lineFilename);
  result = result && addAudioToQueue(announcer, speechFile, checkPath); // e.g. bakerloo line

  if (station->service[svcIndex].atPlatform) {
    result = result && addAudioToQueue(announcer, "AN/F", checkPath); // This train's destination is
    sprintf(speechFile,"TN/%s",&station->service[svcIndex].destinationNaptan[5]);
    result = result && addAudioToQueue(announcer, speechFile, checkPath); // destination station
  } else {
    result = result && addAudioToQueue(announcer, "AN/N", checkPath); // The next train to
    sprintf(speechFile,"TN/%s",&station->service[svcIndex].destinationNaptan[5]);
    result = result && addAudioToQueue(announcer, speechFile, checkPath); // destination station
    if (station->service[svcIndex].timeToStation <= 40) {
      result = result && addAudioToQueue(announcer, "AN/G", checkPath);  // is now approaching. please stand back
    } else {
      int mins = (station->service[svcIndex].timeToStation + 30) / 60; // Round to nearest minute
      result = result && addAudioToQueue(announcer, "AN/A", checkPath); // will arrive in
      sprintf(speechFile,"NU/%d",mins);
      result = result && addAudioToQueue(announcer, speechFile, checkPath);
      if (mins>1) result = result && addAudioToQueue(announcer, "AN/MS", checkPath); // minutes
      else result = result && addAudioToQueue(announcer, "AN/M", checkPath); // minute
    }
  }

  return result;
}


//
// The main processing cycle for the National Rail Departures Board
//
void departureBoardLoop() {

  if (millis() > nextDataUpdate && !fetchInProgress && lastUpdateResult != UPD_UNAUTHORISED && !isSleeping && wifiConnected) {
    // Initiate a background update on Core 0
    fetchMode = FETCH_BOARD;
    fetchInProgress = true;
    xTaskNotifyGive(fetchTaskHandle);
    if (firstLoad) {
      waitForFirstLoad();
      strcpy(displayedTime,currentTime);
      if (lastUpdateResult == UPD_NO_CHANGE || lastUpdateResult == UPD_SEC_CHANGE) lastUpdateResult = UPD_SUCCESS;
    }
  }

  if (fetchComplete && lastUpdateResult == UPD_SEC_CHANGE && !isScrollingService && !isSleeping) {
    fetchComplete = false;
    updateRailDepartures();
    if (station->numServices) {
      if (!station->service[0].via[0]) isShowingVia=false;
      drawPrimaryService(isShowingVia);
      sendBuffer();
      if (station->calling[0] && showFullCalling) {
        for (int i=0;i<numMessages;i++) {
          if (strncmp("Calling",line2[i],7)==0) {
            // refresh the calling at times
            sprintf(line2[i],"Calling at: %s",station->calling);
            break;
          }
        }
      }
    }
    if (noScrolling && station->numServices>1) {
      drawServiceLine(1,0,0);
      pushClipAtLine(LINE2);
    }
  }

  if (fetchComplete && lastUpdateResult != UPD_SEC_CHANGE && !isScrollingService && !isSleeping) {
    if (!isScrollingStops || (!showFullCalling && isShowingCalling) || (!showFullMsgs && !isShowingCalling)) {
      fetchComplete = false;
      // Get the update data if there is any
      if (lastUpdateResult == UPD_SUCCESS) {
        // Retrieve the updated data
        updateRailDepartures();
        drawStationBoard();
        if (enableAudioAnnouncements && station->numServices && strcmp(station->service[0].serviceId,currentServiceId)) {
          strlcpy(currentServiceId,station->service[0].serviceId,sizeof(currentServiceId));
          if (playPrimaryService(audioVoice,true)) playPrimaryService(audioVoice,false);
        }
      } else if (lastUpdateResult == UPD_NO_CHANGE) {
        lastDataLoadTime = millis();
        noDataLoaded = false;
        dataLoadSuccess++;
      } else if (lastUpdateResult == UPD_DATA_ERROR || lastUpdateResult == UPD_TIMEOUT || lastUpdateResult == UPD_HTTP_ERROR) {
        lastLoadFailure=millis();
        dataLoadFailure++;
        if (noDataLoaded) showNoDataScreen();
      } else if (lastUpdateResult == UPD_UNAUTHORISED) {
        showTokenErrorScreen();
        while (true) { delay(1);}
      } else {
        dataLoadFailure++;
      }
    }
  }

  if (millis()>timer && numMessages && !isScrollingStops && !isSleeping && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR && !noScrolling && !noDataLoaded) {
    // Need to start a new scrolling messages line
    prevMessage = currentMessage;
    prevScrollStopsLength = scrollStopsLength;
    currentMessage++;
    if (currentMessage>=numMessages) currentMessage=0;
    scrollStopsXpos=0;
    scrollStopsYpos=10;
    setBoardFont(RAIL9,u8g2_clip);  // set font to ensure getStringWidth is correct!
    scrollStopsLength = getStringWidth(line2[currentMessage]);
    isScrollingStops=true;
    if (strncmp("Calling",line2[currentMessage],7)==0) isShowingCalling=true; else isShowingCalling=false;
  }

  // Check if there's a via destination
  if (millis()>viaTimer) {
    if (station->numServices && station->service[0].via[0] && !isSleeping && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
      isShowingVia = !isShowingVia;
      drawPrimaryService(isShowingVia);
      sendBuffer();
      if (isShowingVia) viaTimer = millis()+3000; else viaTimer = millis()+4000;
    }
  }

  if (millis()>serviceTimer && !isScrollingService && !isSleeping && !noServiceClockIsActive && !noDataLoaded && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
    // Need to change to the next service if there is one
    if ((station->numServices <= 1 || (station->numServices==2 && noScrolling)) && !weatherMsg[0] && !showStationName) {
      // There's no other services and no weather so just so static attribution.
      drawServiceLine(1+((station->numServices==2 && noScrolling)?1:0),0,0);
      pushClipAtLine(LINE3);
      serviceTimer = millis() + 30000;
      isScrollingService = false;
    } else {
      if (prevService==-2) prevService=-1; else prevService = line3Service;
      line3Service++;
      int maxServiceLines = station->numServices + ((weatherEnabled && weatherMsg[0])?1:0) + (showStationName?1:0) + 1;
      if (station->numServices) {
        if (line3Service>maxServiceLines) line3Service=(noScrolling && station->numServices>1) ? 2:1;  // First 'other' service
      } else {
        if (line3Service>maxServiceLines) line3Service=0;
      }
      scrollServiceYpos=10;
      isScrollingService = true;
    }
  }

  if (isScrollingStops && millis()>timer && !isSleeping && !noScrolling && !noServiceClockIsActive) {
    clip_canvas.fillScreen(RGB565_BLACK);
    bool isCalling = (strncmp("Calling",line2[currentMessage],7) == 0);
    if (scrollStopsYpos) {
      // we're scrolling up the message initially
      // if the previous message didn't scroll then we need to scroll it up off the screen
      if (prevScrollStopsLength && prevScrollStopsLength<SCREEN_WIDTH) {
        if (strncmp("Calling",line2[prevMessage],7)) centreText(line2[prevMessage],scrollStopsYpos-12,(prevMessage<=lastSvcDescMessage)?colours[RAIL_SERVICEITEMS]:colours[RAIL_MESSAGES],RAIL9);
        else drawStr(0,scrollStopsYpos-12,colours[RAIL_CALLING],line2[prevMessage],RAIL9); // Handle very short calling at lists
      }
      if (scrollStopsLength<SCREEN_WIDTH && !isCalling) centreText(line2[currentMessage],scrollStopsYpos-1,(currentMessage<=lastSvcDescMessage)?colours[RAIL_SERVICEITEMS]:colours[RAIL_MESSAGES],RAIL9); // Centre text if it fits
      else if (isCalling) drawStr(0,scrollStopsYpos-1,colours[RAIL_CALLING],line2[currentMessage],RAIL9);
      else drawStr(0,scrollStopsYpos-1,(currentMessage<=lastSvcDescMessage)?colours[RAIL_SERVICEITEMS]:colours[RAIL_MESSAGES],line2[currentMessage],RAIL9);
      scrollStopsYpos--;
      if (scrollStopsYpos==0) timer=millis()+1500;
    } else {
      // we're scrolling left
      if (scrollStopsLength<SCREEN_WIDTH && !isCalling) centreText(line2[currentMessage],0,(currentMessage<=lastSvcDescMessage)?colours[RAIL_SERVICEITEMS]:colours[RAIL_MESSAGES],RAIL9); // Centre text if it fits
      else drawStr(scrollStopsXpos,0,isCalling?colours[RAIL_CALLING]:(currentMessage<=lastSvcDescMessage)?colours[RAIL_SERVICEITEMS]:colours[RAIL_MESSAGES],line2[currentMessage],RAIL9);
      if (scrollStopsLength < SCREEN_WIDTH) {
        // we don't need to scroll this message, it fits so just set a longer timer
        timer=millis()+6000;
        isScrollingStops=false;
      } else {
        scrollStopsXpos--;
        if (scrollStopsXpos < -scrollStopsLength) {
          isScrollingStops=false;
          timer=millis()+500;  // pause before next message
        }
      }
    }
    pushClipAtLine(LINE2);
  }

  if (isScrollingService && millis()>serviceTimer && !isSleeping && !noServiceClockIsActive) {
    clip_canvas.fillScreen(RGB565_BLACK);
    if (scrollServiceYpos) {
      // we're scrolling the service into view
      // if the prev service is showing, we need to scroll it up off
      if (prevService>=0) drawServiceLine(prevService,0,scrollServiceYpos-12);
      drawServiceLine(line3Service,0,scrollServiceYpos-1);
      pushClipAtLine(LINE3);
      scrollServiceYpos--;
      if (scrollServiceYpos==0) {
        serviceTimer=millis()+5000;
        isScrollingService=false;
      }
    }
  } else if (!isScrollingService && !isSleeping && !noServiceClockIsActive && timeIsDisplayed) {
    // Keep the time running
    if (strcmp(currentTime,displayedTime)) {
      drawServiceLine(line3Service,0,0);
      pushClipAtLine(LINE3);
      strcpy(displayedTime,currentTime);
    }
  }

  if (!isSleeping) {
    // Ensure a consistent refresh rate (for smooth text scrolling).
    // Wait any additional ms not used by processing so far before sending the frame to the display controller
    delayMs = frameTimeRail - (millis()-refreshTimer);
    if (delayMs>0) delay(delayMs);
    if (!firstLoad && noServiceClockIsActive && strcmp(currentTime,displayedTime)) {
      drawNSEclock(colours[NSE_CLOCK]);
      strcpy(displayedTime,currentTime);
    } else sendBuffer();
    refreshTimer=millis();
  }
}

int findServiceInAudioHistory(const char* svcId) {
  for (int i=0;i<MAXTUBEAUDIOHISTORY;++i) {
    if (strcmp(tubeAudioHistory[i].serviceId,svcId) == 0) return i;
  }
  return -1;  // not found;
}

void checkTubeAudioAnnouncement() {

  if (!enableAudioAnnouncements || !station->numServices) return;

  // Check all the services, stop after three announcements on this pass
  int announcements = 0;
  for (int i=0;i<station->numServices;++i) {

    // Work out what the announcement stage is for this service
    tubeAnnouncements thisAnnouncement = NONE;
    if (station->service[i].atPlatform) thisAnnouncement = AT_PLATFORM;
    else if (station->service[i].timeToStation <= 40) thisAnnouncement = APPROACHING;
    else if (station->service[i].timeToStation <= 150) thisAnnouncement = NEXT_TRAIN;

    // Is this service in the history?
    int svcIndex = findServiceInAudioHistory(station->service[i].serviceId);
    if (svcIndex >= 0) {
      log_i("Found service %s in history slot %d stage %d, now at stage %d",station->service[i].serviceId,svcIndex,tubeAudioHistory[svcIndex].lastAnnouncement,thisAnnouncement);
      if (thisAnnouncement > tubeAudioHistory[svcIndex].lastAnnouncement) {
        if (!(tubeAudioHistory[svcIndex].lastAnnouncement == APPROACHING && thisAnnouncement == AT_PLATFORM)) {
          if (playTubeService(audioVoice,i,announcements,true)) playTubeService(audioVoice,i,announcements,false);
          announcements++;
        }
        tubeAudioHistory[svcIndex].lastAnnouncement = thisAnnouncement;
      }
    } else {
      // Not in history, add it to the FIFO list
      log_i("Adding service %s to history slot %d, stage %d",station->service[i].serviceId,tubeAudioHistoryIndex, thisAnnouncement);
      strcpy(tubeAudioHistory[tubeAudioHistoryIndex].serviceId,station->service[i].serviceId);
      tubeAudioHistory[tubeAudioHistoryIndex].lastAnnouncement = thisAnnouncement;

      tubeAudioHistoryIndex = (tubeAudioHistoryIndex + 1) % MAXTUBEAUDIOHISTORY;
      if (thisAnnouncement > NONE) {
        if (playTubeService(audioVoice,i,announcements,true)) playTubeService(audioVoice,i,announcements,false);
        announcements++;
      }
    }

    if (announcements>2) break;
  }

}

//
// Processing loop for London Underground Arrivals board
//
void undergroundArrivalsLoop() {
  int clockX = (SCREEN_WIDTH - 58) / 2;

  if (millis()>nextDataUpdate && !fetchInProgress && !isSleeping && wifiConnected) {
    // Initiate a background update on Core 0
    fetchMode = FETCH_BOARD;
    fetchInProgress = true;
    xTaskNotifyGive(fetchTaskHandle);
    if (firstLoad) {
      waitForFirstLoad();
      strcpy(displayedTime,currentTime);
    }
    if (lastUpdateResult == UPD_NO_CHANGE) lastUpdateResult = UPD_SUCCESS;
  }

  if (fetchComplete && lastUpdateResult == UPD_NO_CHANGE && !isScrollingPrimary && !isSleeping) {
    fetchComplete = false;
    updateArrivals();
    checkTubeAudioAnnouncement();
    // Draw the primary service line(s)
    if (station->numServices) {
      drawUndergroundService(0,0,0,(showTubeCurrentLocation && isShowingVia && station->service[0].currentLocation[0]));
      pushClipAtLine(ULINE1);
      if (station->numServices>1) {
        drawUndergroundService(1,0,0,(showTubeCurrentLocation && isShowingVia && station->service[1].currentLocation[0]));
        pushClipAtLine(ULINE2);
      }
    } else {
      clearCanvas();
      centreText("There are no scheduled arrivals.",ULINE1,colours[TUBE_NOSERVICES],TUBE10,u8g2_main);
    }
  }

  if (fetchComplete && lastUpdateResult != UPD_NO_CHANGE && (!isScrollingService || !showFullMsgs) && !isScrollingPrimary && !isSleeping) {
    fetchComplete = false;
    isScrollingService = false;
    // Get the updated data
    if (lastUpdateResult == UPD_SUCCESS || lastUpdateResult == UPD_SEC_CHANGE) {
      updateArrivals();
      drawUndergroundBoard();
      checkTubeAudioAnnouncement();
    } else if (lastUpdateResult == UPD_DATA_ERROR || lastUpdateResult == UPD_TIMEOUT || lastUpdateResult == UPD_HTTP_ERROR) {
      lastLoadFailure = millis();
      dataLoadFailure++;
      if (noDataLoaded) showNoDataScreen(); else drawUndergroundBoard();
    } else if (lastUpdateResult == UPD_UNAUTHORISED) {
      showTokenErrorScreen();
      while (true) delay(10);
    } else {
      dataLoadFailure++;
    }
  }

  // Check if we're showing currentLocation
  if (showTubeCurrentLocation && millis()>viaTimer) {
    if (station->numServices && station->service[0].currentLocation[0] && !isSleeping && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
      isShowingVia = !isShowingVia;
      drawUndergroundService(0,0,0,isShowingVia);
      pushClipAtLine(ULINE1);
      if (station->numServices>1 && station->service[1].currentLocation[0]) {
        drawUndergroundService(1,0,0,isShowingVia);
        pushClipAtLine(ULINE2);
      }
      if (isShowingVia) viaTimer = millis()+3000; else viaTimer = millis()+8000;
    }
  }

  // Scrolling the additional services
  if (millis()>serviceTimer && !isScrollingService && !isSleeping && !noDataLoaded && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
    if (station->numServices<=2 && numMessages==1 && attributionScrolled) {
      // There are no additional services to scroll in so static attribution.
      serviceTimer = millis() + 30000;
    } else {
      // Need to change to the next service or message if there is one
      attributionScrolled = true;
      prevService = line3Service;
      line3Service++;
      scrollServiceYpos=11;
      scrollStopsXpos=0;
      isScrollingService = true;

      if (line3Service>=station->numServices) {
        // Showing the messages
        prevMessage = currentMessage;
        prevScrollStopsLength = scrollStopsLength;  // Save the length of the previous message
        currentMessage++;

        if (currentMessage>=numMessages) {
          if (station->numServices>2) {
            line3Service=2;
            currentMessage=-1; // Rollover back to services
          } else {
            line3Service = station->numServices;
            currentMessage=0;
          }
        }
        setBoardFont(RAIL9,u8g2_clip);
        scrollStopsLength = getStringWidth(line2[currentMessage]);
      } else {
        scrollStopsLength=SCREEN_WIDTH;
      }
    }
  }

  if (isScrollingService && millis()>serviceTimer && !isSleeping) {
    clip_canvas.fillScreen(RGB565_BLACK);
    timeIsDisplayed = false;
    if (scrollServiceYpos) {
      // we're scrolling up the message initially
      // Was the previous display a service?
      if (prevService<station->numServices) {
        drawUndergroundService(prevService,0,scrollServiceYpos-12);
      } else {
        // if the previous message didn't scroll then we need to scroll it up off the screen
        if (prevScrollStopsLength && prevScrollStopsLength<SCREEN_WIDTH) {
          if (strcmp(line2[prevMessage],"88:88:88")==0) { drawStr(clockX,scrollServiceYpos-11,colours[TUBE_CLOCK],currentTime,TUBECLOCK8); }
          else if (showStationName && prevService == station->numServices) centreText(line2[prevMessage],scrollServiceYpos-11,colours[TUBE_STATION],stationNameFont);
          else if (line2[prevMessage][0] == '%') centreText(weatherMsg,scrollServiceYpos-11,colours[TUBE_WEATHER],RAIL9);
          else centreText(line2[prevMessage],scrollServiceYpos-11,colours[TUBE_MESSAGES],RAIL9);
        }
      }
      // Is this entry a service?
      if (line3Service<station->numServices) {
        drawUndergroundService(line3Service,0,scrollServiceYpos-1);
      } else {
        if (scrollStopsLength<SCREEN_WIDTH) {
          if (strcmp(line2[currentMessage],"88:88:88")==0) { drawStr(clockX,scrollServiceYpos,colours[TUBE_CLOCK],currentTime,TUBECLOCK8); timeIsDisplayed = true; }
          else if (showStationName && line3Service == station->numServices) centreText(line2[currentMessage],scrollServiceYpos,colours[TUBE_STATION],stationNameFont);
          else if (line2[currentMessage][0] == '%') centreText(weatherMsg,scrollServiceYpos,colours[TUBE_WEATHER],RAIL9);
          else centreText(line2[currentMessage],scrollServiceYpos,colours[TUBE_MESSAGES],RAIL9); // Centre text if it fits
        }
        else {
          drawStr(0,scrollServiceYpos,colours[TUBE_MESSAGES],line2[currentMessage],RAIL9);
        }
      }
      pushClipAtLine(ULINE3);
      scrollServiceYpos--;
      if (scrollServiceYpos==0) {
        if (line3Service<station->numServices) {
          serviceTimer=millis()+3500;
          isScrollingService=false;
        } else {
          serviceTimer=millis()+500;
        }
      }
    } else {
      // we're scrolling left
      if (scrollStopsLength<SCREEN_WIDTH) {
        if (strcmp(line2[currentMessage],"88:88:88")==0) { drawStr(clockX,1,colours[TUBE_CLOCK],currentTime,TUBECLOCK8); timeIsDisplayed = true; }
        else if (showStationName && line3Service == station->numServices) centreText(line2[currentMessage],1,colours[TUBE_STATION],stationNameFont);
        else if (line2[currentMessage][0] == '%') centreText(weatherMsg,1,colours[TUBE_WEATHER],RAIL9);
        else centreText(line2[currentMessage],1,colours[TUBE_MESSAGES],RAIL9); // Centre text if it fits
      }
      else {
        drawStr(scrollStopsXpos,1,colours[TUBE_MESSAGES],line2[currentMessage],RAIL9);
      }
      pushClipAtLine(ULINE3);
      if (scrollStopsLength < SCREEN_WIDTH) {
        // we don't need to scroll this message, it fits so just set a longer timer
        serviceTimer=millis()+3000;
        if (timeIsDisplayed) serviceTimer += 3000;
        isScrollingService=false;
      } else {
        scrollStopsXpos--;
        if (scrollStopsXpos < -scrollStopsLength) {
          isScrollingService=false;
          serviceTimer=millis()+500;  // pause before next message
        }
      }
    }
  } else if (!isScrollingService && !isSleeping && timeIsDisplayed) {
    if (strcmp(currentTime,displayedTime)) {
      // Keep the time running
      clearCanvas(clip_canvas);
      drawStr(clockX,1,colours[TUBE_CLOCK],currentTime,TUBECLOCK8);
      pushClipAtLine(ULINE3);
      strcpy(displayedTime,currentTime);
    }
  }

  if (isScrollingPrimary && !isSleeping) {
    clip_canvas.fillScreen(RGB565_BLACK);
    // we're scrolling the primary service(s) into view
    if (station->numServices) drawUndergroundService(0,0,scrollPrimaryYpos-1);
    else {
      centreText("There are no scheduled arrivals.",scrollPrimaryYpos-1,colours[TUBE_NOSERVICES],TUBE10);
    }
    pushClipAtLine(ULINE1);
    if (station->numServices>1) {
      drawUndergroundService(1,0,scrollPrimaryYpos-1);
      pushClipAtLine(ULINE2);
    }
    scrollPrimaryYpos--;
    if (scrollPrimaryYpos==0) {
      isScrollingPrimary=false;
    }
  }

  if (!isSleeping) {
    delayMs = frameTimeTube - (millis()-refreshTimer);
    if (delayMs>0) delay(delayMs);
    sendBuffer();
    refreshTimer=millis();
  }
}


//
// Processing loop for Bus Departures board
//
void busDeparturesLoop() {
  int clockX = (SCREEN_WIDTH - 58) / 2;

  if (millis()>nextDataUpdate && !fetchInProgress && !isSleeping && wifiConnected) {
    // Initiate a background update on Core 0
    fetchMode = FETCH_BOARD;
    fetchInProgress = true;
    xTaskNotifyGive(fetchTaskHandle);
    if (firstLoad) {
      waitForFirstLoad();
      strcpy(displayedTime,currentTime);
    }
    if (lastUpdateResult == UPD_NO_CHANGE) lastUpdateResult = UPD_SUCCESS;
  }

  if (fetchComplete && lastUpdateResult == UPD_NO_CHANGE) {
    fetchComplete=false;
    updateBusDepartures();
    // Draw the primary service line(s)
    if (station->numServices) {
      drawBusService(0,0,busDestX);
      pushClipAtLine(LINE1);
      if (station->numServices>1) { drawBusService(1,0,busDestX); pushClipAtLine(LINE2); }
    } else {
      clearCanvas();
      centreText("There are no scheduled services.",LINE1-1,colours[BUS_NOSERVICES],RAIL9,u8g2_main);
    }
  }

  if (fetchComplete && lastUpdateResult != UPD_NO_CHANGE && !isScrollingService && !isScrollingPrimary && !isSleeping) {
    fetchComplete = false;
    if (lastUpdateResult == UPD_SUCCESS) {
      updateBusDepartures();
      drawBusDeparturesBoard();
    } else if (lastUpdateResult == UPD_DATA_ERROR || lastUpdateResult == UPD_TIMEOUT || lastUpdateResult == UPD_HTTP_ERROR) {
      lastLoadFailure = millis();
      dataLoadFailure++;
      if (noDataLoaded) showNoDataScreen(); else drawBusDeparturesBoard();
    } else if (lastUpdateResult == UPD_UNAUTHORISED) {
      showTokenErrorScreen();
      while (true) delay(10);
    } else {
      dataLoadFailure++;
    }
  }

  // Scrolling the additional services
  if (millis()>serviceTimer && !isScrollingPrimary && !isScrollingService && !isSleeping && !noDataLoaded && lastUpdateResult!=UPD_UNAUTHORISED && lastUpdateResult!=UPD_DATA_ERROR) {
    // Need to change to the next service if there is one
    if (station->numServices<=2 && messages->numMessages==1) {
      // There are no additional services or weather to scroll in so static attribution.
      serviceTimer = millis() + 10000;
      line3Service=station->numServices;
    } else {
      // Need to change to the next service or message
      prevService = line3Service;
      line3Service++;
      scrollServiceYpos=11;
      isScrollingService = true;
      if (line3Service>=station->numServices) {
        // Showing the messages
        prevMessage = currentMessage;
        currentMessage++;
        if (currentMessage>=messages->numMessages) {
          if (station->numServices>2) {
            line3Service = 2;
            currentMessage=-1; // Rollover back to services
          } else {
            line3Service = station->numServices;
            currentMessage=0;
          }
        }
      }
    }
  }

  if (isScrollingService && millis()>serviceTimer && !isSleeping) {
    clip_canvas.fillScreen(RGB565_BLACK);
    timeIsDisplayed = false;
    if (scrollServiceYpos) {
      // we're scrolling up the message
      // Was the previous display a service?
      if (prevService<station->numServices) {
        drawBusService(prevService,scrollServiceYpos-12,busDestX);
      } else {
        // Scrolling up the previous message
        if (strcmp(line2[prevMessage],"88:88:88")==0) { drawStr(clockX,scrollServiceYpos-12,colours[BUS_CLOCK],currentTime,TUBECLOCK8); }
        else if (showStationName && prevService == station->numServices) centreText(line2[prevMessage],scrollServiceYpos-12,colours[BUS_STATION],stationNameFont);
        else if (line2[prevMessage][0] == '%') centreText(weatherMsg,scrollServiceYpos-12,colours[BUS_WEATHER],RAIL9);
        else centreText(line2[prevMessage],scrollServiceYpos-12,colours[BUS_MESSAGES],RAIL9);
      }
      // Is this entry a service?
      if (line3Service<station->numServices) {
        drawBusService(line3Service,scrollServiceYpos-1,busDestX);
      } else {
        if (strcmp(line2[currentMessage],"88:88:88")==0) { drawStr(clockX,scrollServiceYpos-1,colours[BUS_CLOCK],currentTime,TUBECLOCK8); timeIsDisplayed = true; }
        else if (showStationName && line3Service == station->numServices) centreText(line2[currentMessage],scrollServiceYpos-1,colours[BUS_STATION],stationNameFont);
        else if (line2[currentMessage][0] == '%') centreText(weatherMsg,scrollServiceYpos-1,colours[BUS_WEATHER],RAIL9);
        else centreText(line2[currentMessage],scrollServiceYpos-1,colours[BUS_MESSAGES],RAIL9); // Centre text if it fits
      }
      pushClipAtLine(LINE3);
      scrollServiceYpos--;
      if (scrollServiceYpos==0) {
        isScrollingService = false;
        serviceTimer = millis()+2800;
        if (station->numServices<=2 || timeIsDisplayed) serviceTimer+=3000;
      }
    }
  } else if (!isScrollingService && !isSleeping && timeIsDisplayed) {
    if (strcmp(currentTime,displayedTime)) {
      // Keep the time running
      clearCanvas(clip_canvas);
      drawStr(clockX,0,colours[BUS_CLOCK],currentTime,TUBECLOCK8);
      pushClipAtLine(LINE3);
      strcpy(displayedTime,currentTime);
    }
  }

  if (isScrollingPrimary && !isSleeping) {
    clip_canvas.fillScreen(RGB565_BLACK);
    // we're scrolling the primary service(s) into view
    if (station->numServices) drawBusService(0,scrollPrimaryYpos-1,busDestX);
    else centreText("There are no scheduled services.",scrollPrimaryYpos-1,colours[BUS_NOSERVICES],RAIL9);
    pushClipAtLine(LINE1);
    if (station->numServices>1) {
      drawBusService(1,scrollPrimaryYpos-1,busDestX);
      pushClipAtLine(LINE2);
    }
    if (station->numServices>2) {
      clip_canvas.fillScreen(RGB565_BLACK);
      drawBusService(2,scrollPrimaryYpos-1,busDestX);
    }
    pushClipAtLine(LINE3);
    scrollPrimaryYpos--;
    if (scrollPrimaryYpos==0) {
      isScrollingPrimary=false;
      serviceTimer = millis()+2800;
    }
  }

  if (!isSleeping) {
    delayMs = frameTimeBus - (millis()-refreshTimer);
    if (delayMs>0) delay(delayMs);
    sendBuffer();
    refreshTimer=millis();
  }
}

// The Core 0 Background Data Fetch Task
void fetchDeparturesTask(void *pvParameters) {
  while(true) {
    // Put task to sleep until triggered by Core 1
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Perform the requested data update...
    fetchInProgress = true;
    log_i("Start fetch on Core 0 for fetchMode %d",fetchMode);
    switch (fetchMode) {
      case FETCH_BOARD:
        switch (boardMode) {
          case MODE_RAIL:
            if (useRDMclient) {
              lastUpdateResult = rdmRailData->fetchDepartures(station,messages,locationCode,rdmDeparturesApiKey,rdmServiceApiKey,MAXBOARDSERVICES,enableBus,callingCrsCode,locationCleanFilter,nrTimeOffset,(showLastSeen && !noScrolling),showServiceMsgs);
            } else {
              lastUpdateResult = darwinRailData->fetchDepartures(station,messages,locationCode,nrToken,MAXBOARDSERVICES,enableBus,callingCrsCode,locationCleanFilter,nrTimeOffset,(showLastSeen && !noScrolling),showServiceMsgs);
            }
            nextDataUpdate = millis()+apiRefreshRate;
            break;
          case MODE_TUBE:
            lastUpdateResult = tfldata->fetchArrivals(station,messages,locationCode,lineId,lineDirection,(noScrolling || !showServiceMsgs),tflAppKey);
            nextDataUpdate = millis() + UGDATAUPDATEINTERVAL; // default update freq
            break;
          case MODE_BUS:
            lastUpdateResult = busdata->fetchDepartures(station,locationCode,locationCleanFilter);
            nextDataUpdate = millis() + BUSDATAUPDATEINTERVAL;
            break;
        }
        fetchComplete = true;
        break;

      case FETCH_WEATHER:
        // Update the weather forecast
        lastWeatherUpdateResult = currentWeather->updateWeather(openWeatherMapApiKey, locationLat, locationLon);
        nextWeatherUpdate = millis() + WEATHERUPDATEINTERVAL; // update every 20 mins
        weatherFetchComplete = true;
        break;

      case FETCH_RSS:
        // Update the RSS headlines
        lastRssUpdateResult=rss->loadFeed(rssURL);
        nextRssUpdate = millis() + RSSUPDATEINTERVAL;
        rssFetchComplete = true;
        break;
    }

    // Signal to Core 1 that the fetch is complete
    fetchInProgress = false;
    log_i("Finish fetch on Core 0");
  }
}

// The Core 0 Background Audio Player Task
void AudioTask(void *pvParameters) {
    AudioCommand cmd;

    for (;;) {
        // Start new track if idle
        if (!wavFile) {
            if (xQueueReceive(audioQueue, &cmd, 0) == pdTRUE) {
                if (cmd.delay) vTaskDelay(cmd.delay);   // pause before playback
                wavFile = SD_MMC.open(cmd.filename, "r");
                if (wavFile) {
                    i2s.writeSilence(1024);
                    decoderStream.begin();
                    copier.begin(decoderStream, wavFile);
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(10)); // idle only
            }
        }

        // Actively playing
        size_t bytesCopied = copier.copy();

        // EOF detection
        if (!wavFile.available() && !bytesCopied) {
            i2s.flush();
            i2s.writeSilence(256);
            wavFile.close();
            decoderStream.end();
        }

        taskYIELD();
    }
}

void setup() {

  createSharedDataStructures();
  createDataClients();

  // These are the default wsdl XML SOAP entry points. They can be overridden in the config.json file if necessary
  strlcpy(wsdlHost,"lite.realtime.nationalrail.co.uk",sizeof(wsdlHost));
  strlcpy(wsdlAPI,"/OpenLDBWS/wsdl.aspx?ver=2021-11-01",sizeof(wsdlAPI));
  String buildDate = String(__DATE__);
  char notice[30];
  sprintf(notice,"\xA9 %s Gadec Software", buildDate.substring(buildDate.length()-4).c_str());

  bool isFSMounted = LittleFS.begin(true);    // Start the File System, format if necessary
  strcpy(station->location,"");               // No default location
  strcpy(weatherMsg,"");                      // No weather message
  strcpy(nrToken,"");                         // No default National Rail token
  strcpy(tflAppKey,"");                       // No default TfL app_key
  loadApiKeys();                              // Load the API keys from the apiKeys.json
  loadConfig(true);                           // Load the configuration settings from config.json

  setupMatrix();
  u8g2_main.begin(main_canvas);
  u8g2_clip.begin(clip_canvas);
  u8g2_main.setFontMode(1);
  u8g2_clip.setFontMode(1);
  main_canvas.fillScreen(RGB565_BLACK);
  clip_canvas.fillScreen(RGB565_BLACK);
  dma_display->setBrightness(brightness);
  if (flipScreen) panel->setRotation(0);

  main_canvas.drawRGBBitmap(0, 0, GadecLogo, gadeclogo_width, gadeclogo_height);
  centreText(notice,7,RGB565_WHITE,RAIL9,u8g2_main,80,SCREEN_WIDTH-80);
  centreText("github.com/gadec-uk",18,RGB565_BLUE,RAIL9,u8g2_main,80,SCREEN_WIDTH-80);
  sendBuffer();

  // Initialise default font for clip canvas
  setBoardFont(RAIL9,u8g2_clip);

  if (!(cardReaderReady = setupCardReader())) enableAudioAnnouncements = false;

  if (enableAudioAnnouncements) {
    if (setupAudioSystem()) {
      // set the volume to the saved value
      es.WriteReg(0x32,audioVolume);

      // Create Inter-Core Queue in PSRAM, holds up to MAX_QUEUE_SIZE (60) play requests.
      audioQueue = xQueueCreateWithCaps(MAX_QUEUE_SIZE, sizeof(AudioCommand), MALLOC_CAP_SPIRAM);

      // Create background audio task on Core 0
      xTaskCreatePinnedToCore(
          AudioTask,       // Task function
          "AudioTask",     // Task name
          8192,            // Stack size in words
          NULL,            // Parameters
          2,               // Priority (Higher priority than background tasks)
          NULL,            // Task handle
          0                // Core ID: 0
      );
    } else {
      // setup failed, disable audio
      enableAudioAnnouncements = false;
    }
  }

  delay(5000);
  clearCanvas();
  drawStartupHeading();
  progressBar("Connecting to Wi-Fi",20);
  WiFi.mode(WIFI_MODE_NULL);        // Reset the WiFi
  WiFi.setSleep(WIFI_PS_NONE);      // Turn off WiFi Powersaving
  WiFi.hostname(hostname);          // Set the hostname ("Departures Board")
  WiFi.mode(WIFI_STA);              // Enter WiFi station mode

  WiFiManager wm;                             // Start WiFiManager
  wm.setAPCallback(wmConfigModeCallback);     // Set the callback for config mode notification
  wm.setWiFiAutoReconnect(true);              // Attempt to auto-reconnect WiFi
  wm.setConnectTimeout(8);
  wm.setConnectRetries(2);
  std::vector<const char *> menu = {"wifi","exit"};
  wm.setMenu(menu);

  bool result = wm.autoConnect("Departures Board");    // Attempt to connect to WiFi (or enter interactive configuration mode)
  if (!result || wifiConfigured) {
    // Need to restart after config (cannot reuse port)
    ESP.restart();
  }

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
  }

  // Get our IP address and store
  updateMyUrl();
  log_i("My URL: %s",myUrl);
  if (MDNS.begin(hostname)) {
    MDNS.addService("http","tcp",80);
  }

  wifiConnected=true;
  WiFi.setAutoReconnect(true);
  clearCanvas();
  drawStartupHeading();
  progressBar("Wi-Fi Connected",30);

  // Configure the local webserver paths
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){handleRoot(request);});
  server.on("/erasewifi", HTTP_GET, [](AsyncWebServerRequest *request){handleEraseWiFi(request);});
  server.on("/factoryreset", HTTP_GET, [](AsyncWebServerRequest *request){handleFactoryReset(request);});
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request){handleInfo(request);});
  server.on("/formatffs", HTTP_GET, [](AsyncWebServerRequest *request){handleFormatFFS(request);});
  server.on("/dir", HTTP_GET, [](AsyncWebServerRequest *request){handleFileList(request);});
  server.onNotFound([](AsyncWebServerRequest *request){handleNotFound(request);});
  server.on("/cat", HTTP_GET, [](AsyncWebServerRequest *request){handleCat(request);});
  server.on("/del", HTTP_GET, [](AsyncWebServerRequest *request){handleDelete(request);});
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){handleReboot(request);});
  server.on("/stationpicker", HTTP_GET, [](AsyncWebServerRequest *request){handleStationPicker(request);});
  server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request){handleFirmwareInfo(request);});
  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request){handleBrightness(request);});
  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request){handleOtaUpdate(request);});
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){handleControl(request);});
  server.on("/voices", HTTP_GET, [](AsyncWebServerRequest *request){handleVoices(request);});
  server.on("/success", HTTP_GET, [](AsyncWebServerRequest *request){handleStreamGzipFlashFile(request->url(), successhtm, sizeof(successhtm),request);});
  server.on("/vol", HTTP_GET, [](AsyncWebServerRequest *request){handleVolume(request);});
  server.on("/getthemes", HTTP_GET, [](AsyncWebServerRequest *request){handleGetThemes(request);});
  server.on("/loadtheme", HTTP_GET, [](AsyncWebServerRequest *request){handleLoadTheme(request);});

  //
  // Save settings returned by the Web GUI
  //
  server.on("/savesettings", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->_tempObject) {
      String* body = (String*)(request->_tempObject);
      saveFile("/config.json", body->c_str());

      delete body; // Clean up memory
      request->_tempObject = nullptr;

      if ((!railIsSet && !tubeIsSet && !busIsSet) || (!nrToken[0] && rdmDeparturesApiKey=="" && boardMode==MODE_RAIL) || request->hasParam("reboot")) {
        // First time setup or base config change, we need a full reboot
        sendResponse(200,"Configuration saved. The Departures Board will now restart.",request);
        restartTimer.once(1, []() { ESP.restart(); });
      } else {
        sendResponse(200,"Configuration updated. The Departures Board will update shortly.",request);
        softResetNeeded = true;
      }
    } else {
      sendResponse(400,"Empty",request);
    }
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index) {
      // First chunk: Create a String object in RAM
      request->_tempObject = new String("");
    }

    String* body = (String*)(request->_tempObject);
    for (size_t i = 0; i < len; i++) {
      body->concat((char)data[i]);
    }
  });

  //
  // Save the API keys returned from the Web GUI
  //
  server.on("/savekeys", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->_tempObject) {
      String* body = (String*)(request->_tempObject);

      JsonDocument doc;
      bool result = true;
      String msg = "The API keys have been saved successfully.";
      DeserializationError error = deserializeJson(doc, body->c_str());
      if (!error) {
        if (!saveFile("/apikeys.json", body->c_str())) {
          msg = "Failed to save the API keys to the file system (file system corrupt or full?)";
          result = false;
        } else {
          JsonObject settings = doc.as<JsonObject>();
          String nrToken = settings["nrToken"].as<String>();
          String rdmDepToken = settings["rdmDepKey"].as<String>();
          if (!nrToken.length() && !rdmDepToken.length()) msg+="\n\nNote: Only Tube and Bus Departures will be available without either Rail Data or National Rail keys.";
        }
      } else {
        msg = "Invalid JSON format. No changes have been saved.";
        result = false;
      }

      delete body; // Clean up memory
      request->_tempObject = nullptr;

      if (result) {
        // Load/Update the API Keys in memory
        loadApiKeys();
        // If all location codes are blank we're in the setup process. If not, the keys have been changed so just reboot.
        if (!railIsSet && !tubeIsSet && !busIsSet) {
          sendResponse(200,msg,request);
          writeDefaultConfig();
          showSetupCrsHelpScreen();
        } else {
          msg += "\n\nThe Departures Board will now restart.";
          sendResponse(200,msg,request);
          restartTimer.once(1, []() { ESP.restart(); });
        }
      } else {
        sendResponse(400,msg,request);
      }
    } else {
      sendResponse(400,"Empty",request);
    }
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index) {
      // First chunk: Create a String object in RAM
      request->_tempObject = new String("");
    }

    String* body = (String*)(request->_tempObject);
    for (size_t i = 0; i < len; i++) {
      body->concat((char)data[i]);
    }
  });

  //
  // Handle uploads to LittleFS
  //
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request){handleStreamGzipFlashFile(request->url(), uploadhtm, sizeof(uploadhtm),request);});
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->redirect("/success");
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      String path = "/" + filename;
      if (LittleFS.exists(path)) LittleFS.remove(path);
      size_t fileSize = request->header("Content-Length").toInt();
      size_t availableSpace = LittleFS.totalBytes() - LittleFS.usedBytes() - 1024;

      if (fileSize > availableSpace) {
          sendResponse(507,"Insufficient storage space in File System",request);
          request->client()->close();
          return;
      }
      // First chunk: Create/Open the file and store the handle in _tempObject
      // We use a pointer to a File object so we can keep it open between chunks
      File *file = new File(LittleFS.open(path, FILE_WRITE));
      if (!*file) {
        sendResponse(500,"File System Error",request);
        request->client()->close();
        return;
      }
      request->_tempObject = file;
    }

    // If we have a valid file handle, write the current chunk
    if (len && request->_tempObject) {
      File *file = reinterpret_cast<File *>(request->_tempObject);
      file->write(data, len);
    }

    if (final && request->_tempObject) {
      // Last chunk: Close the file and clean up the pointer
      File *file = reinterpret_cast<File *>(request->_tempObject);
      file->close();
      delete file;
      request->_tempObject = nullptr;
    }
  });

  //
  // Handle manual firmware updates at /update
  //
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){handleStreamGzipFlashFile(request->url(), updatehtm, sizeof(updatehtm),request);});
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Check if the Update library encountered any errors.
    bool shouldReboot = !Update.hasError();

    // Create a response. The AJAX script is just looking for a successful HTTP status.
    AsyncWebServerResponse *response = request->beginResponse((shouldReboot ? 200 : 500), "text/plain", (shouldReboot ? "OK" : "FAIL"));
    response->addHeader("Connection", "close");
    request->send(response);

    // If successful, restart the ESP32 to boot into the new firmware
    if (shouldReboot) restartTimer.once(0.5, []() { ESP.restart(); });
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      // First chunk: Initialize the OTA Update
      // UPDATE_SIZE_UNKNOWN tells the library to just accept chunks until 'final' is true
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        sendResponse(500,"Update begin failed",request);
      }
    }

    // Write chunk data to the flash memory
    if (!Update.hasError() && len) {
      if (Update.write(data, len) != len) {
        sendResponse(500,"Update write failed",request);
      }
    }

    // Final chunk: Close the OTA process
    if (final) {
      if (!Update.end(true)) {
        sendResponse(500,"Update end failed",request);
      }
    }
  });

  server.begin();     // Start the local web server

  // Check for Firmware updates?
  if (firmwareUpdates) {
    progressBar("Checking for firmware updates",40);
    if (ghUpdate->getLatestRelease()==UPD_SUCCESS) {
      checkForFirmwareUpdate();
    } else {
      char msg[200];
      sprintf(msg,"Unable to get the latest release information (%s)",jsonKeyBuffer->lastResultMessage);
      showUpdateCompleteScreen("Firmware Update Aborted",msg,15,false);
      clearCanvas();
      drawStartupHeading();
      sendBuffer();
    }
  }
  checkPostWebUpgrade();
  // First time configuration?
  if ((!railIsSet && !tubeIsSet && !busIsSet) || (!nrToken[0] && rdmDeparturesApiKey=="" && boardMode==MODE_RAIL)) {
    if (!apiKeys) showSetupKeysHelpScreen();
    else showSetupCrsHelpScreen();
    // First time setup mode will exit with a reboot, so just loop here forever
    while (true) { delay(10); }
  }

  configTzTime(ukTimezone, "uk.pool.ntp.org","time.cloudflare.com","time.windows.com");
  if (timezone!="") {
    setenv("TZ",timezone.c_str(),1);
    tzset();
  }

  // Check the clock has been set successfully before continuing
  int p=50;
  int ntpAttempts=0;
  bool ntpResult=true;
  progressBar("Setting the system clock...",50);
  if(!getLocalTime(&timeinfo,2000)) {              // attempt to set the clock from NTP
    do {
      ntpResult = getLocalTime(&timeinfo,2000);
      ntpAttempts++;
      p+=5;
      progressBar("Setting the system clock...",p);
      if (p>80) p=45;
    } while ((!ntpResult) && (ntpAttempts<10));
  }
  if (!ntpResult) {
    // Sometimes NTP/UDP fails. A reboot usually fixes it.
    progressBar("Failed to set the clock. Rebooting in 5 sec.",0);
    delay(5000);
    ESP.restart();
  }
  prevUpdateCheckDay = timeinfo.tm_mday;
  sprintf(currentTime,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);

  // Reload settings (clock has now been set)
  loadConfig();
  resetTubeAudioHistory();

  // Start FTP Server?
  if (enableFTP && cardReaderReady) {
    ftpSvr = new FtpServer();
    if (ftpSvr) {
      ftpSvr->begin("admin","depboard","Matrix Departure Board SD Card");
    } else {
      enableFTP = false;
    }
  }

  setBoardFont(stationNameFont = RAIL9W, u8g2_clip);
  if (getStringWidth(locationName) > SCREEN_WIDTH) stationNameFont = RAIL9;

  station->numServices=0;
  currentServiceId[0] = '\0';
  if (rssEnabled && boardMode!=MODE_BUS) {
    progressBar("Loading RSS headlines feed",60);
    updateRssFeed();
  }

  if (weatherEnabled) {
    progressBar("Getting weather conditions",64);
    updateCurrentWeather(locationLat,locationLon);
  }

  // Create the background task pinned to Core 0
  xTaskCreatePinnedToCore(
    fetchDeparturesTask,  // Task function
    "FetchTask",          // Task name
    10240,                // Stack size (CRITICAL: Needs to be large for SSL/XML)
    NULL,                 // Task parameters
    1,                    // Priority
    &fetchTaskHandle,     // Task handle
    0                     // Core 0 (Network/Background core)
  );

  if (boardMode == MODE_RAIL) {
      if (!useRDMclient) {
        // Using legacy darwin XML client
        progressBar("Initialising National Rail interface",67);
        int res = darwinRailData->init(wsdlHost, wsdlAPI);
        if (res != UPD_SUCCESS) {
          showWsdlFailureScreen();
          while (true) {delay(1);}
        }
      }
      progressBar("Initialising National Rail interface",70);
      rdmRailData->cleanFilter(locationFilter,locationCleanFilter,MAXFILTERSIZE);
      startupProgressPercent=70;
  } else if (boardMode == MODE_TUBE) {
      progressBar("Initialising TfL interface",70);
      startupProgressPercent=70;
  } else if (boardMode == MODE_BUS) {
      progressBar("Initialising BusTimes interface",70);
      // Create a cleaned filter
      busdata->cleanFilter(locationFilter,locationCleanFilter,MAXFILTERSIZE);
      startupProgressPercent=70;
  }
}

void loop() {

  if (touchEnabled) button.updateTouchState();

  if (button.wasShortTapped()) {
    if (isSleeping) {
      if (NSEclockIsActive) NSEclockIsActive = false;
      else forcedAwake = true;
    } else {
      switchToNextMode();
    }
  } else if (button.wasLongTapped() && longPressClock) {
    NSEclockIsActive = !NSEclockIsActive;
  }

  if (millis()-lastTimeUpdate >= 100) {
    // Update the current time
    int prevSecond = timeinfo.tm_sec;
    if (getLocalTime(&timeinfo)) {
      sprintf(currentTime,"%02d:%02d:%02d",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
      lastTimeUpdate = millis();
      if (millis()>3888000000 && timeinfo.tm_hour==3) ESP.restart(); // Reboot every 45 days at 3am
      if (isSleeping && (useNSEclockForSleep || NSEclockIsActive) && prevSecond != timeinfo.tm_sec) drawNSEclock(colours[NSE_CLOCK]);  // Update the large clock
    }
  }

  // Check for firmware updates daily if enabled
  if (dailyUpdateCheck && !fetchInProgress && millis()>fwUpdateCheckTimer) {
    fwUpdateCheckTimer = millis() + 3300000 + random(600000); // check again in 55 to 65 mins
    if (timeinfo.tm_mday != prevUpdateCheckDay) {
      if (ghUpdate->getLatestRelease()==UPD_SUCCESS) {
        checkForFirmwareUpdate();
      }
      prevUpdateCheckDay = timeinfo.tm_mday;
    }
  }

  bool wasSleeping = isSleeping;
  isSleeping = isSnoozing();

  if (isSleeping && !useNSEclockForSleep && !NSEclockIsActive && millis()>timer) {
    if (!wasSleeping) panel->clearScreen();
    timer=millis() + SCREENSAVERINTERVAL;
  } else if (wasSleeping && !isSleeping) {
    // Exit sleep mode cleanly
    softResetBoard(MODE_LOADCONFIG);
  }

  // WiFi Status check
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    wifiConnected=false;
  } else if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
    wifiConnected=true;
    updateMyUrl();  // in case our IP changed
  }

  // Force a manual reset if we've been disconnected for more than 10 secs
  if (WiFi.status() != WL_CONNECTED && millis() > lastWiFiReconnect+10000) {
    WiFi.disconnect();
    delay(100);
    WiFi.reconnect();
    lastWiFiReconnect=millis();
  }

  switch (boardMode) {
    case MODE_RAIL:
      departureBoardLoop();
      break;

    case MODE_TUBE:
      undergroundArrivalsLoop();
      break;

    case MODE_BUS:
      busDeparturesLoop();
      break;
  }

  if (manualUpdateCheck && !fetchInProgress) doManualOtaCheck();

  if (rssEnabled && boardMode != MODE_BUS && millis() > nextRssUpdate && !fetchInProgress && !isSleeping && wifiConnected) {
    // Start an RSS Update on Core 0
    fetchMode = FETCH_RSS;
    fetchInProgress = true;
    xTaskNotifyGive(fetchTaskHandle);
  }

  if (rssFetchComplete) {
    // Background fetch has completed
    rssFetchComplete = false;
    if (lastRssUpdateResult == UPD_SUCCESS) buildRssMessage();
  }

  if (weatherEnabled && millis()>nextWeatherUpdate && !fetchInProgress && locationLat && locationLon && !isSleeping && wifiConnected) {
    // Start a weather update on Core 0
    fetchMode = FETCH_WEATHER;
    fetchInProgress = true;
    xTaskNotifyGive(fetchTaskHandle);
  }

  if (weatherFetchComplete) {
    weatherFetchComplete = false;
    if (lastWeatherUpdateResult == UPD_SUCCESS) {
      strlcpy(weatherMsg,currentWeather->currentWeatherMessage,MAXWEATHERSIZE);
    } else {
      weatherMsg[0] = '\0';
    }
  }

  if (softResetNeeded && !fetchInProgress) {
    softResetNeeded=false;
    softResetBoard(MODE_LOADCONFIG);
  }

  if ((schedulerActive || (carouselActive && numCarouselSlots>1)) && !isSleeping && !fetchInProgress && millis() > nextSchedulerCheck) {
    int nowTime = getTimeInMinutes();
    if ((activeSlotEventTime < nextSlotEventTime && nowTime >= nextSlotEventTime) || (activeSlotEventTime > nextSlotEventTime && nowTime < activeSlotEventTime && nowTime >= nextSlotEventTime)) {
      if (carouselActive) currentCarouselSlot = (currentCarouselSlot + 1) % numCarouselSlots;
      softResetBoard(MODE_LOADCONFIG);
    }
    nextSchedulerCheck = millis() + 10000;  // ten seconds
  }

  if (enableFTP) ftpSvr->handleFTP();

}