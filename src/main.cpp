#define HOSTNAME "WiFiFan"

#include <ArduinoJson.h>
#include <IotWebConf.h>
#include <DNSServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <esp8266_peri.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include "mqtt.h"

#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = HOSTNAME;
// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = HOSTNAME;

#define CONFIG_VERSION "beta4"

DNSServer dnsServer;
ESP8266WebServer server(80);
HTTPUpdateServer httpUpdater;

#define STRING_LEN 128
#define NUMBER_LEN 32

char mqttServer[STRING_LEN] = "";
char mqttPort[NUMBER_LEN] = "";
char mqttUser[STRING_LEN] = "";
char mqttPass[STRING_LEN] = "";

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfSeparator separator = IotWebConfSeparator("MQTT Settings");
IotWebConfParameter mqttServerParam = IotWebConfParameter("Server", "mqttServer", mqttServer, STRING_LEN);
IotWebConfParameter mqttPortParam = IotWebConfParameter("Port", "mqttPort", mqttPort, NUMBER_LEN, "number", "1883", NULL, "min='1' max='65535' step='1'");
IotWebConfParameter mqttUserParam = IotWebConfParameter("User", "mqttUser", mqttUser, STRING_LEN);
IotWebConfParameter mqttPassParam = IotWebConfParameter("Password", "mqttPass", mqttPass, STRING_LEN);

const uint16_t kRecvPin = 14; //D5
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 15; // Suits most messages, while not swallowing many repeats.

IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results; // Somewhere to store the results

Mqtt mqtt();

bool OTASetup = false;

void handle_root()
{
  // -- Captive portal request were already served.
  if (iotWebConf.handleCaptivePortal())
  {
    return;
  }

  String html = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  html += "<title>Hot Tub Remote</title></head><body>Hot Tub Remote<br /><br />";
  html += "Go to <a href='config'>configure page</a> to change settings.<br />";
  html += "Go to <a href='status'>status</a> to view status.<br />";
  html += "</body></html>\n";

  server.send(200, "text/html", html);
}

void setupBoard()
{
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(D3, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);
  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);
  digitalWrite(D7, LOW);

  WiFi.hostname(HOSTNAME);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
}

void setupMqtt()
{
  if (strlen(mqttServer) > 0)
    mqtt.setServer(mqttServer, atoi(mqttPort));

  if (strlen(mqttUser) > 0)
    mqtt.setCredentials(&mqttUser[0], &mqttPass[0]);
}

const char *otaErrorToString(ota_error_t error)
{
  switch (error)
  {
  case OTA_AUTH_ERROR:
    return "Auth Failed";
  case OTA_BEGIN_ERROR:
    return "Begin Failed";
  case OTA_CONNECT_ERROR:
    return "Connect Failed";
  case OTA_RECEIVE_ERROR:
    return "Receive Failed";
  case OTA_END_ERROR:
    return "End Failed";
  default:
    return "";
  }
}

static int lastOTAPercent;
void setupOTA()
{
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.onStart([]() {
    const char *type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.print("OTA->Start updating ");
    Serial.println(type);
  });
  ArduinoOTA.onEnd([]() { Serial.println("OTA->End"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percent = (progress / (total / 100));
    if (lastOTAPercent == percent || percent % 5 != 0)
      return;

    Serial.print("OTA->Progress: ");
    Serial.print(percent);
    Serial.println("%");
    lastOTAPercent = percent;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("OTA->Error: ");
    Serial.println(otaErrorToString(error));
  });
  ArduinoOTA.begin();

  Serial.println("OTA->Setup complete.");
}

void sendStartInfo()
{
  Serial.println("MAIN->##############################");
  Serial.println("MAIN->##     WIFI FAN STARTED     ##");
  Serial.println("MAIN->##############################");

  struct rst_info *rst = system_get_rst_info();
  const char *reasons[] = {"Normal Startup", "Hardware WDT", "Exception", "Software WDT", "Soft Restart", "Deep Sleep Awake", "External System Reset"};

  Serial.print("MAIN->Restart Reason: ");
  Serial.print(reasons[rst->reason]);
  Serial.print(" - ");
  Serial.println(rst->reason);

  if (rst->exccause > 0)
  {
    Serial.print("MAIN->excCause: ");
    Serial.print(rst->exccause);
    Serial.print(", excVaddr: ");
    Serial.print(rst->excvaddr);
    Serial.print(", epc1: ");
    Serial.print(rst->epc1);
    Serial.print(", epc2: ");
    Serial.print(rst->epc2);
    Serial.print(", epc3: ");
    Serial.print(rst->epc3);
    Serial.print(", depc: ");
    Serial.println(rst->depc);
  }
}

void wifiConnected()
{
  setupOTA();

  if (strlen(mqttServer) > 0)
    setupMqtt();
}

void setupIotWebConf()
{
  iotWebConf.setStatusPin(LED_BUILTIN);
  iotWebConf.skipApStartup();
  iotWebConf.setupUpdateServer(&httpUpdater);
  iotWebConf.setWifiConnectionCallback(wifiConnected);

  iotWebConf.addParameter(&separator);
  iotWebConf.addParameter(&mqttServerParam);
  iotWebConf.addParameter(&mqttPortParam);
  iotWebConf.addParameter(&mqttUserParam);
  iotWebConf.addParameter(&mqttPassParam);

  iotWebConf.init();
}

void handleNotFound()
{
  iotWebConf.handleNotFound();
}

void setupWebServer()
{
  // -- Set up required URL handlers on the web server.
  server.on("/", handle_root);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("MAIN->HTTP server started.");
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  mqtt.callback(topic, payload, length);
}

void setup(void)
{
  setupBoard();
  setupIotWebConf();
  setupWebServer();
  mqtt.setup(*mqttCallback);
  irrecv.enableIRIn(); // Start the receiver
}

void loop(void)
{
  ArduinoOTA.handle();
  iotWebConf.doLoop();
  server.handleClient();

  mqtt.loop();

  // Check if the IR code has been received.
  if (irrecv.decode(&results))
  {
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
    // Check if we got an IR message that was to big for our capture buffer.

    if (results.overflow)
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);

    // Display the library version the message was captured with.
    Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_ "\n");
    // Display the basic output of what we found.
    Serial.print(resultToHumanReadableBasic(&results));

    // Output the results as source code
    Serial.println(resultToSourceCode(&results));
    Serial.println(); // Blank line between entries
    yield();          // Feed the WDT (again)
  }
}