const char *ssid = "SSID";
const char *password = "password";

const char ntp_server = "1.ru.pool.ntp.org";  // change to your favorite
const long time_shift = 60 * 60 * 3;          // +03:00
const long time_interval = 30 * 60 * 1000;    // 30 minutes

#define uS_TO_S_FACTOR 1000000 /* microseconds in one second */
#define TIME_TO_SLEEP 10       /* secs to sleep ESP32 */
#define MAX_DELAY 300          /* NTP update interval in secs */
