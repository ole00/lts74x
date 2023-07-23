# lts74x
Yet another 74x-series IC logic tester that uses Arduino board for testing the logic states of the IC.

![Board image](https://github.com/ole00/lts74x/raw/master/img/lts74x_top.jpg "lts74x")

Lts74x was inspired by the following propjects:
* Integrated circuit tester: https://github.com/Johnlon/integrated-circuit-tester
* Smart IC Tester: https://github.com/akshaybaweja/Smart-IC-Tester

Features:
* Uses Arduino Nano and one GPIO Expander IC
* Tests many 16 and 18 pin 74x series ICs, test database is based on the above projects.
* Tests some 20 pin 74x series ICs, requires Arduino Nano modification (LED and pull resistor removals)
* Uses OLED screen for IC selection
* Displays basic IC pinout and indicates failed pins after the test.
* Screen saver to prevent OLED screen burn.
* Supports testing Z states on IC pins.
* Supports Clock pin.
* Supports latched pin testing.
* Tests are compressed to fit into Arduino Nano flash (still about ~4 kb free).
* Open source / hardware design (kicad), schematics and gerbers provided.
* Basic Arduino pin protection via 1k Ohm resitor in series (we test ICs that might be broken internally).

Setup:
-----------
 * Upload the lts74x20.ino sketch to your Arduino Nano. Use Arduino IDE to upload the sketch, both IDE version 1.8.X and 2.X should work fine.
   Note that some USB-C based Arduino Nano clones require to select Tools-> Processor ->ATMegae328P (Old Bootloader) to flash them.
 * Decide whether you want/need the 20pin IC test feature. If so, modify the Arduino board - see Arduino mod in Q&A section.
 * Build the lts74x hardware. Buy the PCB from an online PCB production service (use provided gerber archive in 'gerbers' directory).
   Then solder the components on the PCB - check the schematic (lts74x.pdf) and bom.txt for parts list.


How lts74x works:
------------------
* The IC tester works on it own, that means it does not need a PC serial connection in order to test the IC chips.
* After start, the IC list is displayed on the screen and you can select the IC you want to test by pressing
 Up and Down buttons. Selection is confirmed by the OK button. You can also check the basic pinout of the device
 by pressing the Back button.
* Pressing the OK button starts the IC testing (the switch powering the IC must be switched on - the app dectects
 that and asks you to flip the switch if the switch is Off). Of course, you should insert the actual IC in the ZIF
 socket beforehand to get meaningful test results. Insert the IC while the power switch is Off (which prevents accidental zapping of
 the IC during insertion), then you can turn the Power switch On and start the test.

* Each IC has its own set of tests stored in the flash. Upon pressing OK, the test set is loaded, decoded and then run 100 times.
  Note that the test set is comprised of several individual tests that check the different input pin combinations (High, Low inputs)
  and output results (High, Low, Z state outputs). The whole testing set is run and if there is an error the IC pinout with
  failed pins is displayed on the screen.

* Each test is defined as a text string where each character in the string corresponds to a certain pin state. Pins marked as 0 or 1
  are the input pins and their value is set via Arduino to the desired value (Low or High repsectively). Pins marked
  as L, H or Z are the input pins and their value is read by Arduino and evaluated. L marked pins must read digital 0, H marked
  pins must read digital 1 from the correspondig GPIO. Z marked pins test the high impedance output (see Q&A section for details) 
  of the IC chip.
  
PCB:
-----------
 The most convenient way to get the PCB is to order it online on jlcpcb.com, pcbway.com, allpcb.com or other online services. 
 Use the zip archive stored in the gerbers directory and upload it to the manufacturer's site of your choice. 
 Upload the lts74x_r3_fab.zip and set the following parameters (if required).

  * Dimensions are 85x68 mm
  * 2 layer board
  * PCB Thickness: 1.6, or 1.2 mm
  * Copper Weight: 1
  * The rest of the options can stay default or choose whatever you fancy (colors, finish etc.)


Soldering/assembly steps:
--------------------------
  * start with the smallest parts, solder the resistors and capacitors.
  * Use multimeter and verify the resistance between Arduino pins and GND (Test point TP1). It should be ~ 470kOhm.
  * Use multimeter and verify the resistance between Arduino pins the corresponding ZIF socket pin - It should be 1kOhm. 
  * To do the above, check the schematics PDF to find out which Arduino pins lead to which ZIF socket pin.
  * solder the U1 IC. 
  * solder the  Q1 and Q2 Mosfets. Note that there is a dual footprint for the Mosfets, use either Through-Hole footrpint or SMT fotrpint, but not
    both at the same time.  Important: use low soldering temeprature (no more that 280 C)
    to prevent the Mosfet damage. While soldering, touch the Mosfets pins for only a very short time (2 seconds max). If you need
    to improve the soldering joint, let the Mosfet to cool down a bit, apply a flux on the pin and then briefly touch upon the pin with the soldering tip.
    I managed to burn 2 Mosfets while doing so with higher temperature. To test the Mosfet is OK use multimeter and test resistance between Ground pin
    (Source) and the Drain pin while the board in powered off. The resistance value must be ~ 470kOhm. If it is way less than that (like 2kOhm) 
    the Mosfet gate was damaged during soldering.
  * Solder the LED
  * Solder the power switch
  * Test your OLED display on a breadboard connected to Arduino and a test sketch whether the display actually works.
    I got an LCD delivered which was faulty, so I had to use another one. Returning a faulty OLED display to the seller might be problematic
    if there are traces of soldering on the display. While running the OLED test, check which I2C address it uses.
  * Solder the OLED display. Before you do that, use M2 bolt and nuts to secure the display to the PCB. Use extra M2 nuts in between the OLED display
    and and the tester PCB as a spacer.
  * Solder the buttons
  * Optionally, solder the Arduino female header socket on the tester PCB if you want to have your Arduino Nano detachable.
  * Optionally, mod your Arduino Nano to support 20 pin IC testing capability (see Q&A section).
  * Verify the 5V pin and Ground is not shorted - use multimeter and continuity test to do that. Very short initial beep might be OK as that
    might be the decoupling capacitors letting through the probing current.
  * Insert/solder the Arduino Uno.
  * Power the Board on by plugging-in the USB cable. Test the basic functionality (buttons, power switch and LED, test an IC with no IC inserted/used).
    If they do not work, check the troubleshooting.

  * Solder the ZIF socket.

Q&A:
---------------
* I can't upload the sketch to my my Arduino Nano clone because of a communication error
   - Nano clones often use CH340 serial chip (or in my case, a clone of the CH340 chip). When using Windows or Mac,
     make sure you have the CH340 serial driver installed. See : https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all
   - Ensure Arduino Nano is selected in the Arduino IDE: Tools -> Board: Arduino Nano
   - Try setting the Processor option Old Bootloader: Tools-> Processor ->ATMega328P (Old Bootloader)

* I turned it on and the OLED display does not show anything
   - Ensure lts74x20.ino sketch was uploaded to your Arduino Nano.
   - Did you do the OLED test as described in Assembly steps? Check the OLED screen communicates via I2C 
     by running I2C scanner sketch: File -> Examples -> Wire -> i2c_scanner. It should find 2 devices:
     one is the GPIO Expander address 0x18 and the second one is the OLED screen using address 0x78.
     Modify OLED_ADDRESS value in lts74x20.ino sketch if your OLED address is different.



* what is the On switch used for? When do I use it?

    - Normally, the switch should be in the Off position (LED is not lit). Also when insering the IC chip or when removing the IC chip
      from the ZIF socket the switch should be Off. Just before you start the test the switch should be turned On.
      So in general: insert the IC when the switch is Off, then turn the switch On and run the test. When finished turn the switch Off,
      and remove the IC chip from the socket.


* Itested a branch new IC chip and it fails during the test. Some pins (or a single pin) indicate a failure.
    - Double check all resistors leading to the ZIF socket pins (1 kOhm) and weak pull down resistors (470 kOhm) are well soldered.
     There might be a missing solder joint on that particular pin or on a pin that contributes to the logical output of the failed pin.
     Test the resistance by attaching multimeter probes to an Arduin pin and corresponding ZIF socket pin, and also between
     the Arduino pin and ground (TP1).

* The site image on the very top shows some bodge wires connected to U1 - shall I be concerned?
  - No, there is no need to replicate that on your board. The image shows a prototype board (Revision 2) that has a bug which was fixed in 
    board Revision 3. Rev. 3 is currently the one available for download.
  
* What is the mod to let me test 20 pin ICs?
    - See image bellow. Basically, remove few pull resistors by touching them with a hot soldering iron (~350C) for 4 seconds and then
      while pushing them a bit sideways they will slide off of its footprints on the PCB.
      **Important:** before doing the mod make sure the Arduino sketch is uploaded. After the mod is done, it won't be possible to upload the sketch
      via USB. Uploading will be possible, but it is less convenient and requires another Arduino board.
    - remove the RX and TX resistors or LEDs on the top. 
    - remove the L resistor or LED on the top
    - remove the resistors on the bottom of the board. Please note that my Arduino nano is a clone, it does not even use a genuine CH340
       serial chip. If you have a different Arduino Nano board that the one pictured bellow the you may need to remove other resistors
       or it might be easier to remove the serial chip itself.
    - solder headers to allow sketch reprogramming in the future

* How is the Z state stested?
    - The Z - or High impedance - state test is based on 2 facts: 
    - The lts74x circuit has very weak external pull down resistors (470k) on all ZIF pins. During testing of High or Low states this pull down resistor
      does not affect the results read from the pins, because IC currents are much stronger.
    - The IC pin in Z state should not source or sink any current from the wire connected to the pin (if it does, it is an error).
     Therefore the pin's current (if there is any teeny tiny one) should not have enough 'strength' to override the actual state of the wire
     leading to the pin. So, if the Arduino sets a pin leading to the IC's Z pin as Input with internall Pull up (~30k), then Arduino should 
     read it as High. Also, if Arduino sets a pin leading to the IC's Z pin as Input (external pull down, see above) then Arduino should
     read it as Low. If the reeading is incorrect then the IC's pin is not in the Z state as it 'overrides' the pull resistors by sinking or 
     sourcing the current. That means it's not in Z state and the pin the pin is malfunctioning or is damaged. The Z test does exactly that:
     sets the pin as Input with internal pull up and reads the value, then sets it as Input with external pull down and reads the value.
     If the read values are correct the pin is in Z state. There are few more details involved in the actuall Z test (like there must be a small
     delay allowing the pull resistor to react before reading the value etc.), so check the source code if you are interested.
   
