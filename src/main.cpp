#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <tr064.h>
#include "secrets.h"


const uint HOW_MANY_DAYS_TO_REQUEST_CALLS_FOR = 2;
const uint MAX_CALLS_TO_DISPLAY = 8;  // After this ammount it gets annoying to switch to the next call ;) 

const String CALL_DATA_OPENING_XML_TAG = "<Call>";
const String CALL_DATA_CLOSING_XML_TAG = "</Call>";
const String CALL_DATETIME_OPENING_XML_TAG = "<Date>";
const String CALL_DATETIME_CLOSING_XML_TAG = "</Date>";
const String CALLER_PHONE_NUMBER_OPENING_XML_TAG = "<Caller>";
const String CALLER_PHONE_NUMBER_CLOSING_XML_TAG = "</Caller>";
const String CALL_TYPE_OPENING_XML_TAG = "<Type>";
const String CALL_TYPE_CLOSING_XML_TAG = "</Type>";

enum CallType {
  INCOMMING = 1,
  OUTGOING = 2,
  MISSED = 3,
};

struct Call {
  CallType type;
  String datetime;
  String callerPhoneNumber;
};


void connectWifi() {
  // TODO: Add a timeout and return int based on success or error conditon!
  Serial.println("Connecting wifi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println("Wifi connected.");
}

String getCallList() {
  TR064 tr064(TR064_PORT, TR064_HOST, TR064_USER, TR064_PASSWORD);
  tr064.init();

  String params[][2] = {{}};
  String response[][2] = {{"NewCallListURL", ""}};
  tr064.action("urn:dslforum-org:service:X_AVM-DE_OnTel:1", "GetCallList", params, 0, response, 1);
  String callListUrl = response[0][1];
  if (!callListUrl.length()) {
    return "";
  }
  
  HTTPClient httpClient;
  httpClient.begin(callListUrl + "&days=" + HOW_MANY_DAYS_TO_REQUEST_CALLS_FOR + "&type=xml");
  
  int httpStatusCode = httpClient.GET();
  if (httpStatusCode != HTTP_CODE_OK) {
    Serial.printf("ERROR: GET request to call list url failed (http-status %d)!\n", httpStatusCode);
  }

  String callListXml = httpClient.getString();
  httpClient.end();
  return callListXml;
}

String getXmlTagValue(String s, String openingTag, String closingTag) {
  int valueStart = s.indexOf(openingTag) + openingTag.length();
  int valueEnd = s.indexOf(closingTag);
  return s.substring(valueStart, valueEnd);
}


void parseCalls(String callListXml, int max) {
  for (uint i = 0; i < max; i++) {
    int callDataOpeningXmlTagIndex = callListXml.indexOf(CALL_DATA_OPENING_XML_TAG);
    int callDataClosingXmlTagIndex = callListXml.indexOf(CALL_DATA_CLOSING_XML_TAG);

    bool noCallsLeftToParse = callDataOpeningXmlTagIndex < 0 || callDataClosingXmlTagIndex < 0; 
    if (noCallsLeftToParse) {
      break;
    }

    String callDataClosingXmlTag = "</Call>";

    String callData = callListXml.substring(callDataOpeningXmlTagIndex, callDataClosingXmlTagIndex + callDataClosingXmlTag.length());
    callListXml.remove(callDataOpeningXmlTagIndex, (callDataClosingXmlTagIndex + callDataClosingXmlTag.length()) - callDataOpeningXmlTagIndex);

    String callDatetime = getXmlTagValue(callData, CALL_DATETIME_OPENING_XML_TAG, CALL_DATETIME_CLOSING_XML_TAG);
    String caller = getXmlTagValue(callData, CALLER_PHONE_NUMBER_OPENING_XML_TAG, CALLER_PHONE_NUMBER_CLOSING_XML_TAG);
    int callTypeId = getXmlTagValue(callData, CALL_TYPE_OPENING_XML_TAG, CALL_TYPE_CLOSING_XML_TAG).toInt();

    Serial.printf("Call %i: Type: %s\n", i, callTypeId);
    Serial.println(callDatetime);
    Serial.printf("Caller: %s\n", caller);
    Serial.println(""); // Newline for better readability.
  }
}

void setup() {
  Serial.begin(9600);
  delay(2000);  // Need some time between flashing and reading the output.
  connectWifi();

  Serial.println("Loading call list...");
  String callListXml = getCallList();
  if (!callListXml.length()) {
    Serial.println("Failed to get call list xml!");
    return;
  }
  Serial.println("Parsing calls...");
  parseCalls(callListXml, MAX_CALLS_TO_DISPLAY);
}

void loop() {

}
