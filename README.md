# lts74x
Yet another 74x-series IC logic tester that uses Arduino board for testing the logic states of the IC.

Lts74x was inspired by the following propjects:
* Integrated circuit tester: https://github.com/Johnlon/integrated-circuit-tester
* Smart IC Tester: https://github.com/akshaybaweja/Smart-IC-Tester

Features:
* Uses Arduino Nano and one GPIO Expander IC
* Tests many 16 and 18 pin 74x series ICs, test database is based on the above projects.
* Tests some 20 pin 74x series ICs, requires Arduino Nano modification (LED and pull resistor removals)
* Uses OLED screen for IC selection
* Displays basic IC pinout and indicates failed pins after the test.
* Supports testing Z states on IC pins.
* Supports Clock pin.
* Supports latched pin testing.
* Tests are compressed to fit into Arduino Nano flash (still about ~4 kb free).
* Open source / hardware design (kicad), schematics and gerbers provided.
* Basic Arduino pin protection via 1k Ohm resitor in series (we test ICs that might be broken internally).

More info to come...
