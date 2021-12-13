# sonoffswitch
ESP32/Arduino sketch to control an eWeLink/Sonoff 4CH switch in local LAN mode

I bought a 4CH switch with nice housing @ https://www.amazon.de/gp/product/B08RSBR6C4 for flashing with Tasmota but qhickly learned that there are no accessible RX TX pins on the board inside and OTA flashing wouldn't work either. Looking for possibilities to control the switch without having to use the eWeLink app I found that there exist few Smarthome plugins claiming to be able to control it locally via LAN mode.

As I needed a standalone solution for my project I extracted the essential information from the Home Assistant Sonoff plugin (https://github.com/AlexxIT/SonoffLAN) and converted it into C for the ESP32.

