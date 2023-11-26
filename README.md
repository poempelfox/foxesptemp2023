# foxesptemp2023

The goal of this project is to build a ESP32 based version of my previous AVR based FoxTemp devices, like e.g. [FoxTemp2016](https://gitlab.cs.fau.de/PoempelFox/foxtemp2016) and [FoxTemp2022](https://gitlab.cs.fau.de/PoempelFox/foxtemp2022).

It uses the ESPs normal WiFi instead of transmitting values on 868 MHz to a special receiver.

For very obvious reasons, it will use an order of magnitude more power than the AVR based FoxTemps - it is not realistic to power this from batteries.

On the plus side, this does have a webinterface, and it should be quite flexible with regard to what sensors you connect to what pin. It can also submit values to servers on the internet on its own.
