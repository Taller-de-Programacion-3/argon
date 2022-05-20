// Variables

#include <stdio.h>

SerialLogHandler logHandler;
TCPClient client;

const char* BACKEND_URL = "taller3-backend.herokuapp.com";
const char* TASKS_ENDPOINT = "/tasks/argon";
const int BACKEND_PORT = 80;

char buf[150];

void setup() {
  char headerbuf[50];

  pinMode(A0, OUTPUT);
  
  if (client.connect(BACKEND_URL, BACKEND_PORT)) {
    Log.info("Connected to: %s", BACKEND_URL);

    snprintf(headerbuf, sizeof headerbuf, "GET %s HTTP/1.1", TASKS_ENDPOINT);
    client.println(headerbuf);

    snprintf(headerbuf, sizeof headerbuf, "Host: %s", BACKEND_URL);
    client.println(headerbuf);

    client.println("Content-Length: 0");
    client.println();

  } else {
    Log.info("Failed to connect: %s", BACKEND_URL);
  }
}

void heartBeatLight() {
  digitalWrite(A0, HIGH);

  delay(100);

  digitalWrite(A0, LOW);

  delay(100);
}

void loop() {
  if (client.available()) {

    uint8_t c = 'a';
    char buf[100];

    // TODO. check for Content-Length header.
    client.read(&c, 1); 
    Log.info("Read %s", c);

    if (c == 13) {
      client.read(&c, 1); 
      if (c == 10) {
        client.read(&c, 1); 
        if (c == 13) {
          client.read(&c, 1); 
          if (c == 10) {
            client.read((uint8_t*) buf, 99);
            Log.info("Got buffer with %s", buf);
          }
        }
      }
    }
  }
  
  if (!client.connected()) {
    client.stop();
  }

  heartBeatLight();
}
