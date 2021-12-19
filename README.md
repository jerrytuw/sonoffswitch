# sonoffswitch
ESP32/Arduino sketch to control eWeLink/Sonoff 4CH switches in local LAN mode

I bought a 4CH switch with nice housing @ https://www.amazon.de/gp/product/B08RSBR6C4 for flashing with Tasmota but quickly learned that there are no accessible RX TX pins on the board inside and OTA flashing wouldn't work either. Looking for possibilities to control the switch without having to use the eWeLink app I found that there exist few Smarthome plugins claiming to be able to control it locally via LAN mode.

As I needed a standalone solution for my project I extracted the essential information from the Home Assistant Sonoff plugin (https://github.com/AlexxIT/SonoffLAN) and converted it into C for the ESP32.

The sketch first searches for devices via mDNs (to find IP addresses), then takes input in the form of 2 characters: 1st is channel 1-4, second is 0 or 1 for the channel state (send with enter).

In the devices.h file the WIFI credentials and pairs of deviceId and deviceKey are looked up. Note: see https://github.com/AlexxIT/SonoffLAN#getting-devicekey-manually on how to get the device info.
 
   Essential function:
   int doswitch (int deviceNo, int channel, int function)
      deviceNo: 0...number of devices in devices.h
      channel: 1...4
      function: 0 for off, 1 for on
      -> returns http response code

Provided as is.
