BC-250 LED Controller
=====================
&copy; 2026 by Tony Fabris

A small circuit to control three LEDs on the front of my BC-250 case. I am using this circuit to make a 3D-printed translucent Steam logo badge (on the front of my BC-250 case) to glow different colors depending on its power state.

| ![](../Photos/Badge%20On.jpg)                | ![](../Photos/Badge%20Standby.jpg)           |
|:--------------------------------------------:|:--------------------------------------------:|
| ![](../Photos/Badge%20with%20Board%2001.jpg) | ![](../Photos/Badge%20with%20Board%2002.jpg) |


Project Links
-------------
- [Main Project Details](      ../README.md)
- [LED Controller](            ../LED%20Controller/README.md)
- [Steam Puck Controller Wake](../Steam%20Puck%20Controller%20Wake/README.md)
- [Case](                      ../Case/README.md)


Table of Contents
-----------------
- [Behavior](                         #behavior)
- [Buttons](                          #buttons)
- [Schematic](                        #schematic)
- [Components](                       #components)
- [Construction and Assembly Notes](  #construction-and-assembly-notes)


Behavior
--------

This circuit turns on and off three 5mm LEDs to indicate the power states of a BC-250 board.

This circuit gets its information from the pins LED1 and LED2 on the J2000 connector on the BC-250 board. These pins mimic the behavior of a small bicolor red/green LED on the board's backplane between the Ethernet and USB connectors. More details about these pins can be [found here](https://elektricm.github.io/amd-bc250-docs/hardware/pinouts/#j2000-and-j2001).

It is important not to try to drive any LEDs directly off of the LED1/LED2 pins, because they are signal pins, not power supply pins. They "seem" to work if you connect LEDs to them, but that drives an internal transistor circuit on the BC-250 that wasn't designed to power an LED.

My LED controller circuit prevents damage to the BC-250 by allowing my front panel LEDs to get their power from the ATX power supply instead of the LED1/LED2 pins themselves; it uses those pins only for information signaling.

LEDs in this circuit:

  - Yellow LED: Indicates that there is power on the ATX connector.
  - Blue LED 1: Indicates that the BC-250 board is on.
  - Blue LED 2: Indicates that the BC-250 board is experiencing high activity.

Notes:

  - The Yellow LED will turn off when either of the blue LEDs is turned on.
  - Blue LED 1 will stay on when blue LED 2 is lit, so that both LEDs glow when there is high activity, making the logo badge glow brighter.
  - I'm using the yellow LED color with the intention of making it look like a sleep/standby mode. The BC-250 does not have any sleep/standby circuitry, it can only power on and off. But by leaving the ATX power supply in always-on mode, this allows the badge to keep glowing when the BC-250 is shut down. When paired with my [controller wake circuit](../Steam%20Puck%20Controller%20Wake/README.md), it makes the BC-250 seem more like a game console.

Buttons
-------

There are also buttons under the translucent Steam logo badge. The buttons are wired in parallel so that they both do the same thing. You can press on the badge, and it acts as a power on/off button. Note that these buttons are not actually connected to the LED circuit itself, they are separately connected to the BC-250's power button pins. But they're on the same perfboard as the LEDs, so they're included in this schematic.


Schematic
---------

Refer to the accompanying schematic file (open it in Kicad):

  - "BC-250 LED Controller.kicad_sch"

Components
----------

#### BC-250 board (not on schematic):

  - Obtained from Ebay.
  - Information: https://elektricm.github.io/amd-bc250-docs

#### ATX or equivalent power supply (not on schematic):

  - An ATX power supply with a PCIe power output connector, capable of delivering a peak power of 300-500 watts. Can be a full ATX, Mini-ATX or FlexATX.
  - (Example:) https://www.amazon.com/dp/B0FBX9VS3B

#### J2000:

  - 8-pin Molex Micro-fit connector 44769-0801.
  - Connector which allows tapping into the pins LED1 and LED2 on the J2000 connector on the BC-250 board.
  - (Example:) https://www.digikey.com/en/products/detail/molex/0447690801/513218

    | ![](../Photos/J2000%20Connector%20In%20Case.jpg) **J2000 Connection**|
    |:--------------------------------------------------------------------:|

#### ATX_Connector:

  - ATX 24-pin socket connector.
  - Connector which allows connecting this circuit to the standard 24-pin motherboard cable on an ATX power supply.
  - (Example:) https://www.sparkfun.com/atx-power-supply-connector-right-angle.html

#### JP1:

  - A wire which permanently connects the ATX power supply's PS_ON line to ground. Causes the ATX power supply to remain on at all times. 
  - The pins to use are the 3rd and 4th pins on the "clip side" of the 24-pin connector (which are pin numbers 15 and 16). I'm doing the shorting on the female ATX connector that I bought and which I'm using for other connections as well. [Click here to see pinouts](https://www.etechnophiles.com/wp-content/uploads/2023/02/ATX-power-supply-connector-pinout-768x441.jpg).
  - Important: Also set the "AUTO_PWRON1" jumper on the BC250 board, so that you can turn it on and off with its own power buttons. Set the jumper onto pins 2-3 of the connector.
  
  | ![](../Photos/Auto%20Poweron%20Jumper.jpg) **AUTO_PWRON1 Position** |
  |:-------------------------------------------------------------------:|

#### R1, R2, R3:

  - Resistors, 10k ohm, 1/8 watt (x3)
  - These resistors limit the current to the base legs of the transistors and assist with the logic circuit which decides when the LEDs turn on and off.

#### R4, R5, R6:

  - Resistors, 100 ohm (x2) - Or sized to match your desired LEDs.
  - Resistor, 1k  ohm (x1) - Or sized to match your desired LEDs.
  - These resistors limit the current to the LEDs. This circuit uses 5v from the ATX power supply, which is too much for most LEDs. If you power the LEDs directly with 5v, they will burn out.
  - Choose the resistor values for these, depending on the LEDs that you use. Different LEDs will need different resistor values.
  - Note: The resistor on the yellow LED is deliberately sized to make the LED glow more dimly and more orange in color.

#### LEDs:

  - Blue 3.2fv LED (x2)   - Or whatever colors and voltages you like.
  - Yellow 2.0fv LED (x1) - Or whatever colors and voltages you like.

#### D1, D2:

  - Diodes, 1N1914 or similar (x6)
  - These control the logic of which of the blue LEDs light up at any given time. If the LED1 pin from the BC-250 has a signal, the Blue1 LED lights up. If the LED2 pin from the BC-250 has a signal: Both the Blue1 and Blue2 LEDs light up.

#### Transistors:

  - BC548 or similar (x3)
  - Directs the power to illuminate each LED, depending on the logic from the diodes and the 10k resistors.

#### SW1, SW2:

  - Pushbutton switches with long necks, Digikey EG5455-ND or similar.
  - These are placed under the Steam logo/badge.
  - (Example:) https://www.digikey.com/en/products/detail/e-switch/TL1105YF160Q/514438

#### J3:

  - Small 2-pin connector of any type.
  - Allows for connection and disconnection from the wire that goes to the BC-250 board's power button pins. Since the connection must be soldered to the BC-250 board, this allows for easy installation and maintenance.

Construction and Assembly Notes
-------------------------------

I used a perfboard to solder up the components, and then designed my 3D printed case to fit the perfboard. Your design may differ from mine. 

The perfboard I used is this snappable board. It comes as a 3.5" x 3.8" board which snaps into four pieces, each piece making a 1.75" x 1.9" board: https://www.amazon.com/dp/B081QYPHHP

| ![](../Photos/LED%20Circuit%2001.jpg) **LEDs, M2 Self-tapping Plastic Screws** | ![](../Photos/LED%20Circuit%2002.jpg) **Circuit Back Side** | 
|:------------------------------------------------------------------------------:|:-----------------------------------------------------------:|

The badge is 3D printed in several pieces, which are then assembled onto the front of the [case](../Case/README.md). 

I have designed the depth of the case's front panel recess, and the thickness of the badge, so that the LEDs and the button tops are at exactly the right depth to fit. The LEDs sink loosely into the three holes in the badge, and the button tops of the long-necked buttons just barely touch the back side of the badge.

Soldering the LEDs at the correct depth is tricky. It was done by putting the badge in place, putting the LEDs into the perfboard loosely, putting the perfboard into place behind the badge, and then solder-tacking one pin of each LED in place. The LEDs should fit deep into the sockets in the badge without completely topping out, but still go deep enough so that they glow sideways into the badge material.

(Not Pictured) To make the fitting of the LEDs and the badge easier, I used the cutting features of Bambu Studio to cut out and 3D print a small section of the front of the case where the badge gets mounted. It made the process of test fitting and tacking the LEDs in place much easier.

| ![](../Photos/Badge%20with%20Board%2001.jpg) **LED Tips Are Painted Gray** | ![](../Photos/Badge%20with%20Board%2002.jpg) **Test Fitting Parts** |
|:--------------------------------------------------------------------------:|:-------------------------------------------------------------------:|

The perfboard is mounted inside the case behind the front panel badge with small M2 x 10mm self-tapping plastic screws. The standoffs for the screws are at exactly the right depth for the LEDs and the button tops. The screw thickness must match the screw holes in the plastic, or else they will split the standoffs open.

The badge is affixed with two very small squares of [Alien Tape](https://www.amazon.com/dp/B083C5WXWV). The two rectangular gaps are where the Alien Tape goes, and the two round gaps are where the tops of the long-neck push buttons touch the front of the badge.

It's important to use very small pieces of Alien Tape, because it is very strong, and because the badge still needs to move a little bit to actuate the buttons. The badge should be able to rock up and down. The Alien Tape squares are the fulcrum of its rocking motion. Pressing the top or the bottom of the badge should click the corresponding pushbutton on the LED controller.

| ![](../Photos/LED%20Board%20Mounted.jpg) **Perfboard Mounted** | ![](../Photos/Badge%20Alien%20Tape.jpg) **Small Squares of Alien Tape** | 
|:--------------------------------------------------------------:|:-----------------------------------------------------------------------:|

Finally, I have painted the tips of the LEDs with gray primer paint, so that most of their glow emits sideways, glowing through the badge instead of making bright hot spots at the center of each LED.



