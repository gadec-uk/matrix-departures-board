# matrix-departures-board [![License Badge](https://img.shields.io/badge/BY--NC--SA%204.0%20License-grey?style=flat&logo=creativecommons&logoColor=white)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

This is a LED Matrix Departures Board replicating those at many UK railway stations (using data provided by National Rail's public API), London Underground Arrivals boards (using data provided by TfL) and UK wide bus stops (using data provided by bustimes.org). This implementation uses a [Waveshare ESP32-S3 RGB Matrix Driver Board](https://www.waveshare.com/esp32-s3-rgb-matrix.htm) together with three 64x32 LED Matrix HUB75 panels. Two speech-packs are included (male and female) providing station announcements for all National Rail and Tube stations.

<img src="https://github.com/user-attachments/assets/751090fc-2bf5-4157-8e45-4f9fe757c80f" style="display:block; margin:0 auto;">

A table-top version using a 3.12" OLED panel is also available [here](https://github.com/gadec-uk/departures-board).

## Features
* All processing is done onboard by the ESP32-S3 processor, no middleware servers.
* Two speech-packs are included to provide station audio announcements for all 2600+ National Rail stations and 300+ London Underground stations, plus common delay and cancellation reasons.
* Optional station audio reverb effect (for that authentic atmosphere!).
* Support for touch sensor to switch modes / stations / wake from screensaver.
* Smooth animation matching the real departures and arrivals boards.
* Displays up to the next 9 departures with scheduled time, platform number, destination, calling stations and expected departure time.
* Optionally display the last reported location of a service.
* Optionally only show services calling at a selected railway station.
* Scheduler and Carousel modes to automatically switch between any combination of railway, tube and bus stops.
* Displays Network Rail service messages.
* Train information (operator, class, number of coaches etc.).
* Displays up to the next 9 arrivals with time to station (London Underground mode).
* Optionally display the current location of the train (London Underground mode).
* TfL station and network service messages (London Underground mode).
* Optionally filter by tube line and direction.
* In Bus mode, displays up to the next 9 departures with service number, destination, vehicle registration and schedule/expected time.
* Optionally display RSS headline feeds with UK news, sports and rail news.
* RSS Feed Editor to add custom headline feeds.
* Fully-featured browser based configuration screens - choose any station on the UK network / London Tube & DLR network / UK Bus Stops.
* Automatic firmware updates (optional).
* Displays the weather at the selected location (optional).
* Full-screen, Network SouthEast style station clock (optional).

![image](https://github.com/user-attachments/assets/ca2b163d-8218-4d8e-8340-42d449b41138)

You can see the board in action in the demonstration video below:

[![Departures Board Demo Video](https://github.com/user-attachments/assets/ec6a6bbc-593f-402a-b78c-1b2cfda15c2d)](https://youtu.be/gavZ2IEgk8E)

## Quick Start

### What you'll need

1. A Waveshare ESP32-S3 RGB Matrix driver board. As well as the HUB75 interface to drive the LED matrix panels, these boards include onboard audio, SD card reader and generous amounts of PSRAM and flash storage. For example, from [AliExpress](https://www.aliexpress.com/item/1005012187346495.html).
2. Three 64x32 HUB75 LED Matrix panels. These come in various dot pitches (the spacing between each individual LED pixel). With the most compact pitch (P2.5), the finished board will measure 480x80mm. With P5 panels, the board will measure 960x160mm. For example, from [AliExpress](https://www.aliexpress.com/item/1005008394801350.html).
3. Optionally, a TTP223 touch sensor (for easily switching modes / stations). For example, from [AliExpress](https://www.aliexpress.com/item/1005007850732859.html).
4. A Micro SD card for storing the audio speech-packs.
5. A good quality USB-C power supply (5A recommended for running at high brightness levels). For example, from [Amazon](https://www.amazon.co.uk/dp/B0D12H1N4L)
6. For National Rail, the board supports using either the Rail Delivery Group feeds (recommended) or the legacy OpenLDBWS feed. Both feeds are free of charge and provide identical information but the legacy OpenLDBWS feed may be discontinued in the future. To use the Rail Delivery Group feeds, you will need a [Live Departure Board 1.1](https://raildata.org.uk/dataProduct/P-d81d6eaf-8060-4467-a339-1c833e50cbbe/overview) consumer key and (optionally) a [Service Details 1.1](https://raildata.org.uk/dataProduct/P-4dec1247-d040-4290-80a4-639dfac54a92/overview) consumer key.
7. By default, weather data is sourced from Open-Meteo. If you prefer to use OpenWeather (which usually provides slightly more localised weather conditions) you will need an OpenWeather Map API token (these are also free, sign-up for a free developer account [here](https://home.openweathermap.org/users/sign_up)).

A step-by-step guide to obtaining the API keys is available [here](https://matrix-departures-board.github.io/Departures-Board-API-Keys-Guide.pdf).

<img src="https://github.com/user-attachments/assets/121c0755-f965-45cd-9b9a-40cee435e83f" style="display:block; margin:0 auto;">

### Assembly

Format the micro-SD card as FAT32 and copy all of the [speech-pack](https://github.com/gadec-uk/matrix-departures-board/tree/main/speech-packs) files to the root of the card, retaining the folder structure (the root folder should contain only the V1 and V2 folders). Insert the card into the slot on the processor board and attach the supplied speaker.

The processor board is plugged into the first panel's HUB75 "in" socket, the "out" socket is connected to the next panel's "in" socket and the "out" socket of that panel is connected to the "in" socket of the third panel (using the ribbon cables provided with the panels). Separate power cables must be connected for each panel, fed from the power output (screw terminals) on the processor board.

If using the optional touch-sensor, use the cable provided with the processor board to connect as follows:

| TTP223 Pin | Wire | ESP32-S3 Pin |
|:---------|:----------|:------------:|
| 1 GND | Black | GND |
| 2 I/O | Yellow | IO45 |
| 3 VCC | Red | 3.3v |

Plug the USB-C power supply into the USB-C socket labelled "POWER" and connect the second USB-C socket (labelled "USB") to your computer.

### Installing the firmware

The project uses the Arduino framework and the ESP32 v3.3.9 core. If you want to build from source, you'll need [PlatformIO](https://platformio.org).

The easiest way to install the firmware for the first time is to use the online web based installer [here](https://matrix-departures-board.github.io). You will need to use Chrome, Edge or Firefox as your browser as Safari does not support Web Serial.

Alternatively, you can download pre-compiled firmware images from the [releases](https://github.com/gadec-uk/matrix-departures-board/releases). These can be installed over the USB serial connection using [esptool](https://github.com/espressif/esptool). If you have python installed, install with *pip install esptool*. For convenience, a pre-compiled executable version for Windows is included [here](https://github.com/gadec-uk/matrix-departures-board/tree/main/esptool).

Attach the processor board via its "USB" USB-C port and use the following command to flash the firmware:

```
esptool.py --chip esp32s3 --baud 921600 write_flash \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size detect \
  0x0000 bootloader.bin \
  0xe000 boot_app0.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

The tool should automatically find the correct serial port. If it fails to, you can manually specify the correct port by adding *--port COMx* (replace *COMx* with your actual port, e.g. COM3, /dev/ttyUSB0, etc.).

If using the pre-compiled esptool.exe version on Windows, save the esptool.exe and the four firmware (.bin) files to the same directory. Open a command prompt (Windows Key + R, type cmd and press enter) and change to the directory you saved the files into. Now type the following command on one line and press enter:
```
.\esptool --chip esp32s3 --baud 921600 write_flash --flash-mode dio --flash-freq 80m --flash-size detect 0x0000 bootloader.bin 0xe000 boot_app0.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

Subsequent updates can be carried out automatically over-the-air or you can manually update from the Web GUI.

### First time configuration

WiFiManager is used to setup the initial WiFi connection on first boot. The ESP32 will broadcast a temporary WiFi network named "Departures Board", connect to the network and follow the on-screen instuctions.

Once the ESP32 has established an Internet connection, the next step is to enter your API keys (if you do not enter a Rail Delivery Group API key or National Rail token, the board will only operate in Tube and Bus modes). Finally, select a station location. Start typing the location name and valid choices will be displayed as you type.

### Web GUI

At start-up, the ESP32's IP address is displayed. To change the station or to configure other miscellaneous settings, open the web page at that address. The settings available are:
- **Board Mode** - switch between National Rail Departures, London Underground Arrivals or UK Bus Stops modes.
- **Station** - start typing a few characters of a station name and select from the drop-down station picker displayed (National Rail mode).
- **Only show services calling at** - filter services based on *calling at* location (National Rail mode - if you want to see the next trains *to* a particular station).
- **Only show these platforms** - filter services based on the platform they depart from. Note: there are many services for which platform number is not supplied, these would also be filtered out.
- **Add to Scheduler** - adds the current configured station/tube/bus stop to the scheduler (see schedule tab) to switch based on time of day.
- **Add to Carousel** - adds the current configured station/tube/bus stop to the carousel (see schedule tab) to switch views after a period of time.
- **Underground Station** - start typing a few characters of an Underground or DLR station name and select from the drop-down station picker displayed (London Underground mode).
- **Filter by Line** - select the desired underground line or all lines for all arrivals.
- **Filter by Direction** - select the desired direction or any direction for all arrivals.
- **Bus Stop ATCO code** - type the ATCO number of the bus stop you want to monitor (see [below](#bus-stop-atco-codes) for details).
- **Only show these Bus services** - filter buses by service numbers (enter a list of the service numbers, comma separated).
- **Recently verfied ATCO codes** - quickly select from recently used bus stop ATCO codes.
#### Options tab ####
- **Brightness** - adjusts the brightness of the LED matrix panels.
- **Show current location name** - displays the currently selected station or bus stop name in the cycle of services, following the clock.
- **Show current weather at station/bus stop** - optionally display weather conditions at the selected station or bus stop.
- **Include bus replacement services** - optionally include bus replacement services (National Rail mode).
- **Show station messages** - displays station and service messages (Rail and Tube modes).
- **Show service location** - displays the current location of the next tube train that is due to arrive (Tube mode).
- **Show platform numbers if available** - deselecting this option will hide platform numbers (National Rail).
- **Show service ordinal numbers** - displays "2nd","3rd","4th" etc. next to the service times (National Rail).
- **Show service last seen location** - adds the last reported location and time of a service to the Calling at list (National Rail).
- **Wait for Calling at list to complete** - waits for the Calling at list to finish scrolling before changing the primary service.
- **Wait for Messages or RSS to complete** - waits for the current service message or RSS headline feed to finish scrolling before changing the primary service.
- **Full screen clock if no train services** - displays the full screen, Network SouthEast style station clock if there are no scheduled services at the selected railway station.
- **Enable audio announcements** - Enables station audio announcements in Rail / Tube mode. A micro SD card with speech-pack files must be present.
- **Announcer voice** - Select which station announcer you want to hear. Two speech-packs are provided as standard: "Andy" and "Ruth".
- **Audio volume** - Set the volume level for audio announcements.
- **Enable station reverb effect** - Enables a reverb (echo) effect for more atmospheric sounding announcements.
- **Enable "calling at" announcements** - The station announcements will read out the full listing of calling stations for a service.
- **Enable automatic firmware updates at startup** - automatically checks for AND installs the latest firmware from this repository when the system starts up.
- **Enable daily check for firmware updates** - when enabled, the system will check for and install any updates just after midnight if the board is powered on.
- **Enable overnight sleep mode (screensaver)** - if you're running the board 24/7, you select a time period for the screen to be off (or a full screen clock to be shown).
- **Full screen clock during sleep mode** - displays the full screen station clock during sleep mode.
#### Schedule tab ####
- **Enable scheduler** - automatically switches between views based on the configured time of each entry in the scheduler list below.
- **Enable carousel** - automatically switches between views based on the configured view time of each entry in the carousel list below.
#### Theme Tab ####
- **Current theme** - select the colour theme for your board. There are three system themes provided as standard. To create your own, customise the colours of the various items and use the "Save As..." button to create a new theme.
#### Advanced Tab ####
- **Enable touch sensor** - a tap switches between configured modes (rail/tube/bus) or wakes from sleep. If the Scheduler or Carousel mode is active, a tap switch to the next location in the list. Obviously, do not enable this option if you have not installed a TTP223 touch sensor.
- **Wake from sleep by touch for** - if the board is in screensaver mode and the touch sensor is enabled, a tap will wake the board and it will remain awake for the selected number of minutes (the countdown timer resets on each tap).
- **Long press displays full screen clock** - a long tap switches to the full screen station clock. A short tap will revert to normal operation.
- **Flip the display 180°** - Rotates the display.
- **Swap blue/green pixel order** - different panel manufacturers use different pixel colour orders. If you board is displaying colours incorrectly, select this option to reverse the blue/green order.
- **Enable support for FM6126A driver** - if your matrix panels use FM6126A chips they will require a special initialisation sequence.
- **Enable internal FTP server** - allows you to manage files on the SD card using an FTP client. Connect with username: <i>admin</i> and password: <i>depboard</i>
- **Set custom hostname for this board** - change the hostname from the default "DeparturesBoard", useful if you are running multiple boards.
- **Custom (non-UK) time zone (only for clock)** - if you're not based in the UK you can set the clock to display in your local time zone (see [below](#custom-time-zones) for details).
- **Suppress calling at / information messages** - removes all horizontally scrolling text (much lower functionality but less distracting).
- **Increase API refresh rate** - Reduces the interval between data refreshes (National Rail mode).
- **Display RSS news headlines feed** - Displays the top headlines from the selected feed (Rail/Tube mode).
- **Prioritise RSS headlines feed** - Displays headlines before other network service messages.
- **Display departures offset by** - Displays future (or past) services offset by the selected time. This does not affect the clock display (Rail mode).
- **Rail data source** - Select which api feed should be used for National Rail mode (only feeds with api keys configured are shown).

A drop-down menu (top-right) adds the following options:
- **Check for Updates** - manually checks for and optionally installs any updates to the firmware. Also displays the release notes of the latest firmware.
- **Edit API Keys** - view/edit your National Rail, OpenWeather Map and Transport for London API keys.
- **Edit RSS Feeds** - loads the RSS Feeds Editor where you can add/edit/delete custom headline feeds.
- **Clear WiFi Settings** - deletes the stored WiFi credentials and restarts in WiFiManager mode (useful to change WiFi network).
- **Restart System** - restarts the ESP32.

#### Other Web GUI Endpoints

A few other urls have been implemented, primarily for debugging/developer use:
- **/factoryreset** - deletes all configuration information, api keys and WiFi credentials. The entire setup process will need to be repeated.
- **/update** - for manual firmware updates. Download the latest binary from the [releases](https://github.com/gadec-uk/matrix-departures-board/releases). Only the **firmware.bin** file should be uploaded via */update*. The other .bin files are not used for upgrades. This method is *not* recommended for normal use.
- **/info** - displays some basic information about the current running state.
- **/formatffs** - formats the filing system, erasing the configuration files (but not the WiFi credentials).
- **/dir** - displays a (basic) directory listing of the file system with the ability to view/delete files.
- **/upload** - upload a file to the file system.
- **/control** - an endpoint for automation of sleep mode. Takes optional parameters *sleep* and *clock* - e.g. /control?sleep=1&clock=0 will force sleep mode and turn off the display completely. /control?sleep=0 will revert to normal operation. Always returns current state as json.

### Bus Stop ATCO codes
Every UK bus stop has a unique ATCO code number. To find the ATCO code of the stop you want to monitor, go to [bustimes.org/search](https://bustimes.org/search) and type a location in the search box. Select the location from the list of places shown and then select the particular stop you want from the list. The ATCO code is shown on the stop information page. After entering the code in the Departures Board setup screen, tap the **Verify** button and the location will be shown confirming your selection. You must use the **Verify** button *before* you can save changes. Up to ten of the most recently verified ATCO codes are saved and can be selected from a dropdown list for quick access. The bustimes map and search facility are also embedded in the Bus mode configuration screen from firmware B2.3 onwards.

<img src="https://github.com/user-attachments/assets/8a41ec6d-5f15-4102-b3d5-c09260986319" style="display:block; margin:0 auto;">

### Custom Time Zones
To set a custom time zone for the departure board clock, you will need to enter the POSIX time zone string for your location. Some examples are `CST6CDT,M3.2.0/2,M11.1.0/2` for Canada (Central Time) and `AEST-10AEDT,M10.1.0,M4.1.0/3` for Australia (Eastern Time). The easiest way to find the correct syntax is to ask your favourite AI chat engine *"What is the POSIX time zone string for ..."*. Note that changing the time zone only affects the clock (and date) display. Service times are *always* shown in UK time.

### Speech-Packs ###
Two [speech-packs](https://github.com/gadec-uk/matrix-departures-board/tree/main/speech-packs) are included. Both were generated using [Chatterbox AI](https://chatterboxai.net/) with public domain voice samples from [LibriVox](https://librivox.org/). "Andy" was created from a voice sample of [Andy Minter](https://librivox.org/reader/152) and "Ruth" was created from a voice sample of [Ruth Golding](https://librivox.org/reader/2607). All of the generated audio files were level matched to ITU-R BS.1770-3 Loudness at -17 LUFS and then MP3 mono encoded at 48kbps with a sample rate of 22050Hz.

Additional speech-packs can be added to the card. You must follow the same file and folder naming conventions and all files must be encoded as MP3 mono 22050Hz at 48kbps.

### Donating

<a href="https://buymeacoffee.com/gadec.uk"><img src="https://github.com/user-attachments/assets/e5960046-051a-45af-8730-e23d4725ab53" width="160" style="display:inline-block; vertical-align:top; margin-right: 15px;" /></a>
This software is completely free for non-commercial use without obligation. If you would like to support me and encourage ongoing updates, you can [buy me a coffee!](https://buymeacoffee.com/gadec.uk)

### Licence
This work is licensed under **Creative Commons Attribution-NonCommercial-ShareAlike 4.0**. To view a copy of this licence, visit [https://creativecommons.org/licenses/by-nc-sa/4.0/](https://creativecommons.org/licenses/by-nc-sa/4.0/). Note: the terms of the licence prohibit commericial use of this work, this includes *any* reselling of the work in kit or assembled form for commercial gain.
