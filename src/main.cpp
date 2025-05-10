#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <tr064.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>
#include "secrets.h"


const uint MAX_CALLS_TO_DISPLAY = 10;  // IMPROVEMENT: Move to settings?

const uint PAST_DAYS_TO_REQUEST_CALLS_FOR = 2;


const int CALL_MISSED = 1;
const int CALL_INCOMMING = 2;
const int CALL_OUTGOING = 3;

struct Call {
  int type;
  String call_datetime;
  String caller_phone_number;
  String called_phone_number;
};

uint how_many_available_calls = 0;
uint currently_displayed_call = 0;
Call calls[MAX_CALLS_TO_DISPLAY];


void setup_wifi_connection() {
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

void parse_missed_or_incoming_calls(String call_list_xml) {
  Serial.println("Parsing calls...");

  uint call_add_index = 0;
  for (uint i = 0; i < MAX_CALLS_TO_DISPLAY; i++) {
    String call_xml_line = get_xml_tag(call_list_xml, "Call");

    bool all_calls_are_parsed = !call_xml_line.length();
    if (all_calls_are_parsed) {
      Serial.println("All calls are parsed");
      break;
    }

    Call call = {};
    call.type = get_xml_tag_value(call_xml_line, "Type").toInt();
    call.call_datetime = get_xml_tag_value(call_xml_line, "Date");
    call.caller_phone_number = get_xml_tag_value(call_xml_line, "Caller");
    call.called_phone_number = get_xml_tag_value(call_xml_line, "Called");
    
    calls[call_add_index] = call;
    how_many_available_calls += 1;
    call_add_index += 1;

    call_list_xml = remove_string(call_list_xml, call_xml_line);
  }
}



// REFACTOR: Move to toplevel declaration!
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 4
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


void display_call() {
  Call call = calls[currently_displayed_call];

  String call_date = call.call_datetime.substring(0, 5);
  String call_time = call.call_datetime.substring(9, 14);

  String number;
  String call_type_acronym;
  if (call.type == CALL_MISSED) {
    call_type_acronym = "Verpasst";
    number = call.caller_phone_number;
  } else if (call.type == CALL_INCOMMING) {
    call_type_acronym = "Eingehend";
    number = call.caller_phone_number;
  } else if (call.type == CALL_OUTGOING) {
    call_type_acronym = "Ausgehend";
    number = call.called_phone_number;
  }

  display.clearDisplay();
  display.setCursor(0, 14);
  display.setTextColor(WHITE);
  display.printf("%s %s", call_date, call_time);
  display.setCursor(0, 35);
  display.println(number);
  display.setCursor(0, 56);
  display.println(call_type_acronym);
  display.display();
}

void setup() {
  pinMode(4, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }

  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(WHITE);

  display.clearDisplay();
  display.setCursor(0, 14);
  display.println("Verbinde mit Wlan");
  display.display();

  setup_wifi_connection();
  
  display.clearDisplay();
  display.setCursor(0, 14);
  display.println("Anrufe werden geladen");
  display.display();

  String call_list_xml = request_call_list_xml();
  if (!call_list_xml.length()) {
    display.clearDisplay();
    display.setCursor(0, 14);
    display.println("Fehler beim laden der Anrufe!");
    display.display();
    return;
  }

  Serial.println(call_list_xml);

  parse_missed_or_incoming_calls(call_list_xml);

  Serial.printf("%i calls available\n", how_many_available_calls);

  currently_displayed_call = 0;
  display_call();
}


bool button_was_pressed = false;
bool is_buildin_led_turned_on = false;


void loop() {
  int button_state = digitalRead(4);

  if (button_state == LOW) {
    button_was_pressed = true;
  } else if (button_state == HIGH && button_was_pressed) {
    button_was_pressed = false;

    currently_displayed_call += 1;
    if (currently_displayed_call >= how_many_available_calls) {
      currently_displayed_call = 0;
    }
    display_call();
  }

  delay(50);
}
