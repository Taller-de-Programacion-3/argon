#include <stdio.h>

// Prod.
// const char* BACKEND_URL = "taller3-backend.herokuapp.com";
// const int BACKEND_PORT = 80;

// Dev.
const char *BACKEND_URL = "192.168.0.189";
const int BACKEND_PORT = 5000;

const char *NETWORK = "Fibertel WiFi881 2.4GHz";
const char *NETWORK_PASS = "00412403610";

const char *TASKS_ENDPOINT = "/devices/tasks/argon";
const char *CONTENT_LENGTH_HEADER = "Content-Length";

const char *LED_ON_TASK = "led_on";
const char *LED_OFF_TASK = "led_off";

const int PAYLOAD_BUF_SIZE = 2048;

struct Result
{
    String taskID;
    String value;
};

SerialLogHandler logHandler;
TCPClient client;
std::vector<Result> pendingResults;
bool connected = false;
bool firstCompleted = false;

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

    Log.info(p);

    JSONObjectIterator tasks(JSONValue::parseCopy(p));

    while (tasks.next())
    {
        JSONArrayIterator tasks_i(tasks.value());

        while (tasks_i.next())
        {
            JSONObjectIterator task_attrs(tasks_i.value());

            String id, task_name;

            while (task_attrs.next())
            {
                // Regular attrs.
                if (task_attrs.name() == "id")
                    id = String(task_attrs.value().toString());

                if (task_attrs.name() == "task_name")
                    task_name = String(task_attrs.value().toString());

                // Extra params.
                if (task_attrs.name() == "task_params")
                {
                    JSONObjectIterator task_extra_params(task_attrs.value());

                    while (task_extra_params.next())
                    {
                        Log.info(
                            "Extra param: " +
                            String(task_extra_params.name()) +
                            String(task_extra_params.value().toString()));
                    }
                }
            }

            if (task_name == LED_ON_TASK)
            {
                Log.info("Turning ON led");
                digitalWrite(A0, HIGH);
                pendingResults.push_back(Result{taskID : id, value : "ok"});
            }
            else if (task_name == LED_OFF_TASK)
            {
                Log.info("Turning OFF led");
                digitalWrite(A0, LOW);
                pendingResults.push_back(Result{taskID : id, value : "ok"});
            }
            else
            {
                Log.info("Unknown task, skipping it");
                pendingResults.push_back(Result{taskID : id, value : "ok"});
            }
        }
    }
}

void processTasksResponse(TCPClient c)
{
    int payloadLength = 0;

    // Parse headers
    for (;;)
    {
        String h = nextHeader(c);

        Log.info(h);

        // If we reached end of headers just break.
        if (!h.length())
            break;

        if (h.startsWith(CONTENT_LENGTH_HEADER))
        {
            String value = h.substring(h.indexOf(':') + 1);

            value.trim();

            // `payloadlength` is taking in account the final '\n'.
            payloadLength = value.toInt();
        }
    }

    uint8_t payloadbuf[PAYLOAD_BUF_SIZE];

    // Initialize the array.
    for (int i = 0; i < PAYLOAD_BUF_SIZE; i++)
        payloadbuf[i] = 0;

    Log.info("Reading %d bytes", payloadLength);

    int bytesRead = 0;
    while ((bytesRead += c.read(&payloadbuf[bytesRead], payloadLength)) < payloadLength)
        ;

    payloadbuf[payloadLength] = '\0';

    String payload((const char *)payloadbuf);

    processPayload(payload);
}

String buildRequest(String method, String url, String path, String payload)
{
    String r;

    r = r + method + String(" ") + String(path) + String(" HTTP/1.1\r\n");
    r = r + String("Host: ") + String(url) + String("\r\n");
    r = r + String("Content-Length: ") + payload.length() + String("\r\n");
    r = r + (method == String("POST")? String("Content-Type: application/json\r\n") : String());

    r = r + String("\r\n");
    r = r + payload;

    return r;
}

String buildGET(String url, String path)
{
    return buildRequest(String("GET"), url, path, String());
}

String buildPOST(String url, String path, String payload)
{
    return buildRequest(String("POST"), url, path, payload);
}

String buildResultResponse()
{
    char buf[2048];
    JSONBufferWriter writer(buf, sizeof(buf));

    writer.beginArray();

    for (Result r : pendingResults)
    {
        writer.beginObject();
        writer.name("id").value(r.taskID);
        writer.name("value").value(r.value);

        writer.endObject();
    }

    writer.endArray();

    writer.buffer()[std::min(writer.bufferSize(), writer.dataSize())] = 0;

    pendingResults.clear();

    String result(buf);

    return buildPOST(
        String(BACKEND_URL),
        String(TASKS_ENDPOINT),
        result);
}

double measure()
{
    double acum = 0;

    for (int i = 0; i < 3; i++)
        acum += analogRead(A5);

    Log.info("Read %.2f", (acum / 3));

    return acum;
}

void askForTasks() 
{
    if (!connected && client.connect(BACKEND_URL, BACKEND_PORT))
    {
        connected = true;
        String tasksRequest = buildGET(String(BACKEND_URL), String(TASKS_ENDPOINT));

        Log.info(tasksRequest);

        client.println(tasksRequest.c_str());
    }

    if (connected && client.available())
    {

        Log.info("Connected");

        processTasksResponse(client);
    }

    if (connected && !client.connected())
    {
        Log.info("Disconnecting");

        client.stop();
        connected = false;
        firstCompleted = true;
    }
}

void sendTaskResults() 
{ 
    if (!pendingResults.size()) {
        firstCompleted = false;
        return;
    }

    // New connection to perform the POST.
    if (!connected && client.connect(BACKEND_URL, BACKEND_PORT))
    {
        connected = true;

        Log.info("pending results %d", pendingResults.size());

        String response = buildResultResponse();

        Log.info("sending " + response);

        client.println(response.c_str());
    }
    
    if (!connected) return;

    if (client.available())
    {
        while (-1 != client.read())
            ;
    }

    if (!client.connected())
    {
        Log.info("Disconnecting");

        client.stop();
        connected = false;
        firstCompleted = false;
    }
}

void loop()
{
    (!firstCompleted)?  askForTasks() : sendTaskResults();

    // System.sleep(D1, RISING, 10);

    delay(500);
}
