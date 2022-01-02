#define DEBUG 1

#include <ESP32Time.h>

#include <GxEPD.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
//#include "IMG.h"
#include <NTPClient.h>
#include <WiFi.h>
#include "driver/adc.h"

#include "variables.h"

#define PIN_MOTOR 4
#define PIN_KEY 35
#define PWR_EN 5
#define Backlight 33

#define SPI_SCK 14
#define SPI_DIN 13
#define EPD_CS 15
#define EPD_DC 2
#define SRAM_CS -1
#define EPD_RESET 17
#define EPD_BUSY 16

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntp_server, time_shift, time_interval);
GxIO_Class io(SPI, /*CS=5*/ 15, /*DC=*/2, /*RST=*/17);
GxEPD_Class display(io, /*RST=*/17, /*BUSY=*/16);

ESP32Time tm;

void debug(const char *text){
#ifdef DEBUG
  Serial.println(text);
#endif
}

#define MAX_STRING_SIZE 64
static char sprint_buf[MAX_STRING_SIZE];

const char *myprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(sprint_buf, MAX_STRING_SIZE, format, args);
    va_end(args);
    return sprint_buf;
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason)
  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}

void setModemSleep();
void wakeModemSleep();
 
void setup()
{
  Serial.begin(115200);
  delay(10);
  SPI.begin(SPI_SCK, -1, SPI_DIN, EPD_CS);
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PWR_EN, OUTPUT);
  digitalWrite(PWR_EN, HIGH);

  digitalWrite(PIN_MOTOR, HIGH);
  delay(200);
  digitalWrite(PIN_MOTOR, LOW);
  delay(100);
  digitalWrite(PIN_MOTOR, HIGH);
  delay(200);
  digitalWrite(PIN_MOTOR, LOW);
  Serial.println("ESP32 WIFI NTC test");
  display.init();
  display.setRotation(0);
//  display.fillScreen(GxEPD_WHITE);
//  display.drawBitmap(0, 0, lilygo, 200, 200, GxEPD_BLACK);
//  display.update();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setTextWrap(true);
  display.eraseDisplay();
  display.setTextSize(5);
  display.fillScreen(GxEPD_WHITE);

/*  setModemSleep();
//  Serial.println("MODEM SLEEP ENABLED FOR 5secs");
/*  esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR*TIME_TO_SLEEP);
/*  display.setCursor(0, 73);
  display.println("Beijing time :");
  display.setTextSize(3);
  */
}
bool ntp_update(){
  long new_time;
  
  wakeModemSleep();
  debug("no sleep...");

  if(WiFi.status() != WL_CONNECTED){
    Serial.print("not connectd...");
    return false;
  }
  timeClient.update();
  new_time = timeClient.getEpochTime();
  tm.setTime(new_time);
  
  debug("sleep...");
  setModemSleep();
  debug("yea, sleep...");
  return true;
}

void update_image(){
  static int old_hour=-1, old_minute=-1, new_hour, new_minute;
  new_hour = tm.getHour(true);
  new_minute = tm.getMinute();
  if(new_hour != old_hour || new_minute != old_minute){
    display.init();
    display.setRotation(0);
    display.setCursor(18, 100);
    display.fillRect(0, 100, 200, 36, GxEPD_WHITE);
    display.print(myprintf("%02d:%02d",new_hour,new_minute));
//    display.print(tm.getTime("%H:%M"));
    display.updateWindow(0, 100, 200, 36);
    delay(1000);
    display.powerDown();
    debug("time printed");
    old_hour = new_hour;
    old_minute = new_minute;
  }
}

void loop() {
  static unsigned long epoch, last_epoch=0;
  epoch = tm.getEpoch();
  if(last_epoch==0 || (epoch - last_epoch > MAX_DELAY)){
    // Update the time via ntp!
    if(ntp_update()){
      Serial.printf("updated! old=%ld new=%ld\n",last_epoch, epoch);
      last_epoch = epoch;
    }
  }
  update_image();
  Serial.printf("light_sleep!\n");
  delay(100);
  setCpuFrequencyMhz(40);
  esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR*TIME_TO_SLEEP);
  int ret = esp_light_sleep_start();
  setCpuFrequencyMhz(240);
  Serial.printf("light_sleep: %d\n", ret);
}
 

void disableWiFi(){
    adc_power_off();
    WiFi.disconnect(true);  // Disconnect from the network
    WiFi.mode(WIFI_OFF);    // Switch WiFi off
    debug("");
    debug("WiFi disconnected!");
}
void disableBluetooth(){
    // Quite unusefully, no relevable power consumption
    btStop();
    debug("");
    debug("Bluetooth stop!");
}
 
void setModemSleep() {
    disableWiFi();
//    disableBluetooth();
//    setCpuFrequencyMhz(40);
    // Use this if 40Mhz is not supported
//    setCpuFrequencyMhz(80);
}
 
void enableWiFi(){
  char IP[] = "xxx.xxx.xxx.xxx";          // buffer
  adc_power_on();
  delay(200);
 
  WiFi.disconnect(false);  // Reconnect the network
  WiFi.mode(WIFI_STA);    // Switch WiFi off
 
  delay(200);
 
  debug("START WIFI");
  WiFi.begin(ssid, password);
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_2dBm);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  debug("");
  debug("WiFi connected:");
  WiFi.localIP().toString().toCharArray(IP, 16);
  debug(IP);
    
  timeClient.begin();
}
 
void wakeModemSleep() {
    setCpuFrequencyMhz(240);
    enableWiFi();
}


/*
void loop2()
{
  uint8_t RealTime, LastTime = 0;

  print_wakeup_reason();
  
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("not connectd...");
  }
  timeClient.update();
  Serial.println("new time");
  RealTime = timeClient.getSeconds();
  if (LastTime != RealTime)
  {
    LastTime = RealTime;
    display.setCursor(18, 100);
    display.fillRect(0, 100, 200, 36, GxEPD_WHITE);
//    display.print(timeClient.getFormattedTime());
    display.print(myprintf("%02d:%02d",timeClient.getHours(),timeClient.getMinutes()));
    display.updateWindow(0, 100, 200, 36);
    display.updateWindow(0, 100, 200, 36);
  }
  delay(200);
  display.powerDown();
  Serial.println("OK!");
//  delay(1000);
//  Serial.println("Sleep...");
//  DeepSleep();
//  Serial.println("Woked up!");
}


void DeepSleep(void)
{
  display.powerDown();
  pinMode(PIN_MOTOR, INPUT_PULLDOWN);
  pinMode(PWR_EN, INPUT);
  pinMode(PIN_KEY, INPUT);
//  pinMode(SPI_SCK, INPUT);
//  pinMode(SPI_DIN, INPUT);
  pinMode(EPD_CS, INPUT);
  pinMode(EPD_DC, INPUT);
  pinMode(EPD_RESET, INPUT);
  pinMode(EPD_BUSY, INPUT);
  pinMode(PIN_MOTOR, INPUT);
//  pinMode(Backlight, INPUT),
  // esp_sleep_enable_ext0_wakeup(PIN_KEY, 0);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  int ret = esp_light_sleep_start();
  Serial.printf("light_sleep: %d\n", ret);
}
*/
