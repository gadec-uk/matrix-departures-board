/*
 * Matrix Departures Board (c) 2026 Gadec Software
 *
 * Panel Colour Themes
 *
 * https://github.com/gadec-uk/matrix-departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 */
#include <Arduino.h>

#define MAXTHEMEELEMENTS 35

enum themeElements {
    RAIL_ORDINAL = 0,
    RAIL_DEPARTURETIME = 1,
    RAIL_PLATFORM = 2,
    RAIL_DESTINATION = 3,
    RAIL_VIA = 4,
    RAIL_ONTIME = 5,
    RAIL_EXPECTED = 6,
    RAIL_CANCELLED = 7,
    RAIL_CALLING = 8,
    RAIL_SERVICEITEMS = 9,
    RAIL_MESSAGES = 10,
    RAIL_WEATHER = 11,
    RAIL_STATION = 12,
    RAIL_CLOCK = 13,
    RAIL_NOSERVICES = 14,
    TUBE_ORDINAL = 15,
    TUBE_DESTINATION = 16,
    TUBE_CURRENTLOCATION = 17,
    TUBE_TTS = 18,
    TUBE_DUE = 19,
    TUBE_MESSAGES = 20,
    TUBE_WEATHER = 21,
    TUBE_STATION = 22,
    TUBE_CLOCK = 23,
    TUBE_NOSERVICES = 24,
    BUS_SERVICE = 25,
    BUS_DESTINATION = 26,
    BUS_DEPARTURETIME = 27,
    BUS_EXPECTED = 28,
    BUS_MESSAGES = 29,
    BUS_WEATHER = 30,
    BUS_STATION = 31,
    BUS_CLOCK = 32,
    BUS_NOSERVICES = 33,
    NSE_CLOCK = 34
};

static const char* themeTags[] = {
  "Service ordinal (Rail)",
  "Departure time",
  "Platform number",
  "Service destination (Rail)",
  "Service via destination",
  "On time text",
  "Expected time (Rail)",
  "Cancelled text",
  "Calling at",
  "Service descriptions (Rail)",
  "Messages/RSS (Rail)",
  "Weather (Rail)",
  "Station name (Rail)",
  "Clock (Rail)",
  "No scheduled services (Rail)",
  "Service ordinal (Tube)",
  "Service destination (Tube)",
  "Service current location (Tube)",
  "Time to station (Tube)",
  "Service due (Tube)",
  "Messages/RSS (Tube)",
  "Weather (Tube)",
  "Station name (Tube)",
  "Clock (Tube)",
  "No scheduled services (Tube)",
  "Service number (Bus)",
  "Service destination (Bus)",
  "Scheduled time (Bus)",
  "Expected time (Bus)",
  "Messages (Bus)",
  "Weather (Bus)",
  "Bus stop location (Bus)",
  "Clock (Bus)",
  "No scheduled services (Bus)",
  "Full screen clock (all modes)"
};

uint16_t colours[MAXTHEMEELEMENTS];

const uint16_t defaultTheme[MAXTHEMEELEMENTS] = {
    0xFFFF, // Service ordinal (Rail)
    0xFA00, // Departure time
    0x07FF, // Platform number
    0xFA00, // Service destination (Rail)
    0xFA00, // Service via destination
    0x07E0, // On time text
    0xF800, // Expected time (Rail)
    0xF800, // Cancelled text
    0xEF60, // Calling at
    0xBDF7, // Service descriptions (Rail)
    0xBDF7, // Messages/RSS (Rail)
    0x07E0, // Weather (Rail)
    0xFFFF, // Station name (Rail)
    0xFFFF, // Clock (Rail)
    0xFFFF, // No scheduled services (Rail)
    0xFA00, // Service ordinal (Tube)
    0xFA00, // Service destination (Tube)
    0xFA00, // Service current location (Tube)
    0xFA00, // Time to station (Tube)
    0x07E0, // Service due (Tube)
    0xBDF7, // Messages/RSS (Tube)
    0x07E0, // Weather (Tube)
    0xFFFF, // Station name (Tube)
    0xFFFF, // Clock (Tube)
    0xFFFF, // No scheduled services (Tube)
    0xBDF7, // Service number (Bus)
    0xBDF7, // Service destination (Bus)
    0xBDF7, // Scheduled time (Bus)
    0xBDF7, // Expected time (Bus)
    0xBDF7, // Messages (Bus)
    0x07E0, // Weather (Bus)
    0xBDF7, // Bus stop location (Bus)
    0xBDF7, // Clock (Bus)
    0xBDF7, // No scheduled services (Bus)
    0xBDF7  // Full screen clock (all modes)
};

const uint16_t coolIceTheme[MAXTHEMEELEMENTS] = {
    0xFFFF, // Service ordinal (Rail)
    0x2BFF, // Departure time
    0x07FF, // Platform number
    0xE77E, // Service destination (Rail)
    0x211F, // Service via destination
    0x9FBF, // On time text
    0xF800, // Expected time (Rail)
    0xF800, // Cancelled text
    0x6FFF, // Calling at
    0xBDF7, // Service descriptions (Rail)
    0xBF7F, // Messages/RSS (Rail)
    0x8990, // Weather (Rail)
    0xFFFF, // Station name (Rail)
    0x87E6, // Clock (Rail)
    0xFFFF, // No scheduled services (Rail)
    0xFA00, // Service ordinal (Tube)
    0xFA00, // Service destination (Tube)
    0xFA00, // Service current location (Tube)
    0xFA00, // Time to station (Tube)
    0xC7E0, // Service due (Tube)
    0xBDF7, // Messages/RSS (Tube)
    0xC7E0, // Weather (Tube)
    0xFFFF, // Station name (Tube)
    0x2BFF, // Clock (Tube)
    0xFFFF, // No scheduled services (Tube)
    0xBDF7, // Service number (Bus)
    0xBDF7, // Service destination (Bus)
    0xBDF7, // Scheduled time (Bus)
    0xBDF7, // Expected time (Bus)
    0xBDF7, // Messages (Bus)
    0x07E0, // Weather (Bus)
    0xBDF7, // Bus stop location (Bus)
    0xBDF7, // Clock (Bus)
    0xBDF7, // No scheduled services (Bus)
    0xBDF7  // Full screen clock (all modes)
};

const uint16_t justOrangeTheme[MAXTHEMEELEMENTS] = {
    0xFC00, // Service ordinal (Rail)
    0xFC00, // Departure time
    0xFC00, // Platform number
    0xFC00, // Service destination (Rail)
    0xFC00, // Service via destination
    0xFC00, // On time text
    0xFC00, // Expected time (Rail)
    0xFA00, // Cancelled text
    0xFC00, // Calling at
    0xFC00, // Service descriptions (Rail)
    0xFC00, // Messages/RSS (Rail)
    0xFC00, // Weather (Rail)
    0xFC00, // Station name (Rail)
    0xFD00, // Clock (Rail)
    0xFC00, // No scheduled services (Rail)
    0xFA00, // Service ordinal (Tube)
    0xFA00, // Service destination (Tube)
    0xFA00, // Service current location (Tube)
    0xFA00, // Time to station (Tube)
    0xFA00, // Service due (Tube)
    0xFA00, // Messages/RSS (Tube)
    0xFA00, // Weather (Tube)
    0xDA00, // Station name (Tube)
    0xFA00, // Clock (Tube)
    0xFA00, // No scheduled services (Tube)
    0xFC00, // Service number (Bus)
    0xFC00, // Service destination (Bus)
    0xFC00, // Scheduled time (Bus)
    0xFC00, // Expected time (Bus)
    0xFC00, // Messages (Bus)
    0xFC00, // Weather (Bus)
    0xFC00, // Bus stop location (Bus)
    0xFC00, // Clock (Bus)
    0xFC00, // No scheduled services (Bus)
    0x8410  // Full screen clock (all modes)
};