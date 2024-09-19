#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

const char *ssid = "Project";
const char *password = "12345679";
DNSServer dnsServer;
IPAddress apIP(10, 0, 0, 1);
ESP8266WebServer webServer(80);

// Variable to store the state of d1
bool d1Enabled = false;

void setup() {
  Serial.begin(9600);

  // Set up the Wi-Fi access point
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);

  // Start the DNS server
  dnsServer.start(53, "*", apIP);

  // Define the root URL handler
  webServer.on("/", []() {
    webServer.send(200, "text/plain", "Hello, World!");
  });

  // Define the /listen/d1 endpoint for POST requests
  webServer.on("/listen/d1", HTTP_POST, []() {
    // Return the current state of d1
    String response = d1Enabled ? "d1 is enabled" : "d1 is disabled";
    webServer.send(200, "text/plain", response);
  });

  // Define the /send/d1 endpoint for POST requests
  webServer.on("/send/d1", HTTP_POST, []() {
    // Enable d1
    d1Enabled = true;
    webServer.send(200, "text/plain", "d1 has been enabled");
  });

  // Start the web server
  webServer.begin();
  
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // Handle DNS requests
  dnsServer.processNextRequest();

  // Handle web server requests
  webServer.handleClient();
}