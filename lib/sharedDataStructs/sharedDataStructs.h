/*
 * Matrix Departures Board (c) 2026 Gadec Software
 *
 * Common station data structures shared by data clients
 *
 * https://github.com/gadec-uk/matrix-departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#pragma once
#include <Arduino.h>

#define MAXBOARDMESSAGES 4
#define MAXMESSAGESIZE 600
#define MAXCALLINGSIZE 950
#define MAXOPCOSIZE 50
#define MAXBOARDSERVICES 9
#define MAXLOCATIONSIZE 85
#define MAXBUSTUBELOCATIONSIZE 50
#define MAXFILTERSIZE 25
#define MAXLINESIZE 20
#define MAXTUBEBUSREADSERVICES 40
#define MAXNAPTANSIZE 13
#define MAXSERVICEIDSIZE 18

#define MAXKEYNAMESIZE 50
#define MAXRESULTMESSAGESIZE 80

#define MAXWEATHERSIZE 50

#define OWMKEYSIZE 33
#define MAXLOCATIONCODESIZE 13

#define OTHER 0
#define TRAIN 1
#define BUS 2

struct stnMessages {
  int numMessages;
  char messages[MAXBOARDMESSAGES][MAXMESSAGESIZE];
};

struct rdService {
  char sTime[6];
  char destination[MAXLOCATIONSIZE];
  char destinationCrs[4];
  char via[MAXLOCATIONSIZE];  // also used for line name for TfL
  char viaCrs[4];
  char viaCrs2[4];
  char etd[11];
  char platform[4];
  bool isCancelled;
  bool isDelayed;
  int trainLength;
  byte classesAvailable;
  char opco[MAXOPCOSIZE];
  char opcoId[4];
  char serviceId[MAXSERVICEIDSIZE];
  int serviceType;
  int timeToStation;  // Only for TfL
  bool atPlatform;  // Only for TfL
  char destinationNaptan[MAXNAPTANSIZE]; // Only for TfL
  char lineId[MAXLINESIZE]; // Only for TfL
  char currentLocation[MAXBUSTUBELOCATIONSIZE]; // Only for TfL
};

struct rdStation {
  char location[MAXLOCATIONSIZE];
  bool platformAvailable;
  int numServices;
  bool boardChanged;  // Only for TfL
  char calling[MAXCALLINGSIZE];   // Only store the calling stops for the first service returned
  char callingCrs[MAXCALLINGSIZE];   // Only store the calling stops for the first service returned
  char origin[MAXLOCATIONSIZE]; // Only store the origin for the first service returned
  char originCrs[4];
  char serviceMessage[MAXMESSAGESIZE];  // Only store the service message for the first service returned
  rdService service[MAXBOARDSERVICES];
};

// Rail structure for data downloads
struct rdiService {
  char sTime[6];
  char destination[MAXLOCATIONSIZE];
  char destinationCrs[4];
  char via[MAXLOCATIONSIZE];
  char viaCrs[4];
  char viaCrs2[4];
  char origin[MAXLOCATIONSIZE];
  char originCrs[4];
  char etd[11];
  char platform[4];
  bool isCancelled;
  bool isDelayed;
  int trainLength;
  byte classesAvailable;
  char opco[MAXOPCOSIZE];
  char opcoId[4];
  char calling[MAXCALLINGSIZE];
  char callingCrs[MAXCALLINGSIZE];
  char serviceMessage[MAXMESSAGESIZE];
  int serviceType;
  char serviceID[MAXSERVICEIDSIZE];
  char sortTime[6];
};

struct rdiStation {
  char location[MAXLOCATIONSIZE];
  bool platformAvailable;
  int numServices;
  rdiService service[MAXBOARDSERVICES];
};

// Common structure for tube/bus data downloads
struct busTubeService {
  char destinationName[MAXBUSTUBELOCATIONSIZE];
  char destinationNaptan[MAXNAPTANSIZE];
  char currentLocation[MAXBUSTUBELOCATIONSIZE];
  char lineName[MAXLINESIZE];
  char lineId[MAXLINESIZE];
  char platformDescription[MAXBUSTUBELOCATIONSIZE];
  char platformNumber[3];
  char serviceID[MAXSERVICEIDSIZE];
  int timeToStation;
  char scheduled[6];
  char expected[6];
  char sortTime[6];
};

struct busTubeStation {
  int numServices;
  busTubeService service[MAXTUBEBUSREADSERVICES];
};

// Common data buffers for parsing JSON
struct sharedBufferSpace {
  char currentKey[MAXKEYNAMESIZE];
  char objectCurrentKey[MAXKEYNAMESIZE];
  char currentPath[(MAXKEYNAMESIZE*2+1)];
  char arrayName[(MAXKEYNAMESIZE*2)+1];
  char lastResultMessage[MAXRESULTMESSAGESIZE];
};

