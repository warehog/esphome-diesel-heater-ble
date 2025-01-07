#pragma once

#include <cinttypes>
#include <string>

namespace esphome {
namespace diesel_heater_ble {

enum class HeaterClass {
  HEATER_AA_55,            // The one that sends responses with 0xAA 0x55 header
  HEATER_AA_66,            // The one that sends responses with 0xAA 0x66 header
  HEATER_AA_55_ENCRYPTED,  // The one that sends responses with 0xAA 0x55 header and encrypted data
  HEATER_AA_66_ENCRYPTED,  // The one that sends responses with 0xAA 0x66 header and encrypted data
  HEATER_CLASS_UNKNOWN
};

class HeaterState {
 public:
  HeaterClass heater_class;
  uint8_t rcv_cmd;
  uint8_t runningstate;
  uint8_t errcode;
  uint8_t runningstep;
  uint16_t altitude;
  uint8_t runningmode;
  uint8_t setlevel;
  uint8_t settemp;
  uint16_t supplyvoltage;
  uint16_t casetemp;
  uint16_t cabtemp;

  // encoded types only
  uint16_t sttime;
  uint16_t autotime;
  uint16_t runtime;
  uint8_t isauto;
  uint8_t language;
  uint8_t tempoffset;
  uint8_t tankvolume;
  uint8_t oilpumptype;
  bool rf433onoff;
  uint8_t tempunit;
  uint8_t altiunit;
  uint8_t automaticheating;

  // return table format of data with columnt names
  std::string to_string() {
    return "HeaterState: \n"
           "  heater_class: " +
           std::to_string(static_cast<int>(heater_class)) +
           "\n"
           "  rcv_cmd: " +
           std::to_string(rcv_cmd) +
           "\n"
           "  runningstate: " +
           std::to_string(runningstate) +
           "\n"
           "  errcode: " +
           std::to_string(errcode) +
           "\n"
           "  runningstep: " +
           std::to_string(runningstep) +
           "\n"
           "  altitude: " +
           std::to_string(altitude) +
           "\n"
           "  runningmode: " +
           std::to_string(runningmode) +
           "\n"
           "  setlevel: " +
           std::to_string(setlevel) +
           "\n"
           "  settemp: " +
           std::to_string(settemp) +
           "\n"
           "  supplyvoltage: " +
           std::to_string(supplyvoltage) +
           "\n"
           "  casetemp: " +
           std::to_string(casetemp) +
           "\n"
           "  cabtemp: " +
           std::to_string(cabtemp) +
           "\n"
           "  sttime: " +
           std::to_string(sttime) +
           "\n"
           "  autotime: " +
           std::to_string(autotime) +
           "\n"
           "  runtime: " +
           std::to_string(runtime) +
           "\n"
           "  isauto: " +
           std::to_string(isauto) + "\n";
  }
};

}  // namespace diesel_heater_ble
}  // namespace esphome
