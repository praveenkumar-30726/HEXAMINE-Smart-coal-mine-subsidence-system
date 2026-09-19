/* ============================================================
   05 — Receiver: parse structured DATA, reply with ACK
   Pairs with sketch 04. Detects gaps in the sequence numbers.
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

const long FREQUENCY = 433100000L + ((PAIR_ID - 1) * 300000L);
const uint8_t SYNC_WORD = 0x10 + PAIR_ID;

// ---- MUST match the transmitter ----
const int  SPREADING_FACTOR = 7;
const long BANDWIDTH        = 125000;
const int  CODING_RATE      = 5;
const int  TX_POWER         = 17;

const unsigned long TURNAROUND_MS = 40;  // let the sender switch to receive

unsigned int receivedCount = 0;
int lastSeq = 0;
unsigned int gapsDetected = 0;

String field(String msg, String key) {
  int start = msg.indexOf(key + "=");
  if (start < 0) return "";
  start += key.length() + 1;
  int end = msg.indexOf(';', start);
  if (end < 0) end = msg.length();
  return msg.substring(start, end);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }   // wait for USB CDC
  delay(1000);
  Serial.println("=== RX: DATA + ACK ===");

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

  Serial.println("LoRa OK - listening");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;

  String message = "";
  while (LoRa.available()) message += (char)LoRa.read();

  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();

  String node = field(message, "NODE_ID");
  String type = field(message, "TYPE");
  String val  = field(message, "VALUE");
  int    seq  = field(message, "SEQ").toInt();

  if (type != "DATA") {
    Serial.print("Ignored packet of type: ");
    Serial.println(type);
    return;
  }

  receivedCount++;
  if (lastSeq > 0 && seq > lastSeq + 1) {
    gapsDetected += (seq - lastSeq - 1);
    Serial.print("  !! gap: missed ");
    Serial.print(seq - lastSeq - 1);
    Serial.println(" packet(s)");
  }
  lastSeq = seq;

  Serial.print("SEQ "); Serial.print(seq);
  Serial.print("  from node "); Serial.print(node);
  Serial.print("  value "); Serial.print(val);
  Serial.print("  RSSI "); Serial.print(rssi);
  Serial.print(" dBm  SNR "); Serial.print(snr, 1);
  Serial.print(" dB   [received ");
  Serial.print(receivedCount);
  Serial.print(", missed ");
  Serial.print(gapsDetected);
  Serial.println("]");

  // ---- send the acknowledgment back ----
  delay(TURNAROUND_MS);
  char ack[24];
  snprintf(ack, sizeof(ack), "ACK=%03d", seq);
  LoRa.beginPacket();
  LoRa.print(ack);
  LoRa.endPacket();
}
