N2‑Raw‑MQTT – passive Johnson Controls N2 sniffer for Home Assistant

Author : Przemysław Myk  ·  License : MIT

This project turns an Arduino Mega 2560 + W5100 Ethernet shield + MAX485 into a read‑only bridge that listens to every frame on a Johnson‑Controls N2 bus (RS‑485, 9600 Bd) and publishes the decoded values to MQTT.

No request frames are ever transmitted – DE + RE pins on MAX485 are permanently tied to GND – 100 % passive.

Works with classic single‑point frames 0x80, extended frames 0x84, and burst frames 0xA8 (8 points per reply).

Each value is published under home/n2/raw/<device_addr>/<register> **with **`` so Home Assistant sees the last state after a restart.

Hardware

Part

Notes

Arduino Mega 2560

any genuine or clone board

W5100 Ethernet shield (Arduino UNO footprint)

fits onto Mega headers

MAX485 TTL ↔ RS‑485 module

RO → RX3 (pin 15), DI unused, DE & RE → GND

RJ‑45 / PoE injector (optional)

power the whole sniffer from the same cable

Wiring:

N2‑bus A  ───────────► MAX485 A
N2‑bus B  ───────────► MAX485 B
MAX485 RO ───────────► Mega RX3 (pin 15)
MAX485 DE ─┐
MAX485 RE ─┴──► GND   (receiver‑only)
MAX485 VCC ───────► 5 V
MAX485 GND ───────► GND (optionally connect to bus REF)

If the N2 installation exposes a COM/REF terminal, connect it to the sniffer GND. Otherwise leave GND floating – it usually still works on short lines.

Firmware

N2RawMQTT.ino (see code canvas) compiles in the Arduino IDE with:

Ethernet ≥ 2.0.0  (comes with board package)

PubSubClient ≥ 2.8.0  (Install via Tools → Manage Libraries)

Settings inside the sketch

IPAddress ip      (192,168,1,51);   // LAN address of the sniffer
IPAddress mqttSrv (192,168,1,10);   // MQTT broker (e.g. Mosquitto add‑on)
byte mac[] = { 0xDE,0xAD,0xBE,0xEF,0xFE,0xED }; // unique MAC

If your broker requires credentials add them in mqtt.connect().

Upload, open Serial Monitor @ 115 200 Bd – you should see connected to broker.

Home Assistant integration

Install Mosquitto Broker (add‑on) or any MQTT server.

In Developer Tools → MQTT → Listen subscribe to home/n2/raw/# – you should see messages like:

Topic: home/n2/raw/8/1   Payload: 22.3
Topic: home/n2/raw/8/7   Payload: 1

Add sensors in YAML or via GUI:

mqtt:
  sensor:
    - name: "Room 8 temperature"
      state_topic: "home/n2/raw/8/1"
      unit_of_measurement: "°C"
      device_class: temperature

  binary_sensor:
    - name: "Fan 8 running"
      state_topic: "home/n2/raw/8/7"
      payload_on: "1"
      payload_off: "0"
      device_class: running

Create graphs / dashboards as usual.

Because every point on the bus is published automatically, adding a new sensor only requires another YAML entry – no code changes, no reflash.

Troubleshooting

Issue

Check

No MQTT messages

wrong broker IP / port • firewall • wiring A/B reversed

Random CRC errors

add REF/GND wire or use isolated RS‑485 module

HA shows unknown after reboot

ensure messages are sent with retain=true

License & Disclaimer

MIT – feel free to fork, improve and share.

DisclaimerThis project is provided “as‑is” without any express or implied warranty.Connecting the sniffer to a live N2 bus and using the accompanying firmware is entirely at your own risk.The author (Przemysław Myk) is not liable for any loss, malfunction or damage arising from the use, misuse or inability to use this software or hardware description.

