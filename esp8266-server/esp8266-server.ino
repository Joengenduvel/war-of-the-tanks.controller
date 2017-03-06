#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>

ESP8266WiFiMulti WiFiMulti;

WebSocketsServer webSocket = WebSocketsServer(81);
WiFiServer server(80);


#define USE_SERIAL Serial
#define PIN_LEFT_DIRECTION1  5
#define PIN_LEFT_DIRECTION2  4
#define PIN_LEFT_ENABLE 2


#define PIN_RIGHT_DIRECTION1  12
#define PIN_RIGHT_DIRECTION2  13
#define PIN_RIGHT_ENABLE  14

void setup() {
  pinMode(PIN_LEFT_DIRECTION1, OUTPUT);
  pinMode(PIN_LEFT_DIRECTION2, OUTPUT);
  pinMode(PIN_LEFT_ENABLE, OUTPUT);


  pinMode(PIN_RIGHT_DIRECTION1, OUTPUT);
  pinMode(PIN_RIGHT_DIRECTION2, OUTPUT);
  pinMode(PIN_RIGHT_ENABLE, OUTPUT);

  USE_SERIAL.begin(115200);

  USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  WiFiMulti.addAP("JDguest", "Y0urSec0ndH0me!");

  while (WiFiMulti.run() != WL_CONNECTED) {
    USE_SERIAL.println("waiting for connection");
    delay(100);
  }
  USE_SERIAL.println("Connected");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  server.begin();
}

void loop() {
  webSocket.loop();
  //check for available clients:
  WiFiClient client = server.available();
  if (!client) {
    return;
  } else {
    sendWebPage(client);
    client.flush();
    client.stop();
  }
}

void sendWebPage(WiFiClient client) {
  USE_SERIAL.print("sending page to: ");
  USE_SERIAL.println(client.localIP());

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); // do not forget this one
  client.print("<!DOCTYPE html><html><script type=\"text/javascript\">document.addEventListener('touchstart', process_touchstart, false);var ws=new WebSocket(\"ws://");
  client.print(WiFi.localIP());
  client.println(":81\");function process_touchstart(ev){switch (ev.touches.length){case 1: enable_movement(ev); break; case 2: handle_two_touches(ev); break; case 3: handle_three_touches(ev); break; default: gesture_not_supported(ev); break;}}function enable_movement(ev){document.addEventListener('touchmove', send_positions, false);document.addEventListener('touchend', ws.send(\"0/0\"), false);}function send_positions(ev){var resultPane=document.getElementById(\"result\"); var touchobj=ev.changedTouches[0];var halfWidth=window.innerWidth/2;var halfHeight=window.innerHeight/2;var travelX=parseInt(((touchobj.clientX - halfWidth)/halfWidth)*100);var travelY=parseInt(((halfHeight - touchobj.clientY)/halfHeight)*100);resultPane.textContent=travelX +' - ' + travelY;ws.send(travelX + '/' + travelY);}</script><body><div style=\"position:absolute;top:50%; left:50%;transform: translateY(-50%);transform: translateX(-50%);\"><p>Touch Me!</p><p id=\"result\"></p></div></body></html>");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

  switch (type) {
    case WStype_DISCONNECTED:
      USE_SERIAL.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocket.sendTXT(num, "Connected");
      }
      break;
    case WStype_TEXT: {
        USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);
        String x = String(strtok((char *)payload, "/"));
        String y = String(strtok(NULL, "/"));
        //determine direction
        bool left = x.startsWith("-");
        if (left) {
          x = String(x.substring(1));
        }
        bool reverse = y.startsWith("-");
        if (reverse) {
          y = String(y.substring(1));
        }
        int absX = limit(x.toInt());
        int absY = limit(y.toInt());
        if (left) {
          absX *= -1;
        }
        if (reverse) {
          absY *= -1;
        }
        //initial speeds
        int leftInitialSpeed = absY - absX;
        int rightInitialSpeed = absY + absX;
        //scale proportional to 100%
        float scaleFactor = 1;
        if (!(leftInitialSpeed == 0 || rightInitialSpeed == 0)) {
          int absLeft = abs(leftInitialSpeed);
          int absRight = abs(rightInitialSpeed);
          if (absLeft > 100 || absRight > 100) {
            if (absLeft > absRight) {
              scaleFactor = 100.0 / absLeft;
            } else {
              scaleFactor = 100.0 / absRight;
            }
          }
        }
        USE_SERIAL.println("initial speed");
        USE_SERIAL.print(leftInitialSpeed);
        USE_SERIAL.print(", ");
        USE_SERIAL.println(rightInitialSpeed);
        int leftSpeed = calculateSpeed(abs(leftInitialSpeed * scaleFactor));
        int rightSpeed = calculateSpeed(abs(rightInitialSpeed * scaleFactor));
        USE_SERIAL.printf("scaled speed by: %d\r\n", scaleFactor);
        USE_SERIAL.print(leftSpeed);
        USE_SERIAL.print(", ");
        USE_SERIAL.println(rightSpeed);
        //set enable pins to match motor direction
        if (leftInitialSpeed > 0) {
          USE_SERIAL.println("left forward");
          digitalWrite(PIN_LEFT_DIRECTION1, HIGH);
          digitalWrite(PIN_LEFT_DIRECTION2, LOW);
        } else {
          USE_SERIAL.println("left backward");
          digitalWrite(PIN_LEFT_DIRECTION1, LOW);
          digitalWrite(PIN_LEFT_DIRECTION2, HIGH);
        }
        if (rightInitialSpeed > 0) {
          USE_SERIAL.println("right forward");
          digitalWrite(PIN_RIGHT_DIRECTION1, HIGH);
          digitalWrite(PIN_RIGHT_DIRECTION2, LOW);
        } else {
          USE_SERIAL.println("right backward");
          digitalWrite(PIN_RIGHT_DIRECTION1, LOW);
          digitalWrite(PIN_RIGHT_DIRECTION2, HIGH);
        }
        //set PWM for both motors
        analogWrite(PIN_LEFT_ENABLE, leftSpeed);
        analogWrite(PIN_RIGHT_ENABLE, rightSpeed);
      }
      break;
    case WStype_BIN:
      USE_SERIAL.printf("[%u] get binary lenght: %u\n", num, lenght);
      hexdump(payload, lenght);

      // send message to client
      // webSocket.sendBIN(num, payload, lenght);
      break;
  }

}


int calculateSpeed (int percentage) {
  return map(percentage, 0, 100, 0, 1023);
}

int limit(int value) {
  if (value > 100) {
    return 100;
  }
  return value;
}

