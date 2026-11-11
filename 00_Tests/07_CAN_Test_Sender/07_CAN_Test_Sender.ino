// 07_CAN_Test_Sender.ino  (upload to ESP32 #1)
// Sends a counter over CAN every second using the ESP32's built-in CAN controller (TWAI).
// Wiring: GPIO17 -> CTX, GPIO16 -> CRX on the SN65HVD230, 3V3 and GND,
//         CANH-CANH and CANL-CANL to the other node, plus one GND wire between the boards.
// Run 08_CAN_Test_Receiver on ESP32 #2 at the same time.
// Expected: "Sent 0", "Sent 1", ... If you see "No ACK", the other node is not
// connected or not running: CAN needs at least two working nodes.

#include "driver/twai.h"

#define CAN_TX 17
#define CAN_RX 16

uint8_t counter = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g, &t, &f) != ESP_OK || twai_start() != ESP_OK) {
    Serial.println("CAN driver failed to start");
    while (true) delay(1000);
  }
  Serial.println("CAN sender started at 500 kbit/s");
}

void loop() {
  twai_message_t msg = {};
  msg.identifier = 0x123;
  msg.data_length_code = 2;
  msg.data[0] = counter;
  msg.data[1] = 0xAB;

  if (twai_transmit(&msg, pdMS_TO_TICKS(100)) == ESP_OK) {
    Serial.printf("Sent %u\n", counter);
  } else {
    Serial.println("Send failed");
  }

  twai_status_info_t s;
  twai_get_status_info(&s);
  if (s.tx_error_counter > 100) Serial.println("No ACK: is the receiver connected and running?");
  if (s.state == TWAI_STATE_BUS_OFF) {
    Serial.println("Bus off, recovering");
    twai_initiate_recovery();
  } else if (s.state == TWAI_STATE_STOPPED) {
    twai_start();
  }

  counter++;
  delay(1000);
}
