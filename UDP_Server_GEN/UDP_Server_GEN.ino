/*
 * ESP8266 NodeMCU UDP Server that can work both in 
 * AP or STA mode of operation
 * 
 * Server listens to the incoming messages to read
 * the digital pin of the client. According to the
 * state received (HIGH or LOW), the server
 * changes the state of an onboard LED.
 * 
 * If there is nothing to receive, the server
 * sends some byte value to the client.
 * 
 * @version:  1.0
 * @date:     8.2022.
 * @author :  Nikola Cvetkovic
 */
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Set own WiFi AP credentials, server is visible as remote to clients
#define AP_SSID "BS_ESP8266"
#define AP_PASS "BS_ESP8266"

// IP address length
#define IP_len  4

// Set the delay for WiFi connection to 500ms
#define WIFI_CONN_DELAY_MS 500
// Set the delay for UDP pin state transmission
#define UDP_TX_DELAY_MS    1000
// Max transmit size
#define UDP_PACKET_SIZE    9

// UDP configuration, default gateway's IP address and port
WiFiUDP UDP;
#define UDP_PORT 4210

// For testing only:
IPAddress client_IP (192,168,10,100);
IPAddress own_IP (192,168,4,1);
IPAddress gateway_IP (192,168,4,1);
IPAddress subnet_IP (255,255,255,0);

// UDP Buffer
char rxBuffer[UDP_PACKET_SIZE];
char txBuffer[UDP_PACKET_SIZE];

// Store elapsed time and period for transmitting data
unsigned long previousMillis = 0; 
unsigned const long tx_period = 5000;

// On-board SWITCH can be connected to pin 2
// since this is SDA for I2C which is not used:
const int ledPinUDP = 2;
// Value to return to the client
char returnVal = 0;

// Prepare a buffer for transmit
void fillBuffer(char *txBufferPtr);

void actionUponRx()
{
    if (rxBuffer[IP_len*2 + 1]){
        digitalWrite(ledPinUDP, HIGH);
    } else {
        digitalWrite(ledPinUDP, LOW);
    }
    return;
}

void actionUponTx()
{
    for (int i = 3; i >= 0; i--)
      txBuffer[i] = own_IP[i];
    return;
}
void setup() {

    // Initial delay
    Serial.println();
    Serial.println();
    delay(WIFI_CONN_DELAY_MS);
    
    // LED pin configuration:
    pinMode(ledPinUDP, OUTPUT);
    digitalWrite(ledPinUDP, HIGH);

    // Setup serial port
    Serial.begin(115200);
    Serial.println();

    // Initialize own Access Point
    Serial.println("Starting access point...");
    WiFi.softAPConfig(own_IP, gateway_IP, subnet_IP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("Own access point IP adress is: ");
    Serial.println(WiFi.softAPIP());

    // Start listening to UDP port
    UDP.begin(UDP_PORT);
    Serial.print("Listening on UDP port ");
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
    static char sender_known;
    
    // If there is anything to receive:
    if (UDP.parsePacket())
    {
        // Read the byte:
        rxSize = UDP.read(rxBuffer, UDP_PACKET_SIZE);
        // Remember who the sender is:
        remote_IP = UDP.remoteIP();
        remote_PORT = UDP.remotePort();
        // If the packet is received, sender's IP address is known:
        sender_known = 1;
        // Print the info on serial port:
        Serial.printf("I received %d B from node %d.%d. Payload: %c\r\n", rxSize, rxBuffer[6], rxBuffer[7], rxBuffer[rxSize - 1]);
//        Serial.printf("rxMsg.dst is: %d.%d.%d.%d \r\n", rxBuffer[0], rxBuffer[1],rxBuffer[2], rxBuffer[3]);
//        Serial.printf("rxMsg.src is: %d.%d.%d.%d \r\n", rxBuffer[4], rxBuffer[5],rxBuffer[6], rxBuffer[7]);
        if (sender_known)
        {
            // Prepare a buffer to send
            fillBuffer(txBuffer);
            // Send the button state back to the IP obtained:
            UDP.beginPacket(remote_IP, remote_PORT);
            UDP.write(txBuffer, UDP_PACKET_SIZE);
            UDP.endPacket();
            yield();
            // Print the info on serial port:
            Serial.printf("I am sending %d bytes to the node %d.%d.\r\n", UDP_PACKET_SIZE, txBuffer[2], txBuffer[3]);
//            Serial.printf("txMsg.dst is: %d.%d.%d.%d \r\n", txBuffer[0], txBuffer[1],txBuffer[2], txBuffer[3]);
//            Serial.printf("txMsg.src is: %d.%d.%d.%d \r\n", txBuffer[4], txBuffer[5],txBuffer[6], txBuffer[7]);
        }
        else
        {
            // If the sender is not known, program waits for the message
            Serial.printf("I still don't know who is attached to me. \r\n");
        }
    }  

//    // Read current time:
//    unsigned long currentMillis = millis();
//    
//    // If more than tx_interval has elapsed, send pin state:
//    if (currentMillis - previousMillis >= tx_period) 
//    {
//        // Remember last time code has executed this branch:
//        previousMillis = millis();
//            
//        if (sender_known)
//        {
//            // Prepare a buffer to send
//            fillBuffer(txBuffer);
//            // Send the button state back to the IP obtained:
//            UDP.beginPacket(remote_IP, remote_PORT);
//            UDP.write(txBuffer, UDP_PACKET_SIZE);
//            UDP.endPacket();
//            // Print the info on serial port:
//            Serial.printf("I am sending %d bytes to the node %d.%d.\r\n", UDP_PACKET_SIZE, txBuffer[2], txBuffer[3]);
////            Serial.printf("txMsg.dst is: %d.%d.%d.%d \r\n", txBuffer[0], txBuffer[1],txBuffer[2], txBuffer[3]);
////            Serial.printf("txMsg.src is: %d.%d.%d.%d \r\n", txBuffer[4], txBuffer[5],txBuffer[6], txBuffer[7]);
//        }
//        else
//        {
//            // If the sender is not known, program waits for the message
//            Serial.printf("I still don't know who is attached to me. \r\n");
//        }
//    }
}

// Prepare a buffer for transmit
void fillBuffer(char *txBufferPtr)
{
    // Send IP address of the destination, i.e. gateway address
    for (int i = IP_len - 1; i >= 0; i--)
    {
        *(txBufferPtr + i) = rxBuffer[i + IP_len];
        *(txBufferPtr + i + IP_len) = WiFi.softAPIP()[i];        
//        *(txBufferPtr + i) = txMsg.dstIP[i];
//        *(txBufferPtr + i + IP_len) = txMsg.srcIP[i];
    }
//    for (i = 0; i < UDP_PACKET_SIZE - 2*IP_len; i++)
//    {
//        *(txBufferPtr + i + 2*IP_len) = payload[i];
//    }
    // Dummy payload for testing
    *(txBufferPtr + 2*IP_len) = 66;
}
