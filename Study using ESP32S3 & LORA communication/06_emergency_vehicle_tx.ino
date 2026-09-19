/* ============================================================
   06 — Emergency Vehicle Node (Node A)
   Sends an emergency priority message and waits for an ACK.
   Pairs with sketch 07.
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

#define PAIR_ID 1
#define NODE_ID  "01"
#define VEHICLE  "AMBULANCE"          // try FIRE_ENGINE, POLICE
#define PRIORITY "HIGH"               // try MEDIUM, LOW

const long FREQUENCY = 433100000L + ((PAIR_ID - 1) * 300000L);
const uint8_t SYNC_WORD = 0x10 + PAIR_ID;

const int  SPREADING_FACTOR = 7;
const long BANDWIDTH        = 125000;
const int  CODING_RATE      = 5;
const int  TX_POWER         = 17;

const unsigned long ACK_TIMEOUT_MS = 2000;
const int MAX_RETRIES = 3;

unsigned int seq = 0;
unsigned int confirmed = 0, unconfirmed = 0;

String field(String msg, String key) {
  int start = msg.indexOf(key + "=");
  if (start < 0) return "";
  start += key.length() + 1;
  int end = msg.indexOf(';', start);
  if (end < 0) end = msg.length();
  return msg.substring(start, end);
}

bool sendEmergency(unsigned int s) {
  char packet[96];
  snprintf(packet, sizeof(packet),
           "NODE_ID=%s;TYPE=EMERGENCY;VEHICLE=%s;PRIORITY=%s;SEQ=%03u",
           NODE_ID, VEHICLE, PRIORITY, s);

  unsigned long t0 = millis();
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();

  Serial.print("  TX -> ");
  Serial.println(packet);

  unsigned long deadline = millis() + ACK_TIMEOUT_MS;
  while (millis() < deadline) {
    if (LoRa.parsePacket() > 0) {
      String reply = "";
      while (LoRa.available()) reply += (char)LoRa.read();
      if (field(reply, "ACK").toInt() == (int)s) {
        Serial.print("       ACK in ");
        Serial.print(millis() - t0);
        Serial.print(" ms   RSSI ");
        Serial.print(LoRa.packetRssi());
        Serial.print(" dBm  SNR ");
        Serial.print(LoRa.packetSnr(), 1);
        Serial.println(" dB");
        return true;
      }
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }   // wait for USB CDC
  delay(1000);
  Serial.println("=== EMERGENCY VEHICLE NODE ===");

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

  Serial.println("LoRa OK");
}

void loop() {
  seq++;
  Serial.print("--- message ");
  Serial.print(seq);
  Serial.println(" ---");

  bool delivered = false;
  for (int attempt = 1; attempt <= MAX_RETRIES && !delivered; attempt++) {
    if (attempt > 1) {
      Serial.print("  retry ");
      Serial.print(attempt);
      Serial.print(" of ");
      Serial.println(MAX_RETRIES);
    }
    delivered = sendEmergency(seq);
  }

  if (delivered) {
    confirmed++;
    Serial.println("  >>> DELIVERY CONFIRMED");
  } else {
    unconfirmed++;
    Serial.println("  >>> DELIVERY NOT CONFIRMED after 3 attempts");
    Serial.println("      The data was lost, or the ACK was lost.");
    Serial.println("      From here, we cannot tell which.");
  }

  Serial.print("  totals: confirmed ");
  Serial.print(confirmed);
  Serial.print(", unconfirmed ");
  Serial.println(unconfirmed);
  Serial.println();

  delay(5000);
}
