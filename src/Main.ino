#include <stdio.h>

SerialLogHandler logHandler;
TCPClient client;

const char* BACKEND_URL = "taller3-backend.herokuapp.com";
const char* TASKS_ENDPOINT = "/tasks/argon";
const char* CONTENT_LENGTH_HEADER = "Content-Length";
const int BACKEND_PORT = 80;

char buf[150];
bool waitingForResponse = false;

void setup()
{
    WiFi.setCredentials("Fibertel WiFi881 2.4GHz", "00412403610");

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

String parseHeader(TCPClient c)
{
    String header;
    uint8_t charRead = '\0';

    while (c.read(&charRead, 1) > 0 && charRead != '\r')
        header = header + (char)charRead;

    // Let's skip the next '\r' and '\n'
    c.read(NULL, 1);

    return header;
}

void parseResponse(TCPClient c)
{

    int payloadLength = 0;
    uint8_t payloadbuf[250];

    for (int i = 0; i < 250; i++)
        payloadbuf[i] = 0;

    // Parse headers
    for (;;)
    {
        String header = parseHeader(c);

        if (!header.length())
            break;

        if (header.startsWith(CONTENT_LENGTH_HEADER))
        {
            String value = header.substring(header.indexOf(':') + 1);
            value.trim();

            Log.info(value);

            payloadLength = value.toInt();
        }
    }

    c.read(payloadbuf, payloadLength);

    // `payloadlenght` is taking in account the final \n.
    String payload((const char *)payloadbuf, payloadLength - 1);

    Log.info(payload);

    JSONObjectIterator tasks(JSONValue::parseCopy(payload));

    while (tasks.next())
    {
        JSONArrayIterator tasks_i(tasks.value());

        while (tasks_i.next())
        {
            JSONObjectIterator task_attrs(tasks_i.value());

            while (task_attrs.next())
            {
                if (task_attrs.name() == "name")
                {
                    Log.info("Value = %s", (const char *)task_attrs.value().toString().data());
                    if (strcmp(task_attrs.value().toString().data(), "led_on") == 0)
                    {
                        digitalWrite(A0, HIGH);
                    }
                    else
                    {
                        digitalWrite(A0, LOW);
                    }
                }
            }
        }
    }
}

void loop()
{
    // char headerbuf[50];

    // if (!waitingForResponse && client.connect(BACKEND_URL, BACKEND_PORT))
    // {
    //     Log.info("Connected to: %s", BACKEND_URL);

    //     snprintf(headerbuf, sizeof headerbuf, "GET %s HTTP/1.1", TASKS_ENDPOINT);
    //     client.println(headerbuf);

    //     snprintf(headerbuf, sizeof headerbuf, "Host: %s", BACKEND_URL);
    //     client.println(headerbuf);

    //     client.println("Content-Length: 0");
    //     client.println();

    //     waitingForResponse = true;
    // }

    // if (client.available()) {
    //     Log.info("Parsing response...");
    //     parseResponse(client);
    //     waitingForResponse = false;
    // }

    // if (!client.connected())
    //     client.stop();

    // if (waitingForResponse) return;

    // delay(7000);

    // System.sleep(D1, RISING, 10);

    delay(500);

    Log.info("Awake");

    double acum = 0;

    for (int i = 0; i < 3; i++)
        acum +=  analogRead(A5);

    Log.info("Read %.2f", (acum / 3));
}
