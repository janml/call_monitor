#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <tr064.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include "secrets.h"


const uint MAX_CALLS_TO_DISPLAY = 8;  // IMPROVEMENT: Move to settings?

const uint PAST_DAYS_TO_REQUEST_CALLS_FOR = 2;


const int CALL_MISSED = 1;
const int CALL_INCOMMING = 2;
const int CALL_OUTGOING = 2;

struct Call {
  int type;
  String call_datetime;
  String caller_phone_number;
};


Call calls[MAX_CALLS_TO_DISPLAY];


void establish_and_wait_for_wifi_connection() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
}


String request_call_list_xml() {
  Serial.println("Requesting call list xml...");

  const String service = "urn:dslforum-org:service:X_AVM-DE_OnTel:1";
  const String method = "GetCallList";

  String params[][2] = {{}};
  String response[][2] = {{"NewCallListURL", ""}};
  TR064 tr064(TR064_PORT, TR064_HOST, TR064_USER, TR064_PASSWORD);
  tr064.init();

  tr064.action(service, method, params, 0, response, 1);
  String callListUrl = response[0][1];
  if (!callListUrl.length()) {
    return "";
  }
  
  HTTPClient httpClient;
  httpClient.begin(callListUrl + "&days=" + PAST_DAYS_TO_REQUEST_CALLS_FOR + "&type=xml");
  
  int httpStatus = httpClient.GET();
  if (httpStatus != HTTP_CODE_OK) {
    Serial.printf("ERROR: GET request to call list url failed (http-status %d)!\n", httpStatus);
    return "";
  }

  String callList = httpClient.getString();
  httpClient.end();
  Serial.println("Done requesting call list xml...");
  return callList;
}


String get_xml_tag(String s, String tag) {
  String opening_tag = "<" + tag + ">";
  String closing_tag = "</" + tag + ">";
  int opening_tag_index = s.indexOf(opening_tag);
  int closing_tag_index = s.indexOf(closing_tag);
  if (opening_tag_index < 0 || closing_tag_index < 0) {
    return "";
  }

  return s.substring(
    opening_tag_index,
    closing_tag_index + closing_tag.length()
  );
}


String remove_string(String s, String to_remove) {
  s.remove(
    s.indexOf(to_remove), 
    to_remove.length()
  );
  return s;
}


String get_xml_tag_value(String s, String tag) {
  String opening_tag = "<" + tag + ">";
  String closing_tag = "</" + tag + ">";
  int tag_value_start = s.indexOf(opening_tag) + opening_tag.length();
  int tag_value_end = s.indexOf(closing_tag);
  return s.substring(
    tag_value_start,
    tag_value_end
  );
}

void parse_call_list(String xml) {
  Serial.println("Parsing call list xml...");

  for (uint i = 0; i < MAX_CALLS_TO_DISPLAY; i++) {
    String call_data = get_xml_tag(xml, "Call");

    bool all_calls_are_parsed = !call_data.length();
    if (all_calls_are_parsed) {
      Serial.println("All calls are parsed");
      break;
    }

    Call call = {};
    call.type = get_xml_tag_value(call_data, "Type").toInt();
    call.call_datetime = get_xml_tag_value(call_data, "Date");
    call.caller_phone_number = get_xml_tag_value(call_data, "Caller");
    
    calls[i] = call;

    // Remove processed xml line.
    xml = remove_string(xml, call_data);
  }
}


// REFACTOR: Move to toplevel declaration!
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 4
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 14);
  display.setTextColor(WHITE);
  display.println("0/4 04.04 17:04");
  display.setCursor(0, 35);
  //display.setTextSize(2);
  display.println("0800123456789");
  display.display();

  establish_and_wait_for_wifi_connection();
  
  String call_list_xml = request_call_list_xml();
  if (!call_list_xml.length()) {
    Serial.println("Failed to get calls xml!");
    return;
  }
  parse_call_list(call_list_xml);
  Serial.println("Calls retrieved.");
}

void loop() {
}
