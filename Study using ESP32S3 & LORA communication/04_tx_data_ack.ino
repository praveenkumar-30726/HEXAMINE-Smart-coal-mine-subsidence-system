/* ============================================================
   04 — Transmitter: structured DATA + ACK + statistics
   This is the sketch used for BOTH afternoon experiments
   (distance, and spreading factor).

   Sends : NODE_ID=01;TYPE=DATA;VALUE=123;SEQ=001
   Waits : ACK=001
   Prints: RSSI, SNR, round-trip time, and a summary every run.
   BOARD: ESP32-S3   (Tools > USB CDC On Boot: Enabled)
   ============================================================ */

#include <SPI.h>
#include <LoRa.h>

// ---- ESP32-S3 pin map ----
#define LORA_SCK  12
#define LORA_MISO 13
#define LORA_MOSI 11
#define LORA_SS   10
#define LORA_RST   5
#define LORA_DIO0  4

#define PAIR_ID 1                       // <-- same on both nodes
#define NODE_ID "01"

const long FREQUENCY = 433100000L + ((PAIR_ID - 1) * 300000L);
const uint8_t SYNC_WORD = 0x10 + PAIR_ID;

// ---- THE EXPERIMENT KNOBS -------------------------------
// Change these, and change them on the RECEIVER TOO.
const int  SPREADING_FACTOR = 7;        // experiment 2: 7,8,9,10,11,12
const long BANDWIDTH        = 125000;
const int  CODING_RATE      = 5;
const int  TX_POWER         = 17;
// ---------------------------------------------------------

const int PACKETS_PER_RUN = 50;         // one measurement run
const unsigned long ACK_TIMEOUT_MS = 3000;
const unsigned long GAP_BETWEEN_PACKETS_MS = 1000;

unsigned int  seq = 0;
unsigned int  sent = 0;
unsigned int  acked = 0;
unsigned long rttSum = 0;
unsigned long rttMin = 999999;
unsigned long rttMax = 0;

String field(String msg, String key) {
  int start = msg.indexOf(key + "=");
  if (start < 0) return "";
  start += key.length() + 1;
  int end = msg.indexOf(';', start);
  if (end < 0) end = msg.length();
  return msg.substring(start, end);
}

void printSummary() {
  Serial.println();
  Serial.println("---------- RUN SUMMARY ----------");
  Serial.print("SF ");            Serial.print(SPREADING_FACTOR);
  Serial.print("  BW ");          Serial.print(BANDWIDTH / 1000); Serial.print(" kHz");
  Serial.print("  CR 4/");        Serial.print(CODING_RATE);
  Serial.print("  TX ");          Serial.print(TX_POWER); Serial.println(" dBm");
  Serial.print("Packets sent    : "); Serial.println(sent);
  Serial.print("ACKs received   : "); Serial.println(acked);
  Serial.print("Lost            : "); Serial.println(sent - acked);
  Serial.print("Success rate    : ");
  Serial.print(sent > 0 ? (100.0 * acked / sent) : 0.0, 1);
  Serial.println(" %");
  if (acked > 0) {
    Serial.print("RTT min/avg/max : ");
    Serial.print(rttMin); Serial.print(" / ");
    Serial.print(rttSum / acked); Serial.print(" / ");
    Serial.print(rttMax); Serial.println(" ms");
  }
  Serial.println("---------------------------------");
  Serial.println("Write these numbers in your table, then move to the next distance.");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }   // wait for USB CDC
  delay(1000);
  Serial.println("=== TX: DATA + ACK ===");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);   // S3 needs this
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("LoRa init FAILED - check wiring");
    while (true) { delay(1000); }
  }
  LoRa.setSyncWord(SYNC_WORD);
  LoRa.setSpreadingFactor(SPREADING_FACTOR);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setCodingRate4(CODING_RATE);
  LoRa.setTxPower(TX_POWER);

  Serial.print("LoRa OK  -  run of ");
  Serial.print(PACKETS_PER_RUN);
  Serial.println(" packets starting");
}

void loop() {
  if (sent >= PACKETS_PER_RUN) {
    delay(2000);
    return;                              // run finished; press RESET for a new run
  }

  seq++;
  char packet[64];
  snprintf(packet, sizeof(packet),
           "NODE_ID=%s;TYPE=DATA;VALUE=%d;SEQ=%03u", NODE_ID, (int)random(0, 1000), seq);

  unsigned long t0 = millis();
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  sent++;

  // ---- wait for the ACK ----
  bool gotAck = false;
  unsigned long deadline = millis() + ACK_TIMEOUT_MS;
  while (millis() < deadline && !gotAck) {
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
      String reply = "";
      while (LoRa.available()) reply += (char)LoRa.read();
      if (field(reply, "ACK").toInt() == (int)seq) {
        unsigned long rtt = millis() - t0;
        gotAck = true;
        acked++;
        rttSum += rtt;
        if (rtt < rttMin) rttMin = rtt;
        if (rtt > rttMax) rttMax = rtt;

        Serial.print("SEQ ");   Serial.print(seq);
        Serial.print("  ACK ok  RTT ");  Serial.print(rtt); Serial.print(" ms");
        Serial.print("  RSSI "); Serial.print(LoRa.packetRssi());
        Serial.print(" dBm  SNR "); Serial.print(LoRa.packetSnr(), 1);
        Serial.println(" dB");
      }
    }
  }

  if (!gotAck) {
    Serial.print("SEQ ");
    Serial.print(seq);
    Serial.println("  NO ACK  (data lost, or the reply was lost)");
  }

  if (sent == PACKETS_PER_RUN) printSummary();

  delay(GAP_BETWEEN_PACKETS_MS);
}
