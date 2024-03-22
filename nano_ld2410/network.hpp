#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <secrets.h>

void connect_to_wifi()
{
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE)
    {
        Serial.println("Communication with WiFi module failed!");
        while (true)
            ;
    }

    // attempt to connect to WiFi network:
    do
    {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(SSID);
        delay(1000);
    } while (WiFi.begin(SSID, PASSWORD) != WL_CONNECTED);
    delay(1000);
    Serial.println("Connected to WiFi");
}

const char *udpAddress = "192.168.50.39";
const int udpPort = 2390;
WiFiUDP udp;

void send_udp(String msg)
{
    udp.beginPacket(udpAddress, udpPort);
    udp.write(msg.c_str(), strlen(msg.c_str()));
    udp.write("\n");
    udp.endPacket();
}

void send_udp(const char *msg)
{
    udp.beginPacket(udpAddress, udpPort);
    udp.write(msg, strlen(msg));
    udp.write("\n");
    udp.endPacket();
}

void send_udp(String msg, const char *addr, int port)
{
    udp.beginPacket(addr, port);
    udp.write(msg.c_str(), strlen(msg.c_str()));
    udp.write("\n");
    udp.endPacket();
}

void send_udp(const char *msg, const char *addr, int port)
{
    udp.beginPacket(addr, port);
    udp.write(msg, strlen(msg));
    udp.write("\n");
    udp.endPacket();
}

int get_udp(char * msg)
{
    int packetSize = udp.parsePacket();
    if (packetSize) {
        Serial.print("Received ");
        Serial.print(packetSize);
        Serial.print(" bytes from ");
        Serial.print(udp.remoteIP());
        Serial.print(" port ");
        Serial.println(udp.remotePort());

        int len = udp.read(msg, 255);
        if (len > 0) {
            msg[len] = '\0';
        }
    }

    return packetSize;
}