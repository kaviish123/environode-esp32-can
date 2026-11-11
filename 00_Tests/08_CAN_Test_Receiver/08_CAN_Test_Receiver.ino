// 08_CAN_Test_Receiver.ino  (upload to ESP32 #2)
// Receives the messages from 07_CAN_Test_Sender and prints them.
// Same wiring as the sender: GPIO17 -> CTX, GPIO16 -> CRX.
// Expected: "Received ID 0x123: counter = 0, 1, 2 ..." once a second.

#include "driver/twai.h"

#define CAN_TX 17
#define CAN_RX 16

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
  Serial.println("CAN receiver started, waiting for messages...");
}

void loop() {
  twai_message_t msg;
  if (twai_receive(&msg, pdMS_TO_TICKS(2000)) == ESP_OK) {
    Serial.printf("Received ID 0x%03lX, %d bytes: counter = %u\n",
                  (unsigned long)msg.identifier, msg.data_length_code, msg.data[0]);
  } else {
    Serial.println("Nothing received in 2 s. Check wiring and that the sender is running.");
  }
}
