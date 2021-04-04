// Data Corridor for Main Qube
#include <Arduino.h>
//
#include "namedMesh.h"
//
#include "ArduinoJson.h"
//
#include <SoftwareSerial.h>
//
#include "fota.h"

#define MESH_PREFIX "qubeMeshNet"
#define MESH_PASSWORD "iridiumcoresat12"
#define MESH_PORT 5555
String this_node_name = "master";
String FIRMWARE_VERSION = "data-corridor_1.0.0";

Scheduler userScheduler;  // To control application level tasks
namedMesh mesh;

SoftwareSerial debugSerial(0, 2);
void sendMessage();
Task taskSendMessage(500ul, TASK_FOREVER, &sendMessage);

/*
See if the serial interface has any messages.
If yes, extract them and send it to the respective node.
*/
void sendMessage() {
    if (Serial.available() > 0) {
        debugSerial.println("Received a message from Gateway.");

        String data = Serial.readStringUntil('\r');
        StaticJsonDocument<300> doc;

        // Check for errors in JSON
        DeserializationError error = deserializeJson(doc, data);
        if (error) {
            debugSerial.print(F("deserializeJson() failed: "));
            debugSerial.println(error.f_str());
            return;
        }

        int msg_type = doc["t"];

        // Do I need to update myself?
        if (msg_type == -4) {
            debugSerial.println("Starting OTA update for data corridor");

            String version = doc["version"];
            String ssid = doc["ssid"];
            String pass = doc["pass"];
            if (!version.equals(FIRMWARE_VERSION)) {
                // Disable tasks
                // taskSendMessage.disable();
                // Stop mesh to connect to WiFi in station mode
                // mesh.stop();

                // Now it's safe to perform OTA update
                fota::performOTAUpdate(doc["version"], doc["ssid"],
                                       doc["pass"]);
            } else {
                debugSerial.println(
                    "Skipping OTA update. Device has the same firmware version "
                    "installed.");
                return;
            }
        } else {
            // Who do we have to send it to?
            String send_to = doc["send_to"];
            doc.remove("send_to");

            // Package up the new data and send it to the node
            String msg_data;
            serializeJson(doc, msg_data);

            // Print information on debug serial interface
            debugSerial.print("Sending msg: ");
            debugSerial.print(msg_data);
            debugSerial.print(" to ");
            debugSerial.println(send_to);

            // Sending message to nodes
            mesh.sendSingle(send_to, msg_data);
        }

    } else {
        debugSerial.println("No data to send to nodes.");
    }
}

/*
This function is called whenever it receives a message from a node.
It gets the message, and sends it to ESP32 via serial interface.
*/
void receivedCallback(uint32_t from, String &msg) {
    debugSerial.println("Received a message.");

    StaticJsonDocument<300> doc;

    // Check for errors in JSON
    DeserializationError error = deserializeJson(doc, msg);
    if (error) {
        debugSerial.print(F("deserializeJson() failed: "));
        debugSerial.println(error.f_str());
        return;
    }

    // Find the device id of the sender node.
    doc["from"] = mesh.getNameFromId(from);
    doc["dc_fw"] = FIRMWARE_VERSION;
    String final_data;
    serializeJson(doc, final_data);

    // Send the data via serial interface
    final_data = final_data + '\r';
    Serial.print(final_data);

    debugSerial.printf("startHere: Received from %u msg=%s\n", from,
                       msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    debugSerial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
    debugSerial.printf("Changed connections\n");
}

void setup() {
    // Setup serial interfaces
    Serial.begin(9600);
    debugSerial.begin(9600);
    delay(100);

    // Setting up mesh
    debugSerial.print("Setting up mesh...");
    mesh.setDebugMsgTypes(ERROR | STARTUP);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.setName(this_node_name);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onReceive(&receivedCallback);

    debugSerial.println("COMPLETE");

    // Adding tasks to scheduler
    userScheduler.addTask(taskSendMessage);
    taskSendMessage.enable();

    debugSerial.println("Setup() complete");
}

void loop() {
    // it will run the user scheduler as well
    mesh.update();
}