#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <ESP8266HTTPClient.h>  // For sending HTTP POST requests

// WiFi credentials
const char *ssid = "Project";
const char *password = "12345679";

// DNS and Web Server setup
DNSServer dnsServer;
IPAddress apIP(10, 0, 0, 1);
ESP8266WebServer webServer(80);

// Hardware definitions
Servo servoEntrance;
Servo servoExit;

// Sensor pins
const int slotSensors[] = {D1, D2, D3, D4}; // Parking slots IR sensors
const int entranceSensor = D5;              // Entrance IR sensor
const int exitSensor = D6;                  // Exit IR sensor

// Servo motor pins
const int servoEntrancePin = D7;
const int servoExitPin = D8;

// Variables for slot states and times
bool slotOccupied[] = {false, false, false, false};  // Track if slot is occupied
unsigned long timeIn[] = {0, 0, 0, 0};               // Time of parking (time in)
unsigned long timeOut[] = {0, 0, 0, 0};              // Time of leaving (time out)

// Variables for entrance and exit gate control
unsigned long entranceOpenTime = 0;
unsigned long exitOpenTime = 0;
bool entranceGateOpen = false;
bool exitGateOpen = false;
int carsInside = 0; // Track the number of cars inside

// Function to send log data to PHP
void sendLogData(String url, int parkId) {
    if (WiFi.status() == WL_CONNECTED) {  // Ensure WiFi is connected
        HTTPClient http;
        http.begin("http://localhost/" + url);  // Specify the destination URL
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");  // POST header
        
        // Prepare POST data
        String postData = "id=" + String(parkId);
        
        int httpResponseCode = http.POST(postData);  // Send the POST request
        
        if (httpResponseCode > 0) {
            Serial.println("POST successful: " + String(httpResponseCode));
        } else {
            Serial.println("Error on sending POST: " + String(httpResponseCode));
        }
        
        http.end();  // End the connection
    } else {
        Serial.println("WiFi Disconnected");
    }
}

// Function to handle time in and time out for parking slots
void handleParkingSlots() {
    for (int i = 0; i < 4; i++) {
        int sensorState = digitalRead(slotSensors[i]);

        if (sensorState == LOW && !slotOccupied[i]) { // Car just parked (time in)
            timeIn[i] = millis();  // Record time in
            slotOccupied[i] = true;  // Mark slot as occupied
            sendLogData("time_in.php", i + 1);  // Send time in log to server
            Serial.println("Car parked at slot " + String(i + 1));
        }

        if (sensorState == HIGH && slotOccupied[i]) { // Car just left (time out)
            timeOut[i] = millis();  // Record time out
            slotOccupied[i] = false;  // Mark slot as free
            sendLogData("time_out.php", i + 1);  // Send time out log to server
            Serial.println("Car left from slot " + String(i + 1));
        }
    }
}

// Function to open entrance gate
void openEntranceGate() {
    servoEntrance.write(150);              // Open entrance gate
    entranceOpenTime = millis();           // Record entrance gate open time
    entranceGateOpen = true;  
}

// Function to open exit gate
void openExitGate() {
    servoExit.write(150);                  // Open exit gate
    exitOpenTime = millis();               // Record exit gate open time
    exitGateOpen = true;   
}

// Function to handle the entrance gate operation
void handleEntranceGate() {
    int entranceState = digitalRead(entranceSensor);
    
    // Check entrance sensor
    if (entranceState == LOW && !entranceGateOpen) {
        openEntranceGate();  // Open entrance gate if triggered and not already open
        carsInside++;        // Increment car count
    }

    // Close entrance gate after 3 seconds
    if (entranceGateOpen && millis() - entranceOpenTime > 3000) {
        servoEntrance.write(0);            // Close entrance gate
        entranceGateOpen = false;          // Mark entrance gate as closed
    }
}

// Function to handle the exit gate operation
void handleExitGate() {
    int exitState = digitalRead(exitSensor);
    
    // Check exit sensor
    if (exitState == LOW && !exitGateOpen) {
        openExitGate();       // Open exit gate if triggered and not already open
        if (carsInside > 0) { // Ensure car count does not go negative
            carsInside--;     // Decrement car count
        }
    }

    // Close exit gate after 3 seconds
    if (exitGateOpen && millis() - exitOpenTime > 3000) {
        servoExit.write(0);                   // Close exit gate
        exitGateOpen = false;                 // Mark exit gate as closed
    }
}

// API for sensor states
void handleSensorAPI() {
    String path = webServer.uri();
    int sensorIndex = path.substring(2).toInt() - 1;  // Extract sensor number (adjusted for zero-based indexing)
    
    Serial.println("Sensor Index: " + String(sensorIndex)); // Debugging line

    if (sensorIndex >= 0 && sensorIndex <= 4) {
        int sensorState = digitalRead(slotSensors[sensorIndex]);
        webServer.send(200, "text/plain", String(sensorState));
    } else {
        webServer.send(404, "text/plain", "Sensor not found");
    }
}

// API for number of cars inside
void handleCarCountAPI() {
    webServer.send(200, "text/plain", String(carsInside));  // Return car count as a response
}

void setup() {
    Serial.begin(9600);

    // Initialize sensors
    for (int i = 0; i < 4; i++) {
        pinMode(slotSensors[i], INPUT_PULLUP);
    }
    pinMode(entranceSensor, INPUT_PULLUP);
    pinMode(exitSensor, INPUT_PULLUP);

    // Initialize servo motors
    servoEntrance.attach(servoEntrancePin);
    servoExit.attach(servoExitPin);
    servoEntrance.write(0); // Initial position
    servoExit.write(0);     // Initial position

    // Set up WiFi and DNS server
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, password);
    dnsServer.start(53, "*", apIP);

    // Setup web server routes dynamically
    for (int i = 1; i <= 4; i++) {
        webServer.on("/d" + String(i), handleSensorAPI);
    }
    webServer.on("/open/entrance", []() {
        openEntranceGate();
        webServer.send(200, "text/plain", "Entrance gate opened");
    });
    webServer.on("/open/exit", []() {
        openExitGate();
        webServer.send(200, "text/plain", "Exit gate opened");
    });
    webServer.on("/carsInside", handleCarCountAPI);  // API to get the number of cars inside
    webServer.begin();
}

void loop() {
    dnsServer.processNextRequest();
    webServer.handleClient();
    handleExitGate();
    handleEntranceGate();
    handleParkingSlots();  // Handle parking slot time in/out
}
