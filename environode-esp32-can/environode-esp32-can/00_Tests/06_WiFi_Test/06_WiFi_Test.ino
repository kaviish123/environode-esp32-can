// 06_WiFi_Test.ino
// Tests Wi-Fi and internet time. Put your Wi-Fi name and password below.
// The ESP32 only works on 2.4 GHz Wi-Fi (not 5 GHz).
// Expected: "Connected", the IP address, then the current Indian time every 5 s.

#include <WiFi.h>
#include <time.h>

const char *WIFI_SSID = "YOUR_WIFI_NAME";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

void setup() {
  Serial.begin(115200);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  configTime(19800, 0, "pool.ntp.org");   // India = UTC + 5:30
}

void loop() {
  struct tm t;
  if (getLocalTime(&t)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    Serial.println(buf);
  } else {
    Serial.println("Waiting for internet time...");
  }
  delay(5000);
}
