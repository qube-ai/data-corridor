// Data Corridor for Main Qube
#include <Arduino.h>
#include "namedMesh.h"
#include "ArduinoJson.h"
#include <SoftwareSerial.h>


#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555
String this_node_name = "master";

Scheduler userScheduler;  // To control application level tasks
namedMesh mesh;

SoftwareSerial debugSerial(0, 2);

/*
See if the serial interface has any messages.
If yes, extract them and send it to the respective node.
*/
void sendMessage() {
    if (Serial.available() > 0) {
        debugSerial.println("Gateway sends a message.");
        String data = Serial.readStringUntil('\r');
        StaticJsonDocument<300> doc;

        // Check for errors in JSON
        DeserializationError error = deserializeJson(doc, data);
        if (error) {
            debugSerial.print(F("deserializeJson() failed: "));
            debugSerial.println(error.f_str());
            return;
        }

        // Who do we have to send it to?
        String send_to = doc["send_to"];
        doc.remove("send_to");

        // Package up the new data and send it to the node
        String msg_data;
        serializeJson(doc, msg_data);
        Serial.println(msg_data);
        mesh.sendSingle(send_to, msg_data);

    } else {
        debugSerial.println("No data to send to nodes.");
    }
}

Task taskSendMessage(500ul, TASK_FOREVER, &sendMessage);

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
    String final_data;
    serializeJson(doc, final_data);

    // Send the data via serial interface
    final_data = final_data + "\r";
    Serial.print(final_data);

    debugSerial.printf("startHere: Received from %u msg=%s\n", from,
                       msg.c_str());
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