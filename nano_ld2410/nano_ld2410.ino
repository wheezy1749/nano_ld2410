#include <Arduino.h>
#include <ld2410.h>
#include "network.hpp"
#include <Watchdog.h>

ld2410 radar;
bool engineeringMode = false;
String command;
int update_interval = 5;
int state = 0;

Watchdog watchdog;

void print(String msg) {
    Serial.println(msg);
    send_udp(msg);
}

void setup(void)
{
    udp.begin(udpPort);
    connect_to_wifi();

    Serial.begin(115200); // Feedback over Serial Monitor
    delay(500);           // Give a while for Serial Monitor to wake up
    //radar.debug(Serial); //Uncomment to show debug information from the library on the Serial Monitor. By default this does not show sensor reads as they are very frequent.
    Serial1.begin(256000); // UART for monitoring the radar
    delay(500);
    Serial.print(F("\nLD2410 radar sensor initialising: "));
    if (radar.begin(Serial1))
    {
        Serial.println(F("OK"));
    }
    else
    {
        Serial.println(F("not connected"));
    }

    watchdog.enable(Watchdog::TIMEOUT_8S);
}

unsigned long t3 = millis();

bool send_on_t(String msg, unsigned long t)
{
    t *= 1000;
    if (millis() - t3 > t)
    {
        t3 = millis();
        print(msg);
        return true;
    }
    return false;
}

void handle_server_request(String command)
{
    command.trim();

    if (command.equals("read"))
    {
        command = "";
        print(F("Reading from sensor: "));
        print(F("OK"));
        if (radar.presenceDetected())
        {
            if (radar.stationaryTargetDetected())
            {
                print("Stationary target: " + String(radar.stationaryTargetDistance()) + " cm");
                print("Stationary energy: " + String(radar.stationaryTargetEnergy()));
            }
            if (radar.movingTargetDetected())
            {
                print("Moving target: " + String(radar.movingTargetDistance()) + " cm");
                print("Moving energy: " + String(radar.movingTargetEnergy()));
            }
        }
        else
        {
            print("nothing detected");
        }
    }
    else if (command.equals("readconfig"))
    {
        command = "";
        print(F("Reading configuration from sensor: "));
        if (radar.requestCurrentConfiguration())
        {
            print(F("OK"));
            print("Maximum gate ID: " + String(radar.max_gate));
            print("Maximum gate for moving targets: " + String(radar.max_moving_gate));
            print("Maximum gate for stationary targets: " + String(radar.max_stationary_gate));
            print("Idle time for targets: " + String(radar.sensor_idle_time));
            print(F("Gate sensitivity"));
            for (uint8_t gate = 0; gate <= radar.max_gate; gate++)
            {
                print("Gate " + String(gate));
                print("Moving targets: " + String(radar.motion_sensitivity[gate]));
                print("Stationary targets: " + String(radar.stationary_sensitivity[gate]));
            }
        }
        else
        {
            print(F("Failed"));
        }
    }
    else if (command.startsWith("setmaxvalues"))
    {
        uint8_t firstSpace = command.indexOf(' ');
        uint8_t secondSpace = command.indexOf(' ', firstSpace + 1);
        uint8_t thirdSpace = command.indexOf(' ', secondSpace + 1);
        uint8_t newMovingMaxDistance = (command.substring(firstSpace, secondSpace)).toInt();
        uint8_t newStationaryMaxDistance = (command.substring(secondSpace, thirdSpace)).toInt();
        uint16_t inactivityTimer = (command.substring(thirdSpace, command.length())).toInt();

        if (newMovingMaxDistance > 0 && newStationaryMaxDistance > 0 && newMovingMaxDistance <= 8 && newStationaryMaxDistance <= 8)
        {
            print("Setting max values to gate " + String(newMovingMaxDistance)
            + " moving targets, gate " + String(newStationaryMaxDistance)
            + " stationary targets, " + String(inactivityTimer)
            + "s inactivity timer: ");

            command = "";
            if (radar.setMaxValues(newMovingMaxDistance, newStationaryMaxDistance, inactivityTimer))
            {
                print(F("OK, now restart to apply settings"));
            }
            else
            {
                print(F("failed"));
            }
        }
        else
        {
            print("Can't set distances to " + String(newMovingMaxDistance)
            + " moving " + String(newStationaryMaxDistance)
            + " stationary, try again");
            command = "";
        }
    }
    else if (command.startsWith("setsensitivity"))
    {
        uint8_t firstSpace = command.indexOf(' ');
        uint8_t secondSpace = command.indexOf(' ', firstSpace + 1);
        uint8_t thirdSpace = command.indexOf(' ', secondSpace + 1);
        uint8_t gate = (command.substring(firstSpace, secondSpace)).toInt();
        uint8_t motionSensitivity = (command.substring(secondSpace, thirdSpace)).toInt();
        uint8_t stationarySensitivity = (command.substring(thirdSpace, command.length())).toInt();
        if (motionSensitivity >= 0 && stationarySensitivity >= 0 && motionSensitivity <= 100 && stationarySensitivity <= 100)
        {
            print("Setting gate " + String(gate)
            + " motion sensitivity to " + String(motionSensitivity)
            + " & stationary sensitivity to " + String(stationarySensitivity)
            + F(": "));
            command = "";
            if (radar.setGateSensitivityThreshold(gate, motionSensitivity, stationarySensitivity))
            {
                print(F("OK, now restart to apply settings"));
            }
            else
            {
                print(F("failed"));
            }
        }
        else
        {
            print("Can't set gate " + String(gate)
            + " motion sensitivity to " + String(motionSensitivity)
            + " & stationary sensitivity to " + String(stationarySensitivity)
            + F(", try again"));
            command = "";
        }
    }
    else if (command.equals("restart"))
    {
        command = "";
        print(F("Restarting sensor: "));
        if (radar.requestRestart())
        {
            print(F("OK"));
        }
        else
        {
            print(F("failed"));
        }
    }
    else if (command.equals("factoryreset"))
    {
        command = "";
        print(F("Factory resetting sensor: "));
        if (radar.requestFactoryReset())
        {
            print(F("OK, now restart sensor to take effect"));
        }
        else
        {
            print(F("failed"));
        }
    }
    else
    {
        Serial.print(F("Unknown command: "));
        Serial.println(command);
        command = "";
    }
}

void loop()
{
    watchdog.reset();
    radar.read();

    char server_msg[255];
    if (get_udp(&server_msg[0])) {
        Serial.print("Msg: ");
        Serial.println(server_msg);
        handle_server_request(server_msg);
    }

    int detected = analogRead(A0) > 500;
    if (radar.presenceDetected())
    {
        if (state == 0)
        {
            state = 1;
            send_udp("MOTION_DETECTED, detect = " + String(detected));
            handle_server_request("read");
        }
        else
        {
            if (send_on_t("MOTION_DETECTED, detected = " + String(detected) , update_interval)) {
                handle_server_request("read");
            }
        }
    }
    else
    {
        if (state == 1)
        {
            state = 0;
            print("MOTIONLESS");
        }
        else
        {
            send_on_t("MOTIONLESS", update_interval);
        }
    }
}