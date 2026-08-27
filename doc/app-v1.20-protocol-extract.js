/*
 * Readable protocol-only extraction from:
 *   airheaterble-1-20.apk
 *   assets/apps/__UNI__1471D01/www/app-service.js
 *
 * APK SHA-256:
 *   c3658487ac7fc0120c98d44d9465b85f2bfec43d440e69fd3c87a4fe7caa53fa
 * app-service.js SHA-256:
 *   cde1c0021fdf21c246408241d867b356f13b09cd3cf69129313f7577614829b2
 *
 * The APK bundle is minified. Names and control flow below were expanded for
 * readability while preserving the operations used by the app. This is an
 * evidence file, not code imported by the ESPHome component.
 */

const RESPONSE_FORMAT = {
  AA55_PLAIN: 1,      // MD1
  AA55_ENCRYPTED: 2,  // MD2
  AA66_PLAIN: 3,      // MD3
  AA66_ENCRYPTED: 4,  // MD4
};

const COMMAND = {
  STATUS: 1,
  MODE: 2,
  POWER: 3,
  SET_VALUE: 4,  // level in mode 1, temperature in mode 2
  SET_CLOCK: 10,
  SET_AUTO_START_TIME: 11,
  SET_AUTO_RUN_TIME: 12,
  SET_AUTO_ENABLED: 13,
  SET_LANGUAGE: 14,
  SET_TEMPERATURE_UNIT: 15,
  SET_TANK_VOLUME: 16,
  SET_OIL_PUMP_OR_RF433: 17,
  SET_AUTOMATIC_HEATING: 18,
  SET_ALTITUDE_UNIT: 19,
  SET_TEMPERATURE_OFFSET: 20,
};

const XOR_KEY = [112, 97, 115, 115, 119, 111, 114, 100]; // "password"

function unsigned16ToSigned(value) {
  if (value > 32767.5) {
    value |= -65536;
  }
  return value;
}

function unsigned8ToSigned(value) {
  if (value > 127.5) {
    value |= -256;
  }
  return value;
}

function clamp(value, minimum, maximum) {
  if (value < minimum) value = minimum;
  if (value > maximum) value = maximum;
  return value;
}

/*
 * Extracted from the didUpdateValue callback. The original app performs no
 * length or checksum validation. Those omissions must not be copied into C++.
 */
function decodeNotification(input, state) {
  const data = Array.from(input);

  if (data[0] === 0xaa && data[1] === 0x55) {
    state.md = RESPONSE_FORMAT.AA55_PLAIN;
    state.rcv_cmd = data[2];
    state.runningstate = data[3];
    state.errcode = data[4];
    state.runningstep = data[5];
    state.altitude = data[6] + 256 * data[7];
    state.runningmode = data[8];

    if (state.runningmode === 1) {
      state.setlevel = data[9];
    } else if (state.runningmode === 2) {
      state.settemp = data[9];
      state.setlevel = data[10] + 1;
    } else if (state.runningmode === 0) {
      state.setlevel = data[10] + 1;
    }

    state.supplyvoltage = (256 * data[12] + data[11]) / 10;
    state.casetemp = unsigned16ToSigned(256 * data[14] + data[13]);
    state.cabtemp = unsigned16ToSigned(256 * data[16] + data[15]);
    state.connected = true;
    return state.md;
  }

  if (data[0] === 0xaa && data[1] === 0x66) {
    state.md = RESPONSE_FORMAT.AA66_PLAIN;
    state.rcv_cmd = data[2];
    state.runningstate = data[3];
    state.errcode = data[17];
    state.runningstep = data[5];
    state.altitude = data[6] + 256 * data[7];
    state.runningmode = data[8];

    if (state.runningmode === 1) {
      state.setlevel = data[9];
    } else if (state.runningmode === 2) {
      state.settemp = data[9];
      state.setlevel = data[10] + 1;
    } else if (state.runningmode === 0) {
      state.setlevel = data[10] + 1;
    }

    state.supplyvoltage = (256 * data[12] + data[11]) / 10;
    state.casetemp = unsigned16ToSigned(256 * data[14] + data[13]);
    state.cabtemp = unsigned16ToSigned(256 * data[16] + data[15]);
    state.connected = true;
    return state.md;
  }

  // The original app always executes six XOR blocks. JavaScript arrays grow
  // when a missing element is assigned, so this alone does not prove that the
  // received encrypted wire frame is exactly 48 bytes.
  for (let block = 0; block < 6; block++) {
    for (let offset = 0; offset < 8; offset++) {
      const index = block * 8 + offset;
      data[index] ^= XOR_KEY[offset];
    }
  }

  if (data[0] !== 0xaa || (data[1] !== 0x55 && data[1] !== 0x66)) {
    return 0;
  }

  state.md = data[1] === 0x55
    ? RESPONSE_FORMAT.AA55_ENCRYPTED
    : RESPONSE_FORMAT.AA66_ENCRYPTED;
  state.rcv_cmd = data[2];
  state.runningstate = data[3];
  state.errcode = state.md === RESPONSE_FORMAT.AA55_ENCRYPTED ? data[4] : data[35];
  state.runningstep = data[5];
  state.altitude = (data[7] + 256 * data[6]) / 10;
  state.runningmode = data[8];
  state.setlevel = clamp(data[10], 1, 10);

  // This deliberately mirrors the app, which consults the previously stored
  // unit before assigning the new unit from byte 27 later in this frame.
  state.settemp = state.tempunit !== 1
    ? clamp(data[9], 8, 36)
    : clamp(data[9], 40, 99);

  state.supplyvoltage = (256 * data[11] + data[12]) / 10;
  state.casetemp = unsigned16ToSigned(256 * data[13] + data[14]);
  state.cabtemp = unsigned16ToSigned(256 * data[32] + data[33]) / 10;
  state.sttime = 256 * data[19] + data[20];
  state.autotime = 256 * data[21] + data[22];
  state.runtime = 256 * data[23] + data[24];
  state.isauto = data[25];
  state.language = clamp(data[26], 0, 5);
  state.tempunit = data[27];
  state.tankvolume = data[28];
  state.oilpumptype = data[29];
  if (data[29] === 20) state.rf433onoff = 0;
  if (data[29] === 21) state.rf433onoff = 1;
  state.altiunit = data[30];
  state.automaticheating = data[31];
  state.tempoffset = unsigned8ToSigned(data[34]);
  state.connected = true;
  return state.md;
}

/*
 * This is the shared BLE object's request encoder. A page-local connection
 * encoder constructs the same eight bytes. Neither branches on MD1-MD4. The
 * passkey is a decimal integer from 1 through 9999, split into two base-100
 * fields. Command data is a 16-bit little-endian value.
 */
function buildRequest(passkey, command, value) {
  const frame = new Uint8Array(8);
  frame[0] = 0xaa;
  frame[1] = 0x55;
  frame[2] = Math.trunc(passkey / 100);
  frame[3] = passkey % 100;
  frame[4] = command;
  frame[5] = value % 256;
  frame[6] = value >> 8;
  frame[7] = frame[2] + frame[3] + frame[4] + frame[5] + frame[6];
  return frame;
}

/*
 * The main and parameter pages send one request per one-second interval.
 * Command 1 is the idle/status poll. If the most recent response command does
 * not match the last command, the app retransmits that command on the next
 * interval. A production implementation should bound retries.
 *
 * Core commands 1-4 and clock command 10 are used independently of MD1-MD4.
 * The parameter page exposes commands 11-20 only when md is 2 or 4 (the two
 * encrypted response formats).
 */
