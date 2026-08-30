/*
 * Matrix Departures Board (c) 2026 Gadec Software
 *
 * rssClient Library
 *
 * https://github.com/gadec-uk/matrix-departures-board
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#include <rssClient.h>
#include <xmlListener.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>

rssClient::rssClient(sharedBufferSpace *sharedBuffer) : js(sharedBuffer) {}

// Trim leading and trailing spaces in-place
void rssClient::trim(char* str) {
    char* start = str;
    while (*start && isspace(static_cast<unsigned char>(*start)))
        ++start;

    char* end = start + strlen(start);
    while (end > start && isspace(static_cast<unsigned char>(*(end - 1))))
        --end;

    *end = '\0';

    if (start != str)
        memmove(str, start, end - start + 1);
}

void rssClient::replaceMultiByte(char* str) {
if (!str) return;

    size_t read = 0;
    size_t write = 0;

    while (str[read] != '\0') {
        unsigned char c = static_cast<unsigned char>(str[read]);

        if ((c & 0x80) == 0) {
            // Standard 1-byte ASCII (0x00 - 0x7F)
            str[write++] = str[read++];
        } else {
            unsigned char nextByte = static_cast<unsigned char>(str[read + 1]);

            // Check specifically for UTF-8 '£' (U+00A3 -> 0xC2 0xA3)
            if (c == 0xC2 && nextByte == 0xA3) {
                str[write++] = static_cast<char>(0xA3); // Store single-byte 0xA3
                read += 2;                              // Advance past 2-byte sequence
            } else {
                // Generic multi-byte character fallback
                size_t sequenceLength = 1;

                if ((c & 0xE0) == 0xC0) {
                    sequenceLength = 2; // 2-byte sequence
                } else if ((c & 0xF0) == 0xE0) {
                    sequenceLength = 3; // 3-byte sequence
                } else if ((c & 0xF8) == 0xF0) {
                    sequenceLength = 4; // 4-byte sequence
                }

                // Replace non-matching multi-byte character with '?'
                // str[write++] = '?';

                // Skip sequence length in read buffer
                for (size_t i = 0; i < sequenceLength && str[read] != '\0'; ++i) {
                    read++;
                }
            }
        }
    }

    str[write] = '\0'; // Properly terminate shortened string
}

// Load the RSS feed item titles
int rssClient::loadFeed(String url) {
    HTTPClient http;
    WiFiClient client;
    WiFiClientSecure clientSecure;

    unsigned long perfTimer = millis();
    int redirectCount = 0;
    const int maxRedirects = 5;

    clientSecure.setInsecure();
    http.setReuse(false);
    numRssTitles = 0;

    while (redirectCount < maxRedirects) {
        if (url.startsWith("https")) http.begin(clientSecure,url);
        else http.begin(client, url);
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            WiFiClient *stream = http.getStreamPtr();
            xmlStreamingParser parser;
            parser.setListener(this);
            parser.reset();
            js->currentKey[0] = '\0';
            js->objectCurrentKey[0] = '\0';
            js->currentPath[0] = '\0';
            tagLevel = 0;
            long dataReceived = 0;
            char c;
            unsigned long dataSendTimeout = millis() + 3000UL;

            while((stream->available() || http.connected()) && millis() < dataSendTimeout && numRssTitles < MAX_RSS_TITLES) {
                while (stream->available() && numRssTitles < MAX_RSS_TITLES) {
                    c = stream->read();
                    parser.parse(c);
                    dataReceived++;
                }
                delay(1);
            }

            http.end();
            if (millis() >= dataSendTimeout) {
                return UPD_TIMEOUT;
            }
            return UPD_SUCCESS;
            break;
        } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY ||
                   httpCode == HTTP_CODE_FOUND ||
                   httpCode == HTTP_CODE_TEMPORARY_REDIRECT ||
                   httpCode == HTTP_CODE_PERMANENT_REDIRECT) {
            // Handle redirect
            String newUrl = http.getLocation();
            http.end();  // End current request before retrying
            if (newUrl.length() == 0) {
                return UPD_HTTP_ERROR;
                break;
            }
            url = newUrl;
            redirectCount++;
        } else {
            http.end();
            return UPD_HTTP_ERROR;
            break;
        }
    }
    // never get here
    return UPD_SUCCESS;
}

void rssClient::startTag(const char *tag)
{
    tagLevel++;
    strcpy(js->arrayName,js->objectCurrentKey);
    strcpy(js->objectCurrentKey,js->currentKey);
    strlcpy(js->currentKey, tag, MAXKEYNAMESIZE);
    sprintf(js->currentPath,"%s/%s",js->objectCurrentKey,js->currentKey);
}

void rssClient::endTag(const char *tag)
{
    tagLevel--;
    strcpy(js->currentKey,js->objectCurrentKey);
    strcpy(js->objectCurrentKey,js->arrayName);
    js->arrayName[0] = '\0';
}

void rssClient::parameter(const char *param)
{
}

void rssClient::value(const char *value)
{
    if (strcmp(js->currentPath, "item/title") == 0) {
        strlcpy(rssTitle[numRssTitles],value,MAX_RSS_TITLE_SIZE);
        replaceMultiByte(rssTitle[numRssTitles]);
        trim(rssTitle[numRssTitles]);
        numRssTitles++;
    }
}

void rssClient::attribute(const char *attr)
{
}
