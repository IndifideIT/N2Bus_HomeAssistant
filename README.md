# N2‑Raw‑MQTT – passive Johnson Controls N2 sniffer for Home Assistant

**Author:** Przemysław Myk · **License:** MIT

This project turns an **Arduino Mega 2560** + **W5100 Ethernet shield** + **MAX485** into a *read‑only* bridge that listens to every frame on a Johnson‑Controls N2 bus (RS‑485, 9600 Bd) and publishes the decoded values to **MQTT**.

* **Zero traffic injection** – DE + RE on MAX485 are tied to GND (receiver‑only).  
* Supports single‑point `0x80`, extended `0x84`, and burst `0xA8` (8 points) replies.  
* Publishes every value to `home/n2/raw/<device_addr>/<register>` **with `retain`** so Home Assistant keeps the last state after a restart.

---

## Hardware

| Part                                              | Notes                                                     |
| ------------------------------------------------- | --------------------------------------------------------- |
| Arduino **Mega 2560**                             | any genuine or clone board                                |
| **W5100** Ethernet shield (Arduino UNO footprint) | plugs straight into Mega headers                          |
| **MAX485** TTL ↔ RS‑485 module                    | RO → RX3 (pin 15), DI unused, **DE & RE → GND** (RX‑only) |
| RJ‑45 / PoE injector *(optional)*                 | power the whole sniffer through the same cable            |

### Wiring

```text
N2‑bus A  ───────────► MAX485 A
N2‑bus B  ───────────► MAX485 B
MAX485 RO ───────────► Mega RX3 (pin 15)
MAX485 DE ─┐
MAX485 RE ─┴──► GND   (receiver‑only)
MAX485 VCC ───────► 5 V
MAX485 GND ───────► GND  (optionally connect to bus REF)
```

> If the installation exposes a **`COM/REF`** terminal, connect it to sniffer **GND** (directly or through ≈100 Ω). Otherwise leave GND floating – on short lines it usually still works.

---

## Firmware

The sketch **`N2RawMQTT.ino`** compiles in Arduino IDE with:

* **Ethernet** ≥ 2.0.0 (bundled with AVR core)  
* **PubSubClient** ≥ 2.8.0 (install via *Tools → Manage Libraries*)

### Configuration inside the sketch

```cpp
IPAddress ip      (192,168,1,51);    // sniffer IP
IPAddress mqttSrv (192,168,1,10);    // MQTT broker (e.g. Mosquitto add‑on)
byte mac[] = { 0xDE,0xAD,0xBE,0xEF,0xFE,0xED }; // unique MAC

// If your broker requires authentication:
// mqtt.connect("n2‑raw‑sniffer", "ha_user", "strong_pass");
```

Upload, open **Serial Monitor** at **115 200 Bd** – you should see `connected to broker`.

---

## Home Assistant integration

1. Install the **Mosquitto Broker** add‑on (or use any MQTT server).  
2. In **Developer Tools → MQTT → Listen** subscribe to `home/n2/raw/#` – you should immediately see messages like:

   ```
   Topic: home/n2/raw/8/1   Payload: 22.3
   Topic: home/n2/raw/8/7   Payload: 1
   ```

3. Add entities (YAML example):

   ```yaml
   mqtt:
     sensor:
       - name: "Room 8 temperature"
         state_topic: "home/n2/raw/8/1"
         unit_of_measurement: "°C"
         device_class: temperature

     binary_sensor:
       - name: "Fan 8 running"
         state_topic: "home/n2/raw/8/7"
         payload_on: "1"
         payload_off: "0"
         device_class: running
   ```

   *(You can also use the GUI → Integrations → MQTT → “Add MQTT Sensor”).*

4. Build graphs / dashboards as usual.

Adding a new point later requires **only another YAML entry** – no firmware edits.

---

## Troubleshooting

| Symptom                       | Likely cause / fix                                   |
| ----------------------------- | ---------------------------------------------------- |
| **No MQTT messages**          | wrong broker IP/port • firewall • A/B swapped        |
| **CRC/LRC errors**            | add REF/GND wire or use isolated RS‑485 module       |
| **`unknown` after HA restart**| ensure `retain=true` in `mqtt.publish` (already set) |

---

## License & Disclaimer

**MIT** – feel free to fork, improve and share.

> **Disclaimer**  
> This project is provided *“as‑is”* without any express or implied warranty.  
> Connecting the sniffer to a live N2 bus and using the accompanying firmware is **entirely at your own risk**.  
> The author (Przemysław Myk) is not liable for any loss, malfunction or damage arising from the use, misuse or inability to use this software or hardware description.
