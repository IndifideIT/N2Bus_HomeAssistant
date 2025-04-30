// ---------------------------------------------------------------------------
//  N2RawMQTT.ino  –  Johnson‑Controls N2 passive sniffer  ➜  MQTT
//  Author : Przemysław Myk  (2025‑05‑01)
// ---------------------------------------------------------------------------
//  Hardware  :  Arduino MEGA 2560  +  W5100 Ethernet shield  +  MAX485 RS‑485
//               RO ➜ RX3 (pin 15)   ·  DI — not connected
//               DE & RE tied to GND (receiver only, transmitter disabled)
//               A / B  connect to the 2‑wire N2 bus
//
//  Function  :  Listens to *all* slave replies (command 0x5B):
//                  • classic 0x80 frames (one point)
//                  • "extended" 0x84 frames (structure + point)
//                  • burst 0xA8 frames (8 points in one reply)
//               Decodes every value heuristically and publishes to MQTT
//               topic:  home/n2/raw/<addr>/<reg>   with retain flag.
//
//  This sketch **never writes** to the bus → completely safe, read‑only sniffer.
// ---------------------------------------------------------------------------
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

// ---------------- Ethernet / MQTT -----------------------------------------
byte mac[]   = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };   // unique MAC


// ---------- user settings --------------------------------------------------
const char*  TOPIC_ROOT = "nae1";    // ← dowolny prefiks MQTT
const char*  LINE_ID    = "n2";       // ta konkretna RS-485

IPAddress ip       (192, 168, 1,  51);   // sniffer IP address
IPAddress mqttSrv  (192, 168, 1,  227);   // MQTT broker (Home Assistant)
//---------------------------------------------------------------------------/


EthernetClient eth;
PubSubClient   mqtt(eth);

// ---------------- RS‑485 ---------------------------------------------------
#define STX 0x02                              // start‑of‑frame marker
HardwareSerial &RS = Serial3;                 // RX on pin 15, TX on pin 14 (unused)

// ---------------- Runtime buffers -----------------------------------------
struct Last { uint8_t addr, reg; float val; }; // store last published value
#define MAX_SEEN 128                          // adjust if you need more points
Last seen[MAX_SEEN]; uint8_t seenCnt = 0;

// ---------------- Helper functions ----------------------------------------
static inline bool lrcOK(const uint8_t *f)            // Longitudinal Redundancy Check
{
  uint8_t len = f[1] & 0x7F;                           // LEN without MSB
  uint8_t sum = 0;                                    // sum of LEN…DATA
  for (uint8_t i = 1; i <= len; i++) sum += f[i];
  return sum == 0;                                    // valid when sum + LRC == 0
}

//  Decoders for different point types --------------------------------------
static inline float decodeBI(uint8_t lo)            { return lo ? 1 : 0; }
static inline float decodeADF(uint16_t raw)         { return raw / 10.0; }
static inline float decodeAI(uint16_t raw)          { return (raw / 512.0 - 32) * 5 / 9; }
static inline float decodePMK(uint16_t raw)         { return raw / 512.0; }   // e.g. humidity %

//  Heuristic decoder when type is unknown ----------------------------------
float genericDecode(uint8_t lo, uint8_t hi)
{
  uint16_t raw = lo | (hi << 8);
  if (raw <= 1)                          return decodeBI(lo);          // BI / BO
  if ((raw % 10 == 0) && raw < 10000)    return decodeADF(raw);        // ADF / ADI
  if (raw > 10000)                       return decodePMK(raw);        // PMK‑like
  return decodeAI(raw);                                                  // default AI (temperature °C)
}

//  Publish only if value changed meaningfully ------------------------------
void publishIfChanged(uint8_t addr, uint8_t reg, float v)
{
  for (uint8_t i = 0; i < seenCnt; i++)
    if (seen[i].addr == addr && seen[i].reg == reg) {
      if (abs(seen[i].val - v) < 0.1) return;       // ignore tiny change
      seen[i].val = v;
      goto send;
    }
  if (seenCnt < MAX_SEEN) seen[seenCnt++] = {addr, reg, v};

send:
  char topic[48];
  sprintf(topic, "%s/%s/%u/%u", TOPIC_ROOT, LINE_ID, addr, reg);
  char payload[16];
  dtostrf(v, 0, 2, payload);
  mqtt.publish(topic, payload, true);               // retain = true
}

//  Main frame handler  ------------------------------------------------------
void handleReply(const uint8_t *f)
{
  uint8_t ln   = f[1] & 0x7F;
  uint8_t addr = f[2];
  if (f[3] != 0x5B || ln < 7) return;              // only 0x5B replies

  bool burst = (ln > 10);                           // 0xA8 = 8 points (23 bytes)
  uint8_t off = 4;                                  // index of REG byte in 0x80 reply

  // extended 0x84 adds STRUCT byte 0x05/0x06 before REG
  if (!burst && (f[4] == 0x05 || f[4] == 0x06)) off++;

  if (burst) {
    uint8_t regStart = f[off];
    uint8_t count    = (ln - (off + 3)) / 2;        // number of 2‑byte values
    for (uint8_t i = 0; i < count; i++) {
      uint8_t lo = f[off + 1 + i * 2];
      uint8_t hi = f[off + 2 + i * 2];
      float v    = genericDecode(lo, hi);
      publishIfChanged(addr, regStart + i, v);
    }
  } else {
    uint8_t reg = f[off];
    uint8_t lo  = f[off + 1];
    uint8_t hi  = f[off + 2];
    float v     = genericDecode(lo, hi);
    publishIfChanged(addr, reg, v);
  }
}

// ---------------------------  SETUP / LOOP  -------------------------------
uint8_t buf[80];               // frame buffer
uint8_t idx = 0;               // current fill index

void setup()
{
  Ethernet.begin(mac, ip);
  mqtt.setServer(mqttSrv, 1883);
  mqtt.connect("n2-raw-sniffer", "ha_user", "strong_pass"); // Pass for mqtt
  RS.begin(9600);              // N2 bus speed
}

void loop()
{
  if (!mqtt.connected()) mqtt.connect("n2‑raw‑sniffer");

  // byte‑wise read from RS‑485 --------------------------------------------
  while (RS.available()) {
    uint8_t b = RS.read();
    if (idx == 0 && b != STX) continue;            // wait for STX
    buf[idx++] = b;
    if (idx == 2) continue;                        // need LEN next
    if (idx > 2) {
      uint8_t need = (buf[1] & 0x7F) + 3;          // STX + LEN + payload + LRC
      if (idx >= need) {
        if ((buf[1] & 0x80) && lrcOK(buf))         // must be reply & checksum OK
          handleReply(buf);
        idx = 0;                                   // reset buffer
      }
    }
  }
  mqtt.loop();
}
