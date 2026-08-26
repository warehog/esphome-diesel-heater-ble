#include <cassert>
#include <cstdint>
#include <vector>

#include "messages.h"

using esphome::diesel_heater_ble::Request;
using esphome::diesel_heater_ble::SetPowerRequest;
using esphome::diesel_heater_ble::StatusRequest;

int main() {
  const StatusRequest status_request;
  const std::vector<uint8_t> expected_status{0xAA, 0x55, 0x0C, 0x22, 0x01, 0x00, 0x00, 0x2F};
  assert(status_request.toBytes() == expected_status);

  const SetPowerRequest power_on(true);
  const std::vector<uint8_t> expected_power_on{0xAA, 0x55, 0x0C, 0x22, 0x03, 0x01, 0x00, 0x32};
  assert(power_on.toBytes() == expected_power_on);

  const Request checksum_wraparound(0xFF, 0xFF, 0xFF);
  assert(checksum_wraparound.toBytes().back() == 0x2B);
  return 0;
}
