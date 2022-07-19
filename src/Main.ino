#include <stdio.h>

// Prod.
// const char* BACKEND_URL = "taller3-backend.herokuapp.com";
// const int BACKEND_PORT = 80;

// Dev.
const char* BACKEND_URL = "192.168.0.189";
const int BACKEND_PORT = 5000;

const char* NETWORK = "";
const char* NETWORK_PASS = "";

const char* TASKS_ENDPOINT = "/devices/tasks/argon";
const char* CONTENT_LENGTH_HEADER = "Content-Length";

const int PAYLOAD_BUF_SIZE = 2048;

SerialLogHandler logHandler;
TCPClient client;

void setup()
{
    WiFi.setCredentials(NETWORK, NETWORK_PASS);

    pinMode(A0, OUTPUT);
    pinMode(A5, INPUT_PULLDOWN);
}

void heartBeatLight()
{
    digitalWrite(A0, HIGH);
    delay(100);
    digitalWrite(A0, LOW);
    delay(100);
}

String nextHeader(TCPClient c)
{
    String header;
    uint8_t charRead = '\0';

    while (c.read(&charRead, 1) > 0 && charRead != '\r')
        header = header + (char)charRead;

    // Let's skip the next '\r' and '\n'
    c.read(NULL, 1);

    return header;
}

void processPayload(String p)
{
    /**
     *  Example payload:
     *  {
     *    'tasks': [
     *      {'id': 3, 'task_name': 'task', 'task_params': null },
     *      {'id': 7, 'task_name': 'test', 'task_params': { 'field': 'value' } }
     *    ]
     *  }
     */

    JSONObjectIterator tasks(JSONValue::parseCopy(p));

    while (tasks.next())
    {
        JSONArrayIterator tasks_i(tasks.value());
        
        while (tasks_i.next())
        {
            JSONObjectIterator task_attrs(tasks_i.value());

            while (task_attrs.next())
            {
                // Regular attrs.

                if (task_attrs.name() == "task_name")
                {
                    Log.info("Value = %s", (const char *)task_attrs.value().toString().data());

                    if (task_attrs.value().toString() == "led_on")
                    {
                        digitalWrite(A0, HIGH);
                    }
                    else
                    {
                        digitalWrite(A0, LOW);
                    }
                }
                
                // Extra params.

                if (task_attrs.name() == "task_params")
                {

                    JSONObjectIterator task_extra_params(task_attrs.value());

                    while (task_extra_params.next()) {

                        Log.info(
                            "Extra param: %s = %s", 
                            (const char *)task_extra_params.name(),
                            (const char *)task_extra_params.value().toString().data()
                        );
                    }
               }
            }
        }
    }
}

void processResponse(TCPClient c)
{
    int payloadLength = 0;

    // Parse headers
    for (;;)
    {
        String h = nextHeader(c);

        // If we reached end of headers just break.
        if (!h.length()) break;

        if (h.startsWith(CONTENT_LENGTH_HEADER))
        {
            String value = h.substring(h.indexOf(':') + 1);

            Log.info("Content length: " + value);

            value.trim();

            // `payloadlength` is taking in account the final '\n'.
            payloadLength = value.toInt() - 1;
        }
    }

    uint8_t payloadbuf[PAYLOAD_BUF_SIZE];
    
    // Initialize the array.
    for (int i = 0; i < PAYLOAD_BUF_SIZE; i++)
        payloadbuf[i] = 0;

    int bytesRead = 0;
    while ((bytesRead += c.read(&payloadbuf[bytesRead], payloadLength)) < payloadLength);

    payloadbuf[payloadLength] = '\0';
    
    String payload((const char *)payloadbuf);

    Log.info("Captured payload with length: %d", payload.length());
    
    processPayload(payload);
}

bool connected = false;

void sendTaskRequest(TCPClient c) 
{
    String request;
    
    request = request + String("GET ") + String(TASKS_ENDPOINT) + String(" HTTP/1.1\n");
    request = request + String("Host: ") + String(BACKEND_URL);
    request = request + String("Content-Length: 0");

    client.println(request.c_str());
    client.println();
}

void loop()
{
    if (!connected) {
        if(client.connect(BACKEND_URL, BACKEND_PORT)) {
            Log.info("Connected to %s", BACKEND_URL);
            connected = true;
        }
        
        sendTaskRequest(client);
    }

    if (connected && client.available()) 
        processResponse(client);

    if (connected && !client.connected()) {
        client.stop();
        connected = false;
    }

    // System.sleep(D1, RISING, 10);

    delay(1500);

    double acum = 0;

    for (int i = 0; i < 3; i++)
        acum +=  analogRead(A5);

    Log.info("Read %.2f", (acum / 3));
}
