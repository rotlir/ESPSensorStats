#include <U8g2lib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

/*
If you wish to calibrate values of the ESP.getVcc() function,
set DEBUG_MEASURE_VOLTAGE and CALIBRATE_VOLTAGE to true,
then apply 4.2v and 3.3v to the device and write readings into CAL_4_2V_VOLTAGE and CAL_3_3V_VOLTAGE.
After that set DEBUG_MEASURE_VOLTAGE to false again and leave CALIBRATE_VOLTAGE true.
*/
#define CALIBRATE_VOLTAGE true
#define DEBUG_MEASURE_VOLTAGE false
#define CAL_4_2V_VOLTAGE 2.700
#define CAL_3_3V_VOLTAGE 1.900

#define SHOW_LOGO true

#define DISP_CLOCK D7
#define DISP_DATA D2
#define DISP_CS D1
#define BT0 D4
#define BT1 D5
#define BT2 D6
#define BT3 D3
#define BR_PIN D8

const char ssid[] = "";
const char password[] = "";

U8G2_ST7920_128X64_F_SW_SPI disp(U8G2_R2, DISP_CLOCK, DISP_DATA, DISP_CS);
uint8_t br = 255;
int ascent, descent;
ESP8266WebServer server(80); 
ADC_MODE(ADC_VCC); //required for measuring the controller's supply voltage

int drawInterface(bool draw = true) {
  disp.setDrawColor(0);
  disp.setFont(u8g2_font_tiny5_tf);
  if(!draw) {
    uint8_t y = disp.getAscent() - disp.getDescent() + 2;
    disp.setDrawColor(1);
    disp.setFont(u8g2_font_6x10_tf);
    return y;
  }
  disp.drawBox(0, 0, disp.getDisplayWidth(), disp.getAscent() - disp.getDescent() + 1);
  disp.setDrawColor(1);
  float voltage = getVoltage();
  char buf[7];
  dtostrf(voltage, 6, 3, buf);
  uint8_t i = 0;
  buf[6] = 'v'; 
  disp.drawStr(0, 0, buf);
  uint8_t y = disp.getAscent() - disp.getDescent() + 1;
  unsigned long t = millis();
  uint16_t hours = t / 3600000;
  uint16_t minutes = (t - hours * 3600000) / 60000;
  char c_time[8];
  String s_time;
  if(hours < 10) {
    s_time += "0";
  }
  s_time += hours;
  s_time += ":";
  if(minutes < 10) {
    s_time += "0";    
  }
  s_time += minutes;
  s_time.toCharArray(c_time, 8);
  int x = (disp.getDisplayWidth() - disp.getStrWidth(c_time)) / 2;
  disp.drawStr(x, 0, c_time);
  disp.drawLine(0, y, disp.getDisplayWidth(), y);
  disp.setFont(u8g2_font_6x10_tf);
  disp.sendBuffer();
  return y + 1;
}

//this function performs a delay and changes brightness of the display in case of pressing buttons
void brightnessCheck(uint16_t del, bool serverBegun = false) {
  unsigned long n = millis() + del;
  while(millis() <= n) {
    if(!digitalRead(BT0) & br != 0) {
      br--;
    }
    if(!digitalRead(BT3) & br != 255) {
      br++;
    }
    if(serverBegun) server.handleClient();
    delay(1);
    analogWrite(BR_PIN, br);
  }
    if(serverBegun) drawInterface();   
}

//this function gets the controller's supply voltage and maps it to a correct range if CALIBRATE_VOLTAGE is true
float getVoltage() {
  float voltage = static_cast<float>(ESP.getVcc())/1024.0f;
  if(CALIBRATE_VOLTAGE) {
    voltage = (((voltage - CAL_3_3V_VOLTAGE) * 0.9f) / (CAL_4_2V_VOLTAGE - CAL_3_3V_VOLTAGE)) + 3.3; 
  }
  return voltage;
}

//this function prints the controller's supply voltage on the display
void debug_measure_voltage() {
  float voltage = getVoltage();
  char buf[6];
  dtostrf(voltage, 5, 3, buf);
  disp.drawStr(0, 0, buf);
  disp.sendBuffer();
  disp.clearBuffer();
  brightnessCheck(1000);
}



//this function handles incoming requests from the server 
void handleRequest() {
  StaticJsonDocument<192> doc;
  char data[server.arg(0).length() + 1];
  server.arg(0).toCharArray(data, sizeof(data));
  Serial.println(data);
  deserializeJson(doc, data);
  int y = drawInterface(false);
  disp.setDrawColor(0);
  disp.drawBox(0, y, disp.getDisplayWidth(), disp.getDisplayHeight());
  disp.setDrawColor(1);
  for(JsonPair item : doc.as<JsonObject>()) {
    const char* key = item.key().c_str();
    const char* unit;
    if(!item.value()["unit"].isNull()) {
      unit = item.value()["unit"];
    } else {
      unit = "";
    }
    char val[8];
    itoa(item.value()["value"], val, 10);
    String buf;
    buf += key;
    buf += " ";
    buf += val;
    buf += " ";
    buf += unit;
    char c_buf[buf.length() + 1];
    buf.toCharArray(c_buf, sizeof(c_buf));
    disp.drawUTF8(0, y, c_buf);
    Serial.println(c_buf);
    y += ascent - descent + 1;
  }
  disp.sendBuffer();
  server.send(200);
}

void setup() {
  disp.begin();
  disp.setFont(u8g2_font_6x10_tf);
  disp.setFontPosTop();
  disp.setFontRefHeightExtendedText();
  ascent = disp.getAscent();
  descent = disp.getDescent();
  analogWrite(BR_PIN, br);
  pinMode(BT0, INPUT);
  pinMode(BT1, INPUT);
  pinMode(BT2, INPUT);
  pinMode(BT3, INPUT);
  Serial.begin(115200);
  if(DEBUG_MEASURE_VOLTAGE) {
    while(true) debug_measure_voltage();
  }
  //connecting to WiFi
  char con_text[18 + sizeof(ssid)] = "Connecting to ";
  for(int i = 14; i <= 14 + sizeof(ssid); i++) {
    con_text[i] = ssid[i - 14];
  }
  disp.drawStr(0, 0, con_text);
  disp.sendBuffer();
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    brightnessCheck(500);   
    disp.drawStr(0, 0, con_text);
    disp.sendBuffer();
    disp.clearBuffer();
  }
  disp.sendBuffer();
  //printing local IP address 
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //attaching a function to handle incoming requests
  server.on("/", handleRequest);
  //starting server
  server.begin();
}

void loop() {
  brightnessCheck(1000, true);
}
