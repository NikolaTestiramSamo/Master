/*
 * ESP8266 NodeMCU UDP Client that can work both in 
 * AP or STA mode of operation
 * 
 * Client detects the press of a button and sends the
 * state of the input digital pin to the server via
 * UDP protocol. It is irrelevant wether the client
 * sets WiFi as AP or STA, as long as the server
 * matches this configuration in an opposite matter.
 * 
 * @version:  1.0
 * @date:     8.2022.
 * @author :  Nikola Cvetkovic
 */
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Set remote WiFi credentials, this is seen from the client side
#define WIFI_SSID "MOTE2BS_ESP8266"
#define WIFI_PASS "MOTE2BS_ESP8266"

// IP address length
#define IP_len  4

// Set the delay for WiFi connection an numer of attempts
#define WIFI_CONN_DELAY_MS 500
#define WIFI_CONN_ATTEMPTS 20
// Set the delay for UDP pin state transmission
#define UDP_TX_DELAY_MS    1000
// Max transmit size
#define UDP_PACKET_SIZE    9
// Set deep sleep time in microseconds and max num of messages
#define DEEP_SLEEP_US      30e6
#define MSG_SENT_MAX       5

// UDP configuration, default gateway's IP address and port
WiFiUDP UDP;
IPAddress gateway_IP;
#define UDP_PORT 4210
 
// UDP Buffer
char rxBuffer[UDP_PACKET_SIZE];
char txBuffer[UDP_PACKET_SIZE];

// Store elapsed time and period for transmitting data
unsigned long previousMillis = 0; 
unsigned const long tx_period = 50;

// Flag for entering deep_sleep
volatile int msg_sent_cnt = 0;

// Prepare a buffer for transmit
void fillBuffer(char *txBufferPtr);

void setup() {
    // Initial delay
    Serial.println();
    Serial.println();
    delay(WIFI_CONN_DELAY_MS);

    // Setup serial port
    Serial.begin(115200);
    Serial.println();
    
    // Begin WiFi connection - STA (Station) mode
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    WiFi.mode(WIFI_STA);
    
    // Connecting to WiFi...
    Serial.print("Connecting to ");
    Serial.print(WIFI_SSID);
    // Loop continuously while WiFi is not connected
    char num_of_tries = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        if (num_of_tries++ <= WIFI_CONN_ATTEMPTS)
        {
            delay(WIFI_CONN_DELAY_MS);
            Serial.print(".");
        }
        else
        {
            Serial.printf("\r\nTaking a %ld-sec nap \r\n", (long int)DEEP_SLEEP_US/(long int)1e6);
            ESP.deepSleep(DEEP_SLEEP_US);
        }
        
    }
    
    // Connected to WiFi
    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());

    // Begin UDP port
    UDP.begin(UDP_PORT);
    Serial.print("Opening UDP port ");
    Serial.println(UDP_PORT);
    
    gateway_IP = WiFi.gatewayIP();
}

/*
 * The client will send the state of the LED which will
 * be stored in the first element of the packetBuffer
 * buffer. If first byte is '0', then LED is pulled 
 * LOW, otherwise it is pulled HIGH.
 * 
 */
void loop() {
    // Parameters of the server connected
    static IPAddress remote_IP;
    static int remote_PORT;
    static int rxSize;
    
    // If there is anything to receive:
    if (UDP.parsePacket())
    {
        // Read the byte:
        rxSize = UDP.read(rxBuffer, UDP_PACKET_SIZE);
        
        // Remember who the sender is:
        remote_IP = UDP.remoteIP();
        remote_PORT = UDP.remotePort();
        
        // Print the info on serial port:
        Serial.printf("I received %d B from BS. Payload: %c\r\n", rxSize, rxBuffer[rxSize - 1]);
    }  

    // Read the current time:
    unsigned long currentMillis = millis();
    
    // If more than tx_interval has elapsed, send pin state:
    if (currentMillis - previousMillis >= tx_period) 
    {
        // Remember last time code has executed this branch:
        previousMillis = millis();    

        // Increment number of messages sent
        msg_sent_cnt++;
        
        // Prepare a buffer to send
        fillBuffer(txBuffer);
        
        // Send to the default gateway, can be other node or a base station
        UDP.beginPacket(gateway_IP, UDP_PORT);
        UDP.write(txBuffer, UDP_PACKET_SIZE);
        UDP.endPacket();
        yield();
        
        // Print the info on serial port:
        Serial.printf("I am sending %d bytes to the server.\r\n", UDP_PACKET_SIZE);
    }
    
    // If maximum allowed number of messages for this session is reached, enter sleep
    if (msg_sent_cnt == MSG_SENT_MAX)
    {
        Serial.printf("Taking a %ld-second nap \r\n", (long int)DEEP_SLEEP_US/(long int)1e6);
        ESP.deepSleep(DEEP_SLEEP_US); 
    }
}

// Prepare a buffer for transmit
void fillBuffer(char *txBufferPtr)
{
    // Send IP address of the destination, i.e. gateway address
    for (int i = IP_len - 1; i >= 0; i--)
    {
        *(txBufferPtr + i) = gateway_IP[i];
        *(txBufferPtr + i + IP_len) = WiFi.localIP()[i];
//        *(txBufferPtr + i) = txMsg.dstIP[i];
//        *(txBufferPtr + i + IP_len) = txMsg.srcIP[i];
    }
//    for (i = 0; i < UDP_PACKET_SIZE - 2*IP_len; i++)
//    {
//        *(txBufferPtr + i + 2*IP_len) = payload[i];
//    }
    // Dummy payload 'n' for testing
    *(txBufferPtr + 2*IP_len) = 110;
}
