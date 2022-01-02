#include <ESP32Time.h>

#include <GxEPD.h>
#include <GxDEPG0150BN/GxDEPG0150BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include "IMG.h"
#include <NTPClient.h>
#include <WiFi.h>
#include "driver/adc.h"

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
NTPClient timeClient(ntpUDP, "1.ru.pool.ntp.org", 60 * 60 * 3, 30 * 60 * 1000);
GxIO_Class io(SPI, /*CS=5*/ 15, /*DC=*/2, /*RST=*/17);
GxEPD_Class display(io, /*RST=*/17, /*BUSY=*/16);

ESP32Time tm;

const char *ssid = "ZS";      //"your ssid";
const char *password = "close-open"; //"your password";

#define MAX_STRING_SIZE 64
static char sprint_buf[MAX_STRING_SIZE];

#define uS_TO_S_FACTOR 1000000 /* коэффициент пересчета
                                  микросекунд в секунды */
#define TIME_TO_SLEEP 10       /* время, в течение которого
                                  будет спать ESP32 (в секундах) */

#define MAX_DELAY 300          /* интервал между обращениями к NTP (сек)*/

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
                         //  "Пробуждение от внешнего сигнала с помощью RTC_IO"
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
                         //  "Пробуждение от внешнего сигнала с помощью RTC_CNTL"  
    case 3  : Serial.println("Wakeup caused by timer"); break;
                         //  "Пробуждение от таймера"
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
                         //  "Пробуждение от сенсорного контакта"
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
                         //  "Пробуждение от ULP-программы"
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
                         //  "Пробуждение не связано с режимом глубокого сна"
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
  Serial.println("no sleep...");

  if(WiFi.status() != WL_CONNECTED){
    Serial.print("not connectd...");
    return false;
  }
  timeClient.update();
  new_time = timeClient.getEpochTime();
  tm.setTime(new_time);
  
  Serial.println("sleep...");
  setModemSleep();
  Serial.println("yea, sleep...");
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
    Serial.println("time printed");
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
    Serial.println("");
    Serial.println("WiFi disconnected!");
}
void disableBluetooth(){
    // Quite unusefully, no relevable power consumption
    btStop();
    Serial.println("");
    Serial.println("Bluetooth stop!");
}
 
void setModemSleep() {
    disableWiFi();
//    disableBluetooth();
//    setCpuFrequencyMhz(40);
    // Use this if 40Mhz is not supported
//    setCpuFrequencyMhz(80);
}
 
void enableWiFi(){
    adc_power_on();
    delay(200);
 
    WiFi.disconnect(false);  // Reconnect the network
    WiFi.mode(WIFI_STA);    // Switch WiFi off
 
    delay(200);
 
    Serial.println("START WIFI");
    WiFi.begin(ssid, password);
    WiFi.persistent(false);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_2dBm);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
 
    Serial.println("");
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
    
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
