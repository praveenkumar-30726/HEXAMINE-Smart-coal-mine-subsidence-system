/* ============================================================
   07 — Infrastructure Node (Node B)
   Receives the emergency message, validates it, reports it,
   and acknowledges it. Pairs with sketch 06.
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

const long FREQUENCY = 433100000L + ((PAIR_ID - 1) * 300000L);
const uint8_t SYNC_WORD = 0x10 + PAIR_ID;

const int  SPREADING_FACTOR = 7;
const long BANDWIDTH        = 125000;
const int  CODING_RATE      = 5;
const int  TX_POWER         = 17;

const unsigned long TURNAROUND_MS = 40;

/* ---- NOT REAL SECURITY ----------------------------------
   Node IDs this infrastructure will act on. Anyone can put
   "01" in their packet and be believed, so this stops
   accidents, not attackers. Real systems use cryptographic
   authentication. See PPT 2, slide 14.
   --------------------------------------------------------- */
const int   ALLOWED_COUNT = 2;
const char* ALLOWED_NODES[ALLOWED_COUNT] = { "01", "02" };

int lastSeqSeen = -1;
unsigned int accepted = 0, rejected = 0;

String field(String msg, String key) {
  int start = msg.indexOf(key + "=");
  if (start < 0) return "";
  start += key.length() + 1;
  int end = msg.indexOf(';', start);
  if (end < 0) end = msg.length();
  return msg.substring(start, end);
}

bool isAllowed(String node) {
  for (int i = 0; i < ALLOWED_COUNT; i++) {
    if (node == ALLOWED_NODES[i]) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }   // wait for USB CDC
  delay(1000);
  Serial.println("=== INFRASTRUCTURE NODE ===");

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

  Serial.println("LoRa OK - waiting for emergency messages");
  Serial.println();
}

void loop() {
  if (LoRa.parsePacket() == 0) return;

  String message = "";
  while (LoRa.available()) message += (char)LoRa.read();

  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();

  String node     = field(message, "NODE_ID");
  String type     = field(message, "TYPE");
  String vehicle  = field(message, "VEHICLE");
  String priority = field(message, "PRIORITY");
  int    seq      = field(message, "SEQ").toInt();

  // ---- check 1: is this a message we care about? ----
  if (type != "EMERGENCY") {
    Serial.print("REJECTED  wrong type: ");
    Serial.println(type.length() ? type : "(none)");
    rejected++;
    return;
  }

  // ---- check 2: is the packet complete? ----
  if (vehicle.length() == 0 || priority.length() == 0 || seq == 0) {
    Serial.println("REJECTED  malformed packet - missing fields");
    rejected++;
    return;
  }

  // ---- check 3: do we accept messages from this node? ----
  if (!isAllowed(node)) {
    Serial.print("REJECTED  unauthorised node: ");
    Serial.println(node);
    rejected++;
    return;
  }

  bool duplicate = (seq == lastSeqSeen);
  lastSeqSeen = seq;
  if (!duplicate) accepted++;

  Serial.println("========================================");
  Serial.println("EMERGENCY MESSAGE RECEIVED");
  Serial.println();
  Serial.print("VEHICLE : "); Serial.println(vehicle);
  Serial.print("PRIORITY: "); Serial.println(priority);
  Serial.print("FROM    : node "); Serial.println(node);
  Serial.print("SEQ     : "); Serial.println(seq);
  Serial.print("RSSI    : "); Serial.print(rssi);   Serial.println(" dBm");
  Serial.print("SNR     : "); Serial.print(snr, 1); Serial.println(" dB");

  if (duplicate) {
    Serial.println("NOTE    : duplicate of the last message");
    Serial.println("ACTION  : none - already handled");
  } else if (priority == "HIGH") {
    Serial.println("ACTION  : signal priority granted");
  } else if (priority == "MEDIUM") {
    Serial.println("ACTION  : logged, priority queued");
  } else {
    Serial.println("ACTION  : logged only");
  }

  Serial.print("totals  : accepted ");
  Serial.print(accepted);
  Serial.print(", rejected ");
  Serial.println(rejected);
  Serial.println("========================================");
  Serial.println();

  // ---- acknowledge ----
  delay(TURNAROUND_MS);
  char ack[24];
  snprintf(ack, sizeof(ack), "ACK=%03d", seq);
  LoRa.beginPacket();
  LoRa.print(ack);
  LoRa.endPacket();
}
