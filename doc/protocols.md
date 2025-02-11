# Diesel Heater Bluetooth Protocol Documentation

This document describes the four message formats (protocols) used by the diesel heater over Bluetooth. Two protocols are received unencrypted (MD1 and MD3) and two are received encrypted (MD2 and MD4). In the encrypted messages the payload is divided into 6 blocks of 8 bytes which are decrypted by XOR-ing each byte with the corresponding byte in the string `"password"` (i.e. with the bytes `[112, 97, 115, 115, 119, 111, 114, 100]`).

---

## Overview of Protocols

- **MD1**: Unencrypted message with header `[170, 85]` (hex: `0xAA, 0x55`).  
- **MD3**: Unencrypted message with header `[170, 102]` (hex: `0xAA, 0x66`).  
- **MD2**: Encrypted message that, after decryption, has header `[170, 85]`.  
- **MD4**: Encrypted message that, after decryption, has header `[170, 102]`.

---

## MD1 Protocol (Unencrypted, Header: `[170, 85]`)

**Message Length:** ~17 bytes

| Byte Index | Field Name    | Calculation / Description                                                                                       | Notes |
|------------|---------------|-----------------------------------------------------------------------------------------------------------------|-------|
| 0          | **Header**    | Fixed value `170` (0xAA)                                                                                         |       |
| 1          | **Header**    | Fixed value `85` (0x55)                                                                                          |       |
| 2          | `rcv_cmd`     | Raw command code (byte value)                                                                                    |       |
| 3          | `runningstate`| Unsigned integer (raw byte)                                                                                      |       |
| 4          | `errcode`     | Unsigned integer (raw byte)                                                                                      |       |
| 5          | `runningstep` | Unsigned integer (raw byte)                                                                                      |       |
| 6–7        | `altitude`    | `altitude = dataBytes[6] + 256 * dataBytes[7]`                                                                   | (Unit not scaled) |
| 8          | `runningmode` | Unsigned integer (raw byte). Possible values typically include **0**, **1**, or **2**                              |       |
| 9 / 10     | `settemp`/`setlevel` | Depending on `runningmode`: <br> - If **runningmode == 1**: <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 9 → `setlevel` <br> - If **runningmode == 2**: <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 9 → `settemp` <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 10 → `setlevel` (add 1) <br> - If **runningmode == 0**: <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 10 → `setlevel` (add 1) | The addition of 1 is applied after reading the byte value |
| 11–12     | `supplyvoltage` | Calculated as: <br> `voltage = (256 * dataBytes[12] + dataBytes[11]) / 10` <br> *(Note the reversed order)*  | Result is fixed to 1 decimal place |
| 13–14     | `casetemp`    | `casetemp = UnsignToSign(256 * dataBytes[14] + dataBytes[13])`                                                  | Conversion function applied |
| 15–16     | `cabtemp`     | `cabtemp = UnsignToSign(256 * dataBytes[16] + dataBytes[15])`                                                   | Conversion function applied |

---

## MD3 Protocol (Unencrypted, Header: `[170, 102]`)

**Message Length:** ~18 bytes

| Byte Index | Field Name    | Calculation / Description                                                                                        | Notes |
|------------|---------------|------------------------------------------------------------------------------------------------------------------|-------|
| 0          | **Header**    | Fixed value `170` (0xAA)                                                                                          |       |
| 1          | **Header**    | Fixed value `102` (0x66)                                                                                          |       |
| 2          | `rcv_cmd`     | Raw command code (byte value)                                                                                     |       |
| 3          | `runningstate`| Unsigned integer (raw byte)                                                                                       |       |
| 17         | `errcode`     | Unsigned integer (raw byte)                                                                                       | *Note: In MD3, `errcode` is at index 17 (different from MD1)* |
| 5          | `runningstep` | Unsigned integer (raw byte)                                                                                       |       |
| 6–7        | `altitude`    | `altitude = dataBytes[6] + 256 * dataBytes[7]`                                                                    |       |
| 8          | `runningmode` | Unsigned integer (raw byte)                                                                                       |       |
| 9 / 10     | `settemp`/`setlevel` | Depends on `runningmode`: <br> - If **runningmode == 1**: <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 9 → `setlevel` <br> - If **runningmode == 2**: <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 9 → `settemp` <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 10 → `setlevel` (add 1) <br> - If **runningmode == 0**: <br>&nbsp;&nbsp;&nbsp;&nbsp;Byte 10 → `setlevel` (add 1) | Same behavior as MD1 |
| 11–12     | `supplyvoltage` | `voltage = (256 * dataBytes[12] + dataBytes[11]) / 10`                                                         | Fixed to 1 decimal place |
| 13–14     | `casetemp`    | `casetemp = UnsignToSign(256 * dataBytes[14] + dataBytes[13])`                                                     | Conversion function applied |
| 15–16     | `cabtemp`     | `cabtemp = UnsignToSign(256 * dataBytes[16] + dataBytes[15])`                                                      | Conversion function applied |

---

## MD2 Protocol (Encrypted, Decrypted Header: `[170, 85]`)

**Message Length:** ~35 bytes (bytes with indexes **0–34** are used)

> **Decryption:**  
> For each block of 8 bytes (total 6 blocks), perform:  
> ```
> for (j = 0; j < 6; j++) {
>   baseIndex = 8 * j;
>   dataBytes[baseIndex]     ^= 112; // 'p'
>   dataBytes[baseIndex + 1] ^= 97;  // 'a'
>   dataBytes[baseIndex + 2] ^= 115; // 's'
>   dataBytes[baseIndex + 3] ^= 115; // 's'
>   dataBytes[baseIndex + 4] ^= 119; // 'w'
>   dataBytes[baseIndex + 5] ^= 111; // 'o'
>   dataBytes[baseIndex + 6] ^= 114; // 'r'
>   dataBytes[baseIndex + 7] ^= 100; // 'd'
> }
> ```

| Byte Index | Field Name       | Calculation / Description                                                                                      | Notes |
|------------|------------------|---------------------------------------------------------------------------------------------------------------|-------|
| 0          | **Header**       | Fixed value `170` (0xAA)                                                                                       |       |
| 1          | **Header**       | Fixed value `85` (0x55)                                                                                        |       |
| 2          | `rcv_cmd`        | Raw command code (byte value)                                                                                  |       |
| 3          | `runningstate`   | Unsigned integer (raw byte)                                                                                    |       |
| 4          | `errcode`        | Unsigned integer (raw byte)                                                                                    |       |
| 5          | `runningstep`    | Unsigned integer (raw byte)                                                                                    |       |
| 6–7        | `altitude`       | `altitude = (dataBytes[7] + 256 * dataBytes[6]) / 10`                                                          | *Note:* The order is swapped and the value is scaled (divided by 10) |
| 8          | `runningmode`    | Unsigned integer (raw byte)                                                                                    |       |
| 9          | `settemp`        | Read from byte 9 then clamped: <br> - If `tempunit` ≠ 1 → clamp between **8** and **36** <br> - If `tempunit` = 1 → clamp between **40** and **99** |       |
| 10         | `setlevel`       | Read from byte 10 then clamped between **1** and **10**                                                        |       |
| 11–12     | `supplyvoltage`  | `voltage = (256 * dataBytes[11] + dataBytes[12]) / 10`                                                          |       |
| 13–14     | `casetemp`       | `casetemp = UnsignToSign(256 * dataBytes[13] + dataBytes[14])`                                                   |       |
| 19–20     | `sttime`         | `sttime = 256 * dataBytes[19] + dataBytes[20]`                                                                   |       |
| 21–22     | `autotime`       | `autotime = 256 * dataBytes[21] + dataBytes[22]`                                                                 |       |
| 23–24     | `runtime`        | `runtime = 256 * dataBytes[23] + dataBytes[24]`                                                                  |       |
| 25         | `isauto`         | Unsigned integer (raw byte)                                                                                    |       |
| 26         | `language`       | Read from byte 26 and clamped between **0** and **5**                                                            |       |
| 27         | `tempunit`       | Unsigned integer (raw byte)                                                                                    |       |
| 28         | `tankvolume`     | Unsigned integer (raw byte)                                                                                    |       |
| 29         | `oilpumptype`    | Unsigned integer (raw byte). Also used to set `rf433onoff`: <br> If value is **20** then off; if **21** then on   |       |
| 30         | `altiunit`       | Unsigned integer (raw byte)                                                                                    |       |
| 31         | `automaticheating`| Unsigned integer (raw byte)                                                                                    |       |
| 32–33     | `cabtemp`        | `cabtemp = UnsignToSign(256 * dataBytes[32] + dataBytes[33]) / 10`                                               | Division by 10 applied |
| 34         | `tempoffset`     | `tempoffset = UnsignToSign8(dataBytes[34])`                                                                      | Conversion function applied |

---

## MD4 Protocol (Encrypted, Decrypted Header: `[170, 102]`)

**Message Length:** ~36 bytes (bytes with indexes **0–35** are used)

> **Decryption:**  
> The same decryption procedure as for MD2 applies (XOR each 8-byte block with `"password"`).

| Byte Index | Field Name       | Calculation / Description                                                                                      | Notes |
|------------|------------------|---------------------------------------------------------------------------------------------------------------|-------|
| 0          | **Header**       | Fixed value `170` (0xAA)                                                                                       |       |
| 1          | **Header**       | Fixed value `102` (0x66)                                                                                       |       |
| 2          | `rcv_cmd`        | Raw command code (byte value)                                                                                  |       |
| 3          | `runningstate`   | Unsigned integer (raw byte)                                                                                    |       |
| 35         | `errcode`        | Unsigned integer (raw byte)                                                                                    | *Note:* In MD4, `errcode` is located at index **35** (instead of index 4 as in MD2) |
| 5          | `runningstep`    | Unsigned integer (raw byte)                                                                                    |       |
| 6–7        | `altitude`       | `altitude = (dataBytes[7] + 256 * dataBytes[6]) / 10`                                                          |       |
| 8          | `runningmode`    | Unsigned integer (raw byte)                                                                                    |       |
| 9          | `settemp`        | Read from byte 9 then clamped (see MD2 for clamping rules)                                                     |       |
| 10         | `setlevel`       | Read from byte 10 then clamped between **1** and **10**                                                        |       |
| 11–12     | `supplyvoltage`  | `voltage = (256 * dataBytes[11] + dataBytes[12]) / 10`                                                          |       |
| 13–14     | `casetemp`       | `casetemp = UnsignToSign(256 * dataBytes[13] + dataBytes[14])`                                                   |       |
| 19–20     | `sttime`         | `sttime = 256 * dataBytes[19] + dataBytes[20]`                                                                   |       |
| 21–22     | `autotime`       | `autotime = 256 * dataBytes[21] + dataBytes[22]`                                                                 |       |
| 23–24     | `runtime`        | `runtime = 256 * dataBytes[23] + dataBytes[24]`                                                                  |       |
| 25         | `isauto`         | Unsigned integer (raw byte)                                                                                    |       |
| 26         | `language`       | Read from byte 26 and clamped between **0** and **5**                                                            |       |
| 27         | `tempunit`       | Unsigned integer (raw byte)                                                                                    |       |
| 28         | `tankvolume`     | Unsigned integer (raw byte)                                                                                    |       |
| 29         | `oilpumptype`    | Unsigned integer (raw byte). Also used for `rf433onoff` determination (20 → off, 21 → on)                      |       |
| 30         | `altiunit`       | Unsigned integer (raw byte)                                                                                    |       |
| 31         | `automaticheating`| Unsigned integer (raw byte)                                                                                    |       |
| 32–33     | `cabtemp`        | `cabtemp = UnsignToSign(256 * dataBytes[32] + dataBytes[33]) / 10`                                               | Division by 10 applied |
| 34         | `tempoffset`     | `tempoffset = UnsignToSign8(dataBytes[34])`                                                                      | Conversion function applied |
