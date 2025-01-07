#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "state.h"

namespace esphome {
namespace diesel_heater_ble {

class ResponseParser {
 public:
  static std::vector<uint8_t> decrypt(const std::vector<uint8_t> &raw) {
    // decrypt only if raw data starts with [0xDA, 0x07]
    if (raw[0] == 0xAA) {
      return raw;
    }

    std::vector<uint8_t> decrypted = raw;
    for (size_t i = 0; i < 48; i += 8) {
      decrypted[i] ^= 112;      // "p" in ASCII
      decrypted[i + 1] ^= 97;   // "a" in ASCII
      decrypted[i + 2] ^= 115;  // "s" in ASCII
      decrypted[i + 3] ^= 115;  // "s" in ASCII
      decrypted[i + 4] ^= 119;  // "w" in ASCII
      decrypted[i + 5] ^= 111;  // "o" in ASCII
      decrypted[i + 6] ^= 114;  // "r" in ASCII
      decrypted[i + 7] ^= 100;  // "d" in ASCII
    }
    return decrypted;
  }

  static HeaterClass detect_heater_class(const std::vector<uint8_t> &raw) {
    switch (raw[1]) {
      case 0x55:
        return HeaterClass::HEATER_AA_55;
      case 0x66:
        return HeaterClass::HEATER_AA_66;
      case 0x07:
        return HeaterClass::HEATER_AA_55_ENCRYPTED;
      default:
        return HeaterClass::HEATER_CLASS_UNKNOWN;
    }
  }

  static bool parse(const std::vector<uint8_t> &raw, HeaterState &state) {
    HeaterClass heater_class = detect_heater_class(raw);
    if (heater_class == HeaterClass::HEATER_CLASS_UNKNOWN)
      return false;

    std::vector<uint8_t> decrypted = decrypt(raw);

    state.heater_class = heater_class;
    state.rcv_cmd = decrypted[2];
    state.runningstate = decrypted[3];

    if (heater_class == HeaterClass::HEATER_AA_55 || heater_class == HeaterClass::HEATER_AA_55_ENCRYPTED) {
      state.errcode = decrypted[4];
    } else if (heater_class == HeaterClass::HEATER_AA_66) {
      state.errcode = decrypted[17];
    } else if (heater_class == HeaterClass::HEATER_AA_66_ENCRYPTED) {
      state.errcode = decrypted[35];
    }

    state.runningstep = decrypted[5];
    if (heater_class == HeaterClass::HEATER_AA_55 || heater_class == HeaterClass::HEATER_AA_66) {
      state.altitude = decrypted[6] + (decrypted[7] << 8);
    } else if (heater_class == HeaterClass::HEATER_AA_55_ENCRYPTED ||
               heater_class == HeaterClass::HEATER_AA_66_ENCRYPTED) {
      state.altitude = (decrypted[7] + (decrypted[6] << 8)) / 10;
    }

    state.runningmode = decrypted[8];

    if (heater_class == HeaterClass::HEATER_AA_55 || heater_class == HeaterClass::HEATER_AA_66) {
      if (state.runningmode == 0x00) {
        state.setlevel = decrypted[10] + 1;
      } else if (state.runningmode == 0x01) {
        state.setlevel = decrypted[9];
      } else if (state.runningmode == 0x02) {
        state.settemp = decrypted[9];
        state.setlevel = decrypted[10] + 1;
      }
    } else if (heater_class == HeaterClass::HEATER_AA_55_ENCRYPTED ||
               heater_class == HeaterClass::HEATER_AA_66_ENCRYPTED) {
      state.setlevel = decrypted[10];
      state.settemp = decrypted[9];
    }

    if (heater_class == HeaterClass::HEATER_AA_55 || heater_class == HeaterClass::HEATER_AA_66) {
      state.supplyvoltage = (decrypted[11] + (decrypted[12] << 8)) / 10;
    } else if (heater_class == HeaterClass::HEATER_AA_55_ENCRYPTED ||
               heater_class == HeaterClass::HEATER_AA_66_ENCRYPTED) {
      state.supplyvoltage = (decrypted[12] + (decrypted[11] << 8)) / 10;
    }

    if (heater_class == HeaterClass::HEATER_AA_55 || heater_class == HeaterClass::HEATER_AA_66) {
      state.casetemp = (decrypted[13] + (decrypted[14] << 8));
      state.cabtemp = (decrypted[15] + (decrypted[16] << 8));
    } else if (heater_class == HeaterClass::HEATER_AA_55_ENCRYPTED ||
               heater_class == HeaterClass::HEATER_AA_66_ENCRYPTED) {
      state.casetemp = (decrypted[14] + (decrypted[13] << 8));
      state.cabtemp = (decrypted[33] + (decrypted[32] << 8)) / 10;
    }

    // encrypted types only
    if (heater_class == HeaterClass::HEATER_AA_55_ENCRYPTED || heater_class == HeaterClass::HEATER_AA_66_ENCRYPTED) {
      state.sttime = decrypted[20] + (decrypted[19] << 8);
      state.autotime = decrypted[22] + (decrypted[21] << 8);
      state.runtime = decrypted[24] + (decrypted[23] << 8);
      state.isauto = decrypted[25];
      state.language = decrypted[26];
      state.tempoffset = decrypted[34];
      state.tankvolume = decrypted[28];
      state.oilpumptype = decrypted[29];
      if (raw[29] == 20) {
        state.rf433onoff = false;
      } else if (raw[29] == 21) {
        state.rf433onoff = true;
      }
      state.tempunit = decrypted[27];
      state.altiunit = decrypted[30];
      state.automaticheating = decrypted[31];
    }

    return true;
  }
};

class Request {
 public:
  uint8_t header_1 = 0xAA;
  uint8_t header_2 = 0x55;
  uint8_t password_1 = 0x0C;
  uint8_t password_2 = 0x22;
  uint8_t command;
  uint8_t data_1;
  uint8_t data_2;
  uint8_t checksum;

  Request(uint8_t command, uint8_t data1 = 0x00, uint8_t data2 = 0x00)
      : command(command), data_1(data1), data_2(data2) {
    calculateChecksum();
  }

  void calculateChecksum() { checksum = (password_1 + password_2 + command + data_1 + data_2) % 256; }

  std::vector<uint8_t> toBytes() const {
    return {header_1, header_2, password_1, password_2, command, data_1, data_2, checksum};
  }
};

// Specific Requests

class StatusRequest : public Request {
 public:
  StatusRequest() : Request(0x01) {}
};

class SetPowerRequest : public Request {
 public:
  SetPowerRequest(bool enable) : Request(0x03, enable ? 0x01 : 0x00, 0x00) {}
};

class SetTemperatureRequest : public Request {
 public:
  SetTemperatureRequest(uint8_t temperature) : Request(0x04, temperature, 0x00) {}
};

class SetLevelRequest : public Request {
 public:
  SetLevelRequest(uint8_t level) : Request(0x04, level - 1, 0x00) {}
};

class SetRunningModeRequest : public Request {
 public:
  SetRunningModeRequest(uint8_t mode) : Request(0x02, mode, 0x00) {}
};

class SetAutomaticStartStopRequest : public Request {
 public:
  SetAutomaticStartStopRequest(bool enable) : Request(0x13, enable ? 0x01 : 0x00, 0x00) {}
};

class SetLanguageRequest : public Request {
 public:
  SetLanguageRequest(uint8_t language_code) : Request(0x14, language_code, 0x00) {}
};

class SetTemperatureUnitRequest : public Request {
 public:
  SetTemperatureUnitRequest(bool is_celsius) : Request(0x15, is_celsius ? 0x01 : 0x00, 0x00) {}
};

class SetAltitudeUnitRequest : public Request {
 public:
  SetAltitudeUnitRequest(bool is_meters) : Request(0x16, is_meters ? 0x01 : 0x00, 0x00) {}
};

class SetTankVolumeRequest : public Request {
 public:
  SetTankVolumeRequest(uint8_t volume) : Request(0x17, volume, 0x00) {}
};

class SetOilPumpTypeRequest : public Request {
 public:
  SetOilPumpTypeRequest(uint8_t type) : Request(0x18, type, 0x00) {}
};

class SetTemperatureOffsetRequest : public Request {
 public:
  SetTemperatureOffsetRequest(uint8_t offset) : Request(0x20, offset, 0x00) {}
};

}  // namespace diesel_heater_ble
}  // namespace esphome
