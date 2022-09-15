/*
 * ESP8266 NodeMCU UDP Aggregator that can works both in 
 * AP and STA mode at the same time
 * 
 * Aggregator listens to the incoming messages and
 * passes them to the receiver. Two-way traffic is
 * provided.
 * 
 * @version:  1.0
 * @date:     8.2022.
 * @author :  Nikola Cvetkovic
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Set remote WiFi credentials, this is seen from the client side
const char* ap_ssid = "MOTE_ESP8266";
const char* ap_pass = "MOTE_ESP8266";

// SSIDs to look for when creating a link to the base statiton:
#define BS_SSID "BS_ESP8266"
#define BS_PASS "BS_ESP8266"
#define MOTE2BS_SSID "MOTE2BS_ESP8266"
#define MOTE2BS_PASS "MOTE2BS_ESP8266"

// IP address length
#define IP_len  4
// Set the delay for WiFi connection to 500ms
#define WIFI_CONN_DELAY_MS 500
// Set the delay for UDP pin state transmission
#define UDP_TX_DELAY_MS    100
// Custom UDP tx and rx buffer size
#define UDP_PACKET_SIZE    9
// Set deep sleep time in microseconds and max num of messages
#define DEEP_SLEEP_US      30e6
#define MSG_SENT_MAX       5

// UDP configuration, default gateway's IP address and port
WiFiUDP UDP;
#define UDP_PORT 4210

// For testing only:
IPAddress client_IP;
IPAddress own_IP;
IPAddress sub_gateway_IP;
IPAddress sup_gateway_IP;
IPAddress sub_broadcast_IP;
IPAddress sub_network_IP;
IPAddress sup_network_IP;
IPAddress sendernet_IP;
IPAddress dstnet_IP;
IPAddress subnetmask_IP (255,255,255,0);
 
// UDP Buffers and sizes
char rxBuffer[UDP_PACKET_SIZE];
int rxSize;
char txBuffer[UDP_PACKET_SIZE];
int txSize;

// Flag which indicates that base station is found:
bool connectToBS = false;

// Message structure according to the protocol
typedef struct UdpMsg{
    IPAddress dstIP;
    IPAddress srcIP;
    char payload[UDP_PACKET_SIZE - 2*IP_len];
}UdpMsg;

// Rx and tx message structs
UdpMsg rxMsg;
UdpMsg txMsg;

// Store elapsed time and period for transmitting data
unsigned long previousMillis = 0; 
unsigned const long tx_period = 9000;

// Flag for entering deep_sleep
volatile int msg_sent_cnt = 0;

// Function preforms extraction data from received message
void parseMsg(char *rxBufferPtr);

// Prepare a buffer for transmit
void fillBuffer(char *txBufferPtr);

// Connect to base station's or another node's AP:
void createLink();

// Only for testing:
void actionUponRx();

void setup() {

    // Initial delay
    Serial.println();
    Serial.println();
    delay(WIFI_CONN_DELAY_MS);
    

    // Setup serial port
    Serial.begin(115200);
    Serial.println();
    
    // Determine wether AP should be opened 
//    runLakicevAlgorithm();
//    if (shouldOpenAP)
//        // Begin WiFi connection - STA (Station) mode 
//        WiFi.mode(WIFI_STA);
//    else
//        // Begin WiFi connection - STA and AP mode
        WiFi.mode(WIFI_AP_STA);
        
    // Scan for available networks:
    char n = WiFi.scanNetworks();
    Serial.printf("Found %d networks. \n\r", n);
    
    // Try to find base station
    for (int i = n - 1; i >= 0; i--)
    {
        // If base station is visible, connect to it:
        if (WiFi.SSID(i) == BS_SSID)
        {
            connectToBS  = true;
            createLink(&connectToBS, 0);
            break;
        }
    }
    
    // Try to find path to base station through other nodes
    if (!connectToBS)
    {
        int max_rssi_idx = -1;
        long int max_rssi = -1000;
        for (int i = n - 1; i >= 0; i--)
        {
            // If other MOTE2BS is visible, then connect to strongest:
            if (WiFi.SSID(i) == MOTE2BS_SSID)
            {
                if(WiFi.RSSI(i) > max_rssi)
                {
                    max_rssi = WiFi.RSSI(i);
                    max_rssi_idx = i;                   
                }
                connectToBS = false;
            }
        }
        if (max_rssi_idx >= 0)
        {
            createLink(&connectToBS, max_rssi_idx);
        }
        else
        {
            Serial.printf("Didn't find BS nor MOTE2BS to connect to, going to sleep\r\n");  
            ESP.deepSleep(DEEP_SLEEP_US); 
        }
        
    }        

//    if (shouldOpenAP){
        // Initialize own Access Point
        Serial.println("Starting access point...");
        WiFi.softAPConfig(own_IP, sub_gateway_IP, subnetmask_IP);
        WiFi.softAP(ap_ssid, ap_pass);
        Serial.print("Own access point IP adress is: ");
        Serial.println(WiFi.softAPIP());
        
        sub_broadcast_IP = WiFi.softAPIP();
        sub_broadcast_IP[IP_len - 1] = 255;
        
        // Start listening to UDP port
        UDP.begin(UDP_PORT);
        Serial.print("Listening on UDP port ");
        Serial.println(UDP_PORT);
//    }

    for(int i = IP_len - 1; i >= 0; i--)
    {    
        sub_network_IP[i] = WiFi.softAPIP()[i] & subnetmask_IP[i];
        sup_network_IP[i] = WiFi.localIP()[i] & subnetmask_IP[i];
    }
    Serial.printf("Sub network mask is: ");
    Serial.println(sub_network_IP);
    Serial.printf("Super network mask is: ");
    Serial.println(sup_network_IP);
}

/*
 * The client will send the state of the LED which will
 * be stored in the first element of the rxBuffer
 * buffer. If first byte is '0', then LED is pulled 
 * LOW, otherwise it is pulled HIGH.
 * 
 */
void loop() {
    // Parameters of the server connected
    static IPAddress remote_IP;
    static int remote_PORT;
    
    // If there is anything to receive:
    if (UDP.parsePacket())
    {
        // Read the byte:
        rxSize = UDP.read(rxBuffer, UDP_PACKET_SIZE);
        
        // Extract necessary data:
        parseMsg(rxBuffer);
        remote_PORT = UDP.remotePort();
        
        // Find the network of the sender and destination
        sendernet_IP = UDP.remoteIP();
        sendernet_IP[IP_len - 1] = 0;
        dstnet_IP = rxMsg.dstIP;
        dstnet_IP[IP_len - 1] = 0;

        
        // If the message is addressed to this node from the subnetwork
        if (sendernet_IP == sub_network_IP)
        {   
            Serial.printf("Node %d.%d sent a message, sending to BS.\r\n", rxMsg.srcIP[IP_len - 2], rxMsg.srcIP[IP_len - 1]);
            UDP.beginPacket(sup_gateway_IP, remote_PORT);
            UDP.write(rxBuffer, rxSize);
            UDP.endPacket();
            yield();
        }
        // If the message is addressed to this node from the supernetwork
        else if (rxMsg.dstIP == WiFi.localIP())
        {
            actionUponRx();
        }
        // If the message is addressed to the node in the subnetwork
        else if (dstnet_IP == sub_network_IP)
        {
            Serial.printf("I received %d B from a BS, sending to node.\r\n", rxSize);
            UDP.beginPacket(rxMsg.dstIP, remote_PORT);
            UDP.write(rxBuffer, rxSize);
            UDP.endPacket();
            yield();
        }
        // If the message is addressed to a node in some other network
        else
        {
            Serial.printf("I don't recognize IP address of the destination, I'll broadcast!\n\r");
            UDP.beginPacket(sub_broadcast_IP, remote_PORT); 
            UDP.write(rxBuffer, rxSize);
            UDP.endPacket(); 
            yield();
        }      
        //delay(UDP_TX_DELAY_MS);
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
        UDP.beginPacket(sup_gateway_IP, UDP_PORT);
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

// Function preforms extraction of src and dst IP address and payload
void parseMsg(char *rxBufferPtr)
{
    int i = 0;
    for (i = IP_len - 1; i >= 0; i--)
    {
        rxMsg.dstIP[i] = *(rxBufferPtr + i);
        rxMsg.srcIP[i] = *(rxBufferPtr + i + IP_len);
    }
    for (i = 0; i < UDP_PACKET_SIZE - 2*IP_len; i++)
    {
        rxMsg.payload[i] = *(rxBufferPtr + i + 2*IP_len);
    }
}

// Prepare a buffer for transmit
void fillBuffer(char *txBufferPtr)
{
    // Send IP address of the destination, i.e. gateway address
    for (int i = IP_len - 1; i >= 0; i--)
    {
        *(txBufferPtr + i) = sup_gateway_IP[i];
        *(txBufferPtr + i + IP_len) = WiFi.localIP()[i];
//        *(txBufferPtr + i) = txMsg.dstIP[i];
//        *(txBufferPtr + i + IP_len) = txMsg.srcIP[i];
    }
//    for (i = 0; i < UDP_PACKET_SIZE - 2*IP_len; i++)
//    {
//        *(txBufferPtr + i + 2*IP_len) = payload[i];
//    }
    // Dummy payload 'A' for testing
    *(txBufferPtr + 2*IP_len) = 65;
}

void createLink(const bool *connectToBS, char network_idx)
{   
    // This node will be MOTE2BS AP if the algorithm says so:
    ap_ssid = "MOTE2BS_ESP8266";
    ap_pass = "MOTE2BS_ESP8266";
    
    WiFi.disconnect();
    WiFi.persistent(false);
    
    // Connect to base station's AP:
    if (*connectToBS)
    {
        WiFi.begin(BS_SSID, BS_PASS);
        // Connecting to WiFi...
        Serial.print("Connecting to ");
        Serial.print(BS_SSID);
    }
    // Connect to mote which has a link to BS:
    else
    {   
        WiFi.begin(WiFi.SSID(network_idx), MOTE2BS_PASS);
        // Connecting to WiFi...
        Serial.print("Connecting to ");
        Serial.print(MOTE2BS_SSID);
    }
    // Loop continuously while WiFi is not connected
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(WIFI_CONN_DELAY_MS);
        Serial.print(".");
    }
    sup_gateway_IP = WiFi.gatewayIP();
    // Connected to WiFi
    Serial.println();
    Serial.print("Connected! IP address obtained: ");
    Serial.println(WiFi.localIP());

    // Extract the data for own AP:
    own_IP = WiFi.localIP();
    // This is only for small-scale networks, custom algorithm:
    own_IP[2] += WiFi.localIP()[3];
    own_IP[3] = 1;
    sub_gateway_IP = own_IP;
}

// Action upon receiving a message from the base station:
void actionUponRx()
{
    // Print the info on serial port:
    Serial.printf("I received %d B from BS. Payload: %c\r\n", rxSize, rxBuffer[rxSize - 1]);
    return;
}
