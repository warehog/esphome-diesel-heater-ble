## ESPHome integration for bluetooth diesel heater

![alt text](image.png)

This repository contains an implementation of esphome external component, designed to controll bluettoth enabled diesel heaters.

# Part one:  Why?
I bought one of those bluetooth enabled heaters for my caravan, to replace original Truma heater. It works quite well, but it's not what I hoped for in terms of output power.
Or, perhaps not the power, but overall performance of heating a 7 meter long caravan. 
Soon, I realized that it's not a power issue, but the time needed to get into a comfortable temperature zone. Going from -5 to +20 takes time. A lot of time. Not only an air must bea heated, but everything else (furniture, utilities, floor etc.)

This pushed me into looking for a solution of remote controlling it. I wanted to start heating few hours before planned departure, without needd to be around all the time. 

Since I was already familiar with ESPHome, it went as a first choice, and you are looking on the effect of my efforts.

## How can it be remote controlled?
This is something I did realized way too late into the process. and this fact, is this:
> There is no single (Chineese) Bluetooth Diesel Heater!

It's not an obvious thing, since there is a lot of desingns og those, but they all do the same thing. Right? Or, I thout it is that way.
In reality, thys do are different from each other, not only in outer design, but in the communication protocols used.

```
|--------                   |------------------|                  |----------------|
|       |        BLE        |       HD         |  UART / Custom   |       CB       |
|  APP  | < ------------ >  |  Heater Display  | < ------------ > | Controll Board |
|       |           ---- >  |                  |                  |                |
|--------           |       |------------------|                  |----------------|
                    |
|-------------|     | 443 MHz
|             |     |
|  RF Remote  | < ---
|             |
|-------------|              
```

| Method                                                      | Pros                                                                                                     | Cons                                                                                                  |
|-------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------|
| Use BLE to communicate with HD | Leverages standard BLE; less intrisive, and gives full potential | Might be the hardest to implement, requires BLE device, not available on all heaters |
| Use RF commands to control HD | Quite simple, either by tapping into original remote buttons (siumulate clicks) or use some RF Bridge | Enables controll without feedback; not possible to read state, temperature etc          |
| Hack UART: Replace original HD and talk with CB directly    | Unlocks all features of heater, with much grated controll over original display| Invasive hardware modification; no backup controll when adapter fails |
| Hack UART: Intercept existing HD-CB communication | Non-invasive integration preserving original hardware; can leverage existing communication setup | Complex firmware timing; risk of unstable behavior|

Exact way on how you should controll it, depends on the use case and hardware compability. 


# Part two: Doesn't it already been been done?

Many attempts has been made to control these devices, but none of them seemed to fit my needs.<br>
Some solutions, like: 
* standalone [iotmaestro/vevor-heater-ble](https://github.com/iotmaestro/vevor-heater-ble)  
* ESPHome without fully integrating with it [spin877/Bruciatore_BLE](https://github.com/spin877/Bruciatore_BLE/blob/main/ESP32-VevorBLE.yaml).
* ...and few others there is no point to list.

I have gone through half of the internet looking for right tool.<br>
In many places, people were menitoning a procedure of communication with those using BLE and it seemed so easy to do. <br>
Follow those steps:
* Connect to BLE deice iwth MAC
* Register for notification
* Send some predefined string to get status and parse it (status, temperature, voltage (...))
* Send some predefined string to power on / power off / level up / level down/ change mode

Easy, right?

As you are reading this, you might suspect that it was, indeed, not easy.<br>
I have left a C/C++ world many years ago, in favor of, like, everything else, but when the need calls, what could I do?<br>

And so I did.

# Part three: How it can(not) be done?

What I learned through years of professional experience, is that every problem is solvable.<br>
If it happend to seem unsolvable, you don't need to just spend more time on it - it mean that you are doing it wrong.
That's way some peaple seems to do maginc things so easily - and it's not beacuse they are pros, but beacuse they thought about the problem from a perspective no one else has thought before.

Initially what I wanted, was a simple solution to a simple problem. Just send some string, read the response, send another string, read it again. blah blah.
Non ot it worked. I only got some unparsable giberish, that didn't made any sense. 

Second thoutght was to see how the official APP ccommunucates with the heater. I spend way to much time, setting up a BLE sniffer. I even attempted to use Android simulator with the app inside, and Wireshark on host for packet capture. And you know what? IT DIDNT WORK.

### Wondering what I'm doing wroing
At that point I was wondering, why is it so complex? How did I came here? I only want to send a command and read the response, how much is it to expect?<br>
Then this idea has stiked me, that I'm doing it wrong. I wanted to have a simple solution, so I was focused on wanting a simple solution, while the problem I was solving was way bigger.

# Part four: Taking another angle

At some point in the whole process I realised that, perhaps, the solution was in fron of me all the time. And it was the AirHeaterBLE app itself. 

I tried to use it for packet sniffing, but hand't realized that I can just.. look inside it?

### Decompiling JAVA code
It turned out, that you can preety much decompile any APK (standard Android packaged application installer), back into a source code. There is many techniques for different languages, with varying results between them.<br>
In Java world, it works quite well - you get almost original soirce code. There are some cases, when code is encrypted, but fortunatelly, it was not here.

After few hours, trygin different decompilers and some other tools, I knew that there is some part of the code that I'm missing. There was no single part where any heater controll code was included.

And then it happended.

**IT WAS NOT JAVA**.

**IT WAS JAVASCRIPT.**

The answer to my problem was all the time, lying in front of me.





So, I delved deep into reverse-engineering the entire protocol, and this code is the result of that effort.

## What has been done?
- Created the `diesel_heater_ble` component, which implements the BLE protocol to communicate with the heater controller board.
- Added sensors for all possible settings retrieved from the controller.
- Introduced switches, buttons, and number controls for the most significant features, including:
  - Power switch
  - Level-up button
  - Level-down button
  - Temperature-up button
  - Temperature-down button
  - Level set (number)
  - Temperature set (number)
- Enabled extensibility for this component to support future versions of controllers.

## Key Insights
One of the major discoveries was that there is no single protocol for communicating with all BLE-enabled controllers. Most existing documentation, repositories, and code focus on one of at least four protocol versions (apparently the oldest one).

## Known Protocols
Protocols are identified by the first two bytes of the response:
- **0xAA 0x55**: The most basic protocol with a 20-byte response frame.
- **0xAA 0x66**: Very similar to the previous one, with a slightly different order of bytes in the response.
- **0xAA 0x55 (encrypted)**: A newer version with a 48-byte encrypted data frame.
- **0xAA 0x66 (encrypted)**: Another newer version with a 48-byte encrypted data frame and altered byte order.

Each protocol uses slightly different formats for requests and responses. From the controller’s perspective, incoming data (requests from the mobile app) is "plain text," while responses are encrypted only in the most recent protocol versions.

### IMPORTANT: Current Implementation
This code implements parsing for all protocol versions’ responses but can only send commands using the **0xAA 0x55 (encrypted)** protocol.  
I own a heater that uses this third protocol version, so it is fully implemented in this PR. For other protocols, I would appreciate assistance from owners of devices using those versions.

## Framework Selection
The BLE stack takes up a significant amount of flash memory. Together with several sensors, buttons, etc., I exceeded the 4MB flash capacity of my ESP32 Lolin Lite.  
I discovered that using the ESP-IDF framework results in a smaller binary compared to the Arduino framework. Therefore, unless you have an 8MB board, ESP-IDF is the only viable solution.

## TODO
- Implement `HeaterController_*` classes to support other types of controllers.
- Add a climate component for unification.
- Refactor the code to allow configurations without buttons, switches, or numbers. Currently, if no button/switch/sensor/number is defined in the YAML, the C++ compiler throws an error about missing headers (e.g., when no button is configured, `esphome/components/button/button.h` cannot be found).
