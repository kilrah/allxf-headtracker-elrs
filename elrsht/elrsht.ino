// Bridge from AllXF Headtracker UART to ELRS backpack

// msp.h and some other bits from https://github.com/jlpoltrack/ELRS-Headtracker-to-SBUS/

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <mbedtls/md5.h>
#include <msp.h>

// Must match your ELRS / Backpack setup
#define BINDING_PHRASE   "MY_BINDING_PHRASE"  
#define RX_PIN           4 // Connects to T on headtracker
#define LED              8 // Board LED
#define WIFI_POWER       WIFI_POWER_2dBm // see https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFiGeneric.h#L51 for available values

uint8_t rxBuf[8];
uint8_t rx_count;

uint8_t uid[6];

const uint8_t ht_message_header = 0x5A;
const uint8_t ht_message_footer = 0xA5;

// From Ardupilot, https://github.com/Hwurzburg/ardupilot/blob/5a4f7b50058825af34de3f0f73198a7733186df5/libraries/AP_Mount/AP_Mount_CADDX.h#L65
typedef struct __attribute__  ((packed)) {
	uint8_t  mode:3;        // Gimbal Mode [0~7] [Only 0 1 2 modes are supported for the time being]
	int16_t  sensitivity:5; // Stabilization sensibility [-15~15]
	uint8_t  reserved:4;    
	int32_t  roll:12;       // Roll angle [-2048~2047] => [-180~180]
	int32_t  tilt:12;       // Pitch angle [-2048~2047] => [-180~180]
	int32_t  pan:12;        // Yaw angle [-2048~2047] => [-180~180]
	uint8_t  crch;          // Data validation H
	uint8_t  crcl;          // Data validation L
} ht_message_t;

typedef enum {
  STATE_NEW_MESSAGE,
  STATE_RECEIVING,
  STATE_WAIT_FOOTER
} ht_state_t;

ht_message_t ht_msg;
ht_state_t ht_state = STATE_WAIT_FOOTER;

void sendMspEspNow(uint16_t function, const uint8_t *data, uint16_t len) {
    uint8_t frame[32];
    uint8_t frameLen = mspBuildCommand(frame, sizeof(frame), function, data, len);
    if (frameLen) esp_now_send(uid, frame, frameLen);
}

void sendHTPacket() {
    int16_t angles[3];
    angles[0] = (ht_msg.pan+2048)/2;
    angles[1] = (ht_msg.tilt+2048)/2;
    angles[2] = (ht_msg.roll+2048)/2;
    sendMspEspNow(MSP_ELRS_SET_PTR, (uint8_t*) angles, 6);
}

// Replicates ELRS build system: MD5('-DMY_BINDING_PHRASE="<phrase>"')[0:6]
void generateUID(const char *phrase, uint8_t *out) {
  char buf[128];
  snprintf(buf, sizeof(buf), "-DMY_BINDING_PHRASE=\"%s\"", phrase);

  uint8_t hash[16];
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, (const uint8_t *)buf, strlen(buf));
  mbedtls_md5_finish(&ctx, hash);
  mbedtls_md5_free(&ctx);

  memcpy(out, hash, 6);
  out[0] &= ~0x01;  // Ensure unicast (clear LSB of first byte)
}

void setup() {
  Serial.begin(115200);

  // HT UART
  Serial1.begin(115200, SERIAL_8N1, RX_PIN, -1);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  generateUID(BINDING_PHRASE, uid);

  // Set up Wifi at low power
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, uid);
  WiFi.setTxPower(WIFI_POWER);

  // Set channel to 1 (hardcoded in all Backpack modules)
  WiFi.begin("", "", 1);
  WiFi.disconnect();

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, uid, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK)
      Serial.print("ESP-NOW add peer failed\n");

}

void loop() {
  // Read HT
  if(Serial1.available()) {
    uint8_t data = Serial1.read();

    if(ht_state == STATE_RECEIVING) {
      rxBuf[rx_count++] = data;
      if(rx_count == sizeof(rxBuf)) {
        ht_state = STATE_WAIT_FOOTER;
      }
    }

    else if(ht_state == STATE_NEW_MESSAGE && data == ht_message_header) {
      ht_state = STATE_RECEIVING;
      rx_count = 0;
    }

    else if(ht_state == STATE_WAIT_FOOTER && data == ht_message_footer) {
      ht_state = STATE_NEW_MESSAGE;
      uint8_t* ptr = (uint8_t*)&ht_msg;
      memcpy(ptr, rxBuf, sizeof(rxBuf));
      sendHTPacket();
      digitalWrite(LED, !digitalRead(LED));
      Serial.printf("mode: %d\tsens: %3d\troll: %6d\tpitch: %6d\tyaw: %6d\n", ht_msg.mode, ht_msg.sensitivity, ht_msg.roll, ht_msg.tilt, ht_msg.pan);
    }
  }
}
