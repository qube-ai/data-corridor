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

SoftwareSerial debugSerial(0,2);

bool led_state = false;
String final_str;

void set_relay_state(String to_node, bool relay_state) {}

void sendMessage() {
    
    if(Serial.available() > 0) {
        Serial.println("Gateway sends a message.");
        String data = Serial.readStringUntil('\r');
        StaticJsonDocument<300> doc;

        DeserializationError error = deserializeJson(doc, data);
        if (error) {
            debugSerial.print(F("deserializeJson() failed: "));
            debugSerial.println(error.f_str());
            return;
        }

        String send_to = doc["send_to"];
        doc.remove("send_to");

        String msg_data;
        serializeJson(doc, msg_data);
        Serial.println(msg_data);
        mesh.sendSingle(send_to, msg_data);
    }
    else {
        debugSerial.println("No data to send to baby qubes");
    }
}

void taskReadIncomingMessage(){
    String data = Serial.readStringUntil('\r');
    Serial.println("Got something in buffer.");
    Serial.print(data);
}

Task taskSendMessage(500ul, TASK_FOREVER, &sendMessage);


void receivedCallback(uint32_t from, String &msg) {
    Serial.println("Received a message.");
    
    StaticJsonDocument<300> doc;

    DeserializationError error = deserializeJson(doc, msg);
    if (error) {
        debugSerial.print(F("deserializeJson() failed: "));
        debugSerial.println(error.f_str());
        return;
    }
    doc["from"] = mesh.getNameFromId(from);
    String final_data;
    serializeJson(doc, final_data);

    // Send the data via serial interface
    final_data = final_data + "\r";
    Serial.print(final_data);

    debugSerial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    // Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
    //  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    // Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),
    //               offset);
}


void setup() {
    Serial.begin(9600);
    debugSerial.begin(9600);
    delay(100);

    /* Setting up the Mesh */
    // mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC |
    // COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
    mesh.setDebugMsgTypes(ERROR | STARTUP);

    debugSerial.println("Mesh debug type set");
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
    mesh.setName(this_node_name);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    debugSerial.println("Mesh properties set");

    userScheduler.addTask(taskSendMessage);
    taskSendMessage.enable();

    debugSerial.println("Setup() complete");
}

void loop() {
    // it will run the user scheduler as well
    mesh.update();
}