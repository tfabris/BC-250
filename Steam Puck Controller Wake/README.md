Steam Puck Controller Wake
==========================
&copy; 2026 by Tony Fabris

A small circuit to use a 2026 Steam Controller to power-up a BC-250 gaming rig.

This was inspired by an original design by Martin Juhl Prendergast, but I made significant changes to mine. His original circuit is here: https://github.com/MrMEEE/BC250-USB-Controller-Poweron 


![](../Photos/Wake%20Circuit%2001.jpg)

| ![](../Photos/Case%20Front.jpg)             | ![](../Photos/Wake%20Circuit%2002.jpg)       |
|:-------------------------------------------:|:--------------------------------------------:|
| ![](../Photos/Steam%20Puck%20Dock%2001.jpg) | ![](../Photos/Steam%20Puck%20Dock%2002.jpg)  |


Project Links
-------------
- [Main Project Details](      ../README.md)
- [LED Controller](            ../LED%20Controller/README.md)
- [Steam Puck Controller Wake](../Steam%20Puck%20Controller%20Wake/README.md)
- [Case](                      ../Case/README.md)


Table of Contents
-----------------
- [Purpose](                            #purpose)
- [Behavior](                           #behavior)
- [Special Note About Sleep Mode](      #special-note-about-sleep-mode)
- [Special Note About ATX Power](       #special-note-about-atx-power)
- [BC-250 Power Button Connection](     #bc-250-power-button-connection)
- [Schematic](                          #schematic)
- [Components](                         #components)
- [Construction Notes](                 #construction-notes)
- [Building and Uploading ESP32 Code](  #building-and-uploading-esp32-code)
- [Notes](                              #notes)


Purpose
-------

The BC-250 is missing one of the features that a Steam Machine has: The ability to wake it up from the controller, as if it were a game console. My project adds this wake-from-controller feature to the BC-250.

This uses an ESP32-S2 Mini module, combined with a TS3USB221 USB Multiplexer board, to allow you to wake up your BC-250 when you turn on your 2026 Steam controller.

The ESP32 code in this circuit is specifically designed to work with a 2026 Steam Controller Puck. Currently it does not work with any other kind of controller. Theoretically, if you had the knowledge, you could modify the code yourself and make it accept other kinds of game controllers. Let me know if you're successful at that.

Potentially, you could also modify the electronic circuit in this design so that it could wake up a different kind of gaming computer other than a BC-250, but the instructions here are just for a BC-250.


Behavior
--------

This module is based around a TS3USB221 USB Multiplexer, which is sort of like an electronic A/B switch for a USB connection.

#### Multiplexer ports:

  - Input ("base" or "common") USB port: Connected to the Steam Controller puck.
  - Output 1 USB Port ("monitor"): Connected to the ESP32 module's USB port.
  - Output 2 USB Port ("runtime"): Connected to the BC-250's USB port.

The multiplexer allows switching the puck between the monitor port and the runtime port. The ESP32 is used as the "smarts" to control the multiplexer switching, and the BC-250's power button. It uses three GPIO pins on the ESP32 to do this.

#### ESP32 GPIO Pins:

  - A pin which senses whether the BC-250 is currently on or off ("POWER_SENSE_PIN").
  - A pin which, at the appropriate time, sends a pulse to the BC-250's power button ("POWER_ON_PULSE_PIN").
  - A pin which, at the appropriate time, switches the multiplexer between the monitor and runtime ports ("SELECT_PIN").

#### Basic behavior timeline:

  - The BC-250 is off and the multiplexer is set to output 1 ("monitor").
  - The user is sitting on the couch with the Steam Controller, and presses its Steam button.
  - Data from the controller begins flowing, and the ESP32 sees that data.
  - The ESP32 responds by pulsing the power button on the BC-250 to turn it on.
  - Once the BC-250 has had a moment to begin powering up, the ESP32 switches the multiplexer over to output 2 ("runtime").
  - The ESP32 cannot see data from the controller any more, because the controller is now connected to the BC-250 via the runtime port.
  - The user plays a game for a while, using the controller.
  - The user uses the controller to choose the "shutdown" menu on the BC-250. The BC-250 and the controller shut down.
  - The ESP32 senses that the power has been turned off on the BC-250.
  - The ESP32 responds by switching the multiplexer back to output 1 ("monitor") again. It awaits data from the controller, and the cycle repeats.

That's the basic "golden path" behavior. There are a lot of edge cases and timing issues along the way, which hopefully will all get sorted out in the ESP32 firmware code eventually. At the moment it's working fine for me, and it only gets confused if I do something weird, like, turning the BC-250 on and off rapidly. In theory nothing bad can happen... at worst, maybe something like this could happen: the BC-250 turns back on when you thought you shut it down, a situation which can happen if the controller is unexpectedly still on and sending data.

The code does handle the situations where the BC-250 and the controller are turned on and off independently, and it does pretty well with that, but there are probably some small timing windows where it can get confused and the controller might wake up the machine unintentionally.


Special Note About Sleep Mode
-----------------------------

The BC-250 does not have the capability of entering a Sleep or Standby mode. If the user chooses "sleep" from Steam's power menu, it will not sleep, it merely turns the screen black and stays running. This circuit doesn't try to deal with any of that, it only responds to the BC-250 being on or off. See the [main documentation here](../README.md#problem-when-the-user-selects-sleep) for a workaround.


Special Note About ATX Power
----------------------------

The BC-250 is powered by an ATX power supply. I have chosen to configure this system so that the ATX power is "always-on" to simplify the circuit and the code behavior. The BC-250's USB ports are always powered in this mode, so that this circuit is always powered, and also so that the controller puck is always powered so that it can charge the controller. As seen in the photos, I have designed my BC-250 case so that there's a little place to dock the controller.

#### Therefore, special power configuration is needed:

- Solder a long wire connection to the BC-250's power-on button pins (detailed in its own section, below).

- Jumper the the ATX power supply's PS_ON line to ground, on its 24-pin connector, so that it's always-on. I'm using a separate socket connector for my ATX 24-pin connection (listed in "Components", below). Refer to [this illustration](https://www.etechnophiles.com/wp-content/uploads/2023/02/ATX-power-supply-connector-pinout-768x441.jpg) of the pins, you can connect PS_ON to either of the GND pins next to it.

- Set the "AUTO_PWRON1" jumper on the BC250 board, so that you can turn it on and off with its own power buttons. Set the jumper onto pins 2-3 of the connector. As shown in this photo:

  | ![](../Photos/Auto%20Poweron%20Jumper.jpg) **AUTO_PWRON1 Jumper Position** |
  |:--------------------------------------------------------------------------:|


BC-250 Power Button Connection
------------------------------

Solder a pair of wires to the appropriate power button and ground pads on the rear back side of the BC-250 board. Use extreme caution not to accidentally cause a connection to any of the other pads or pins.

The wires should be long enough to run the length of the board, plus significant extra length, to allow for fiddling around as you install things into the case. Use caution not to yank on this wire or get it caught in anything, it might rip out the pads on the BC-250 if you do.

To the other end of this wire pair, solder a two-pin connector so that it can be disconnected and reconnected as needed during installation and maintenance. ***This is the J3 connector on the schematic.*** I used an old connector that I had lying around:

| ![](../Photos/Power%20Button%20Solder%20Points.jpg) **Identify Solder Points** | ![](../Photos/Power%20Button%20Connector.jpg) **Quick Disconnect for Maintenance** |
|:------------------------------------------------------------------------------:|:-----------------------------------------------------------------------------------|

The J3 connection goes to a couple of things: It connects to a front-panel [power switch and LED assembly](../LED%20Controller/README.md), and also to the transistor for the "POWER_ON_PULSE_PIN" circuit as described in the schematic below.


Schematic
---------

Refer to the accompanying schematic file (open it in Kicad):

  - "Steam Puck Controller Wake.kicad_sch"

Components
----------

#### U1:

  - ESP32-S2 Mini module.
  - (Example): https://www.amazon.com/dp/B0B291LZ99

#### U2:

  - TS3USB221E USB Multiplexer Board 
  - (Example): https://www.amazon.com/dp/B099NPVWP3
  - The "S" pin on this multiplexer is connected to the SELECT_PIN on the ESP32, and it controls which of the two output USB ports on the multiplexer is active. A LOW signal on this pin sets the multiplexer to output 1, which is "monitor" mode, and a HIGH signal on this pin sets the multiplexer to output 2 which is "runtime" mode.

#### J5 - ATX Connector:

  - ATX 24-pin socket connector.
  - Connector which allows connecting and disconnecting the JP1 jumper, and is also used in the [LED controller](../LED%20Controller/README.md).
  - (Example:) https://www.sparkfun.com/atx-power-supply-connector-right-angle.html

#### JP1:

  - A wire which permanently connects the ATX power supply's PS_ON line to ground, as described above.
  - This sets the ATX power supply to always-on.
  - ATX power supply pinout diagram: https://www.etechnophiles.com/wp-content/uploads/2023/02/ATX-power-supply-connector-pinout-768x441.jpg
  - Also make sure to set the "AUTO_PWRON1" jumper on the BC250 board, so that pins 2-3 of that connector are connected, allowing you to turn the BC-250 on and off from its own power button.

#### JP2:

  - A blob of solder across the pads labeled OE and GND on the bottom side of the TS3USB221E USB Multiplexer Board.
  - This is the Output Enable for the multiplexer, essentially an "on" switch for the multiplexer.

#### R1, R2:

  - Resistors, 1k ohm, 1/8 watt (x2)
  - These resistors limit the current to and from some of the sense and control pins on the ESP32 module.

#### R3: 

  - Resistor, 10k ohm (x1) - Ensures that the base leg of the transistor has a drain path to ground, so that residual voltage can't accidentally activate that circuit and power-up the BC-250.

#### R4:

  - Resistor, 22k ohm (x1) - Pullup resistor to fight a problem I was having, where I would occasionally see brief "controller disconnected" messages as if the multiplexer were switching unexpectedly. There is a possibility that this could have been caused by the S line ("SELECT_PIN") dropping low even when set to high, so the pullup resistor is supposed to help prevent this. It didn't fix the problem, but it's a good thing to have anyway.

#### C1:

  - Capacitor, 0.1µF Ceramic - Noise filter to fight the same problem that R4 is also fighting, intended to smooth out possibly noisy voltage spikes or drops in the voltage on the S line ("SELECT_PIN"). Didn't fix the problem, but it's a good thing to have anyway.

#### C2:

  - Capacitor, 0.1µF Ceramic - Noise filter to ensure the overall voltage on VBUS doesn't have noisy voltage spikes. Also intended to fix the the problem that R4 was hoping to fix.

#### C3:

  - Capacitor, 10µF Ceramic - Voltage reservoir to fill in possible power sags on VBUS, from the power coming in from the USB plug. Also intended to fix the the problem that R4 was hoping to fix.

#### Q1:

  - Transistor, BC548 or similar (x1)
  - Used for bridging the BC-250's "Power On" button at the appropriate time, to simulate a button press.
  - Base leg of the transistor receives a HIGH signal from the POWER_ON_PULSE_PIN, for 500ms to simulate a press of the power button. 
  - Collector leg is connected to the power button pin on the BC-250 via J3.
  - Emitter leg is connected to the ground on the BC-250 via J3.

#### J3:

  - Small 2-pin connector of any type.
  - Allows for connection and disconnection from the wire that goes to the BC-250 board's power button pins. Since the connection must be soldered to the BC-250 board, this allows for easy installation and maintenance.

#### J1, J6:

  - USB type-A female ports (x2), for connecting the multiplexer to the Steam Controller puck and the ESP32.

#### J2:

  - USB type-A connection, for connecting to the BC-250's USB port on the back of the board. One end must be a type-A male connector for the BC-250, and the other can be either a connector for a Type-A-to-Type-A cable, or, bare wires soldered to the J2 point on the circuit board.
  - I run this cable out the back of my [3D-printed case](../Case/README.md), next to the USB ports, so that I can plug it into one of the USB 2.0 ports on the BC-250. Make sure to run the cable **before** inserting the BC-250 and buttoning it up:

  | ![](../Photos/Back%20Panel%20USB%20Cable%2001.jpg) **Notch for USB Cable** | ![](../Photos/Back%20Panel%20USB%20Cable%2002.jpg) **I/O Ports** |
  |:--------------------------------------------------------------------------:|:----------------------------------------------------------------:|


#### J4:

  - A small connector which facilitates easy connecting and disconnecting from the "3V" pin on the BC-250's TMPS1 connection header. I scrounged one from my leftover parts bin, so I don't have any specific example to give here. Basically any connector which has the correct pin pitch, and seats on that TPMS1 connection without falling off, can work. If needed, you can use a header with multiple connector pins so that it stays on better (I ended up doing that after taking the photos).
  - This is wired to POWER_SENSE_PIN. A HIGH signal recieved on this pin from the TPMS1 connector is interpreted to mean that the BC-250 is currently powered on.
  - Make sure to connect to the correct pin, use [the pinouts in the docs](https://elektricm.github.io/amd-bc250-docs/hardware/pinouts/#tpms1-lpc-header) and the one "blank" pin to find the "3V" pin on the connector.
  - The [BC-250 docs say](https://elektricm.github.io/amd-bc250-docs/hardware/pinouts/#tpms1-33v-power-rail) "TPMS1 3.3V Power Rail - The 3.3V pin on TPMS1 (pin 9) is active only when the board is powered on, making it suitable for relay or control circuit applications that need to detect system power state." 
  
  | ![](../Photos/TPMS1%20Connection%2001.jpg) **TPMS1 Connection** | ![](../Photos/TPMS1%20Connection%2002.jpg) **TPMS1 Connection** |
  |:---------------------------------------------------------------:|:---------------------------------------------------------------:|
  | ![](../Photos/TPMS1%20Connection%2003.jpg) **TPMS1 Pin**        |

#### USB C-to-A cable:

  - A tiny 3-inch cable, which connects J6 from the multiplexer board to the ESP-32's USB port. 
  - (Example): https://www.amazon.com/dp/B0FZSYD7Z1


Construction Notes
------------------

Because I only intended to make one of these, I did not design a printed circuit board. I built my circuit on a small perf board. Your design for your version of the circuit will be different from mine, and will be partially governed by the shape and size of your perf board (or whatever system you use to put this together). Follow the schematic carefully and ensure nothing grounds out where it's not supposed to. 

The perfboard I used is this snappable board. It comes as a 3.5" x 3.8" board which snaps into four pieces, each piece making a 1.75" x 1.9" board: https://www.amazon.com/dp/B081QYPHHP

This device is intended to sit inside my [BC-250 case](../Case/README.md), so after construction, I wrapped some gaffer tape around it to help prevent it from grounding out in there.

| ![](../Photos/Wake%20Circuit%2003.jpg) **The Mux Hides Under the USB-C** | ![](../Photos/Wake%20Circuit%20Wrapped.jpg) **Gaffer Tape** |
|:------------------------------------------------------------------------:|:-----------------------------------------------------------:|

#### Arbitrary GPIO Pins:

The ESP32 code uses three GPIO pins for its sensing and control lines. I chose which pins were which, based on the convenience of their positioning on my perf board. You might choose different pins, so update those in the ESP32 code (in main.cpp) before uploading the code to your ESP32.

#### Glue down the loose wires for strain relief:

Some of the connections are wires that run to other devices. In my case, it was one USB cable, a pair of wires to the power-button connection, and a single wire to the connector which goes to the 3v sense pin on the TPMS1 header. Where possible, I glued a small part of those wires to the finished perf board using gel-style cyanoacrylate glue. This acts as strain relief to prevent the wires from pulling out at their solder joints.

#### Leave space for the WiFi antenna:

There is a section at the top of the ESP32-S2 Mini board (opposite the USB port) where its WiFi antenna is located. You can recognize the antenna on the PCB as a zigzag line. This is known as the "keepout area" of the board. Design your board layout so that the antenna can remain exposed with no circuits immediately around it. I chose to position mine so that its WiFi antenna protrudes past the edge of my perf board.

#### Multiplexer OE connection:

On my TS3USB221E multiplexer board, on its bottom side, there was a pair of pads labeled "GND" and "OE" which is "Output Enable". It allows an external connection to turn the multiplexer output on and off. In order for this circuit to work at all, the OE pad must be connected to the ground pad that's right next to it (which on my board, means that I put a blob of solder across both pads).

#### Soldering the S connection:

On my TS3USB221E multiplexer board, there was not an easy/nice "through-hole" pin for connecting a wire to its all-important "S" terminal. That's the "SELECT_PIN" connection which selects output 1 or output 2 on the multiplexer. Instead of a nice pin connection, it was a small surface-mount pad on the bottom of the device, on the opposite end of the board from the OE connection. This must connect to one of the ESP32's GPIO pins, and do so without accidentally grounding. So care must be taken when setting this up. I managed to rip the pad off accidentally, and had to locate the resistor that the pad runs to, and run a tiny jumper wire to that. So be careful with that pad! Maybe order extras when you're ordering these little boards?

#### 3.3V Limit:

The ESP32's GPIO pins are only designed to accept 3.3 volts. Though the development board is powered by 5v from the USB port, the ESP32-S2 Mini chip runs at 3.3v. Connecting signals higher than 3.3v to its GPIO pins will destroy the chip. You must either connect only to 3.3V signal lines, or use level shifters or voltage dividers to lower the signals to 3.3v. For example, if you are modifying the circuit so that it works with a different computer other than the BC-250, make sure that the sense line is shifted to the correct level and that it won't exceed 3.3v.


Building and Uploading ESP32 Code
----------------------------------

### Source Code Files

In this repository, the source code files are:

- "platformio.ini": Build configuration
- "secrets.ini":    File that you create yourself (see below)
- "src/main.cpp":   ESP32 code

Prepare your computer for compiling and uploading the ESP32 code. In my case, I am not using an existing IDE development environment such as Arduino IDE or VS Code. I'm just using a plain text editor and the command-line version of PlatformIO. Your tool setup might be different from mine, these instructions are for PlatformIO.

### Install PlatformIO Core CLI

Ensure that your computer is has Python 3 installed, then install PlatformUI using their `get-platformio.py` installer script. Follow their instructions here:

- https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html

When it's done installing, you can delete the file `get-platformio.py` that you downloaded.


#### Install shell commands

Following [their instructions](https://docs.platformio.org/en/latest/core/installation/shell-commands.html#piocore-install-shell-commands), add the PlatformIO directory to your system path config, and make symbolic links to PlatformIO so that you can run it from anywhere. These commands were the ones for my Mac, which slightly differ from the ones on their instructional page. I had to modify mine to get the path statement to work correctly. Yours may differ as well, so use caution with these commands:

      echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bash_profile
      
      ln -s ~/.platformio/penv/bin/platformio ~/.local/bin/platformio
      ln -s ~/.platformio/penv/bin/pio ~/.local/bin/pio
      ln -s ~/.platformio/penv/bin/piodebuggdb ~/.local/bin/piodebuggdb

Close and re-open your terminal window for the bash_profile to reload, then test that PlatformIO is working with this command:

      pio --version

###  Locate your ESP32-S2 device on the USB ports

- Unplug your ESP32-S2 from the multiplexer board, unplug the multiplexer board from the BC-250, and plug the ESP32-S2 into your computer.
- Important: Put the ESP32-S2 into Bootloader mode first (Hold 0 button, tap the RST button, release the 0 button).
- Enter this command in your terminal:

      pio device list

This will list all the USB devices on your system. If you can't immediately tell which one is the ESP32-S2, then unplug it and run `pio device list` again. Whichever device disappeared from the list is probably your ESP32-S2.

In my case, this revealed that my ESP32-S2 is on `/dev/cu.usbmodem01` and it is named `LOLIN-S2-MINI`.

### Prepare Secrets File

Create a text file in the project folder alongside all the other files that you received from this GitHub repository. In the folder named `Steam Puck Controller Wake`, next to the file `platformio.ini`, create a plain ASCII text file called `secrets.ini`. Inside the file place this text:

    [secrets]
    wifi_ssid = "Your WiFi Network Name"
    wifi_pass = "YourWiFiNetworkPassword"
    ota_pass = "SomePasswordYouInvent"

Obviously, replace the names and passwords with something of your own. This file will allow the ESP32-S2 chip to connect to your WiFi network so that you can view its logs, and later, so that you can upload new source code to it via WiFi.

### Build the source code

Change to the directory of the project files. Change to the folder containing the files `platformio.ini` and the `secrets.ini` that you just created. This should be the directory level above the `src` folder. Then run the PlatformIO command which builds the code. In my case, it was this folder, yours may differ:

    cd ~/"Documents/Projects/BC-250/Steam Puck Controller Wake"
    pio run

The code should build successfully without errors. It has not been installed to the ESP32-S2 yet, we'll get to that in a moment. Stop here and troubleshoot if it doesn't successfully build.

### Clean the build files (if needed)

If you are modifying and experimenting with the ESP32 code, there are some situations where you need to clean out the build files in the project folder. This is the command to do that, just in case. It will not hurt anything to run it, and I have placed it here for reference:

    cd ~/"Documents/Projects/BC-250/Steam Puck Controller Wake"
    pio run --target clean

### Edit platformio.ini for first-time USB uploading

The first time you upload code to the ESP32-S2, it must be done with the USB cable, because the WiFi uploading hasn't been installed to it yet. The code in this repository will default to WiFi uploading, so you must change it to USB uploading for the first upload, and then after that you can change it back to WiFi, assuming the ESP32-S2 connects successfully to your WiFi.

Edit the file `platformio.ini` in the project directory. It will have a section that looks something like this:

    ; ---------------------------------------------------------------------------
    ; Comment out the upload_* lines below for USB firmware uploads.
    ; Uncomment them for WiFi firmware uploads. Note that the WiFi firmware
    ; uploads will not work until after the first USB upload has worked.
    ; ---------------------------------------------------------------------------
    upload_protocol = espota
    upload_port = SteamPuckControllerWake.local  
    upload_flags =
        --port=3232
        --auth=${secrets.ota_pass}

Comment out the lower half of that section, by placing semicolons at the beginning of each line, and save the file. It should now look like this:

    ; ---------------------------------------------------------------------------
    ; Comment out the upload_* lines below for USB firmware uploads.
    ; Uncomment them for WiFi firmware uploads. Note that the WiFi firmware
    ; uploads will not work until after the first USB upload has worked.
    ; ---------------------------------------------------------------------------
    ;upload_protocol = espota
    ;upload_port = SteamPuckControllerWake.local  
    ;upload_flags =
    ;    --port=3232
    ;    --auth=${secrets.ota_pass}

Now it is configured for USB uploading.

### Compile and upload the code to the ESP32-S2, the first time via USB cable

- Unplug your ESP32-S2 from the multiplexer board, unplug the multiplexer board from the BC-250, and plug the ESP32-S2 into your computer.
- Important: Put the ESP32-S2 into Bootloader mode first (Hold 0 button, tap the RST button, release the 0 button).
- Enter these commands in your terminal (these are for my Mac, yours may differ):

      usbPortName="/dev/cu.usbmodem01"
      cd ~/"Documents/Projects/BC-250/Steam Puck Controller Wake"
      pio run --target upload --upload-port $usbPortName

In the example above, make sure to enter your own values for usbPortName and for the directory name. Make sure that it successfully uploads the code to your ESP32-S2 without any errors.

### Ensure that it connected to WiFi

Because, when the device is running normally, it will be plugged into the multiplexer rather than your computer, serial logging does not work. Instead, it has a WiFi logging system where you retrieve its code logs via a local web page. If it connected to WiFi correctly, then you should be able to surf to its embedded site http://SteamPuckControllerWake.local and see the log output.

#### Troubleshooting the WiFi connection

If you can't see the logging page over WiFi, troubleshoot the networking side of things. Ensure that the SSID and password were correctly updated in secrets.ini, and look at your WiFi router's management page, to see if the device indeed connected to WiFi. If it connected, it should be listed in the DHCP client table (or whatever features your router has for looking at those things). 

If you can see the device in your DHCP client table, but cannot view its web page by name, check if you can connect to its IP address instead. Surf to http://192.168.0.123 or whatever value it was in your client table. If the IP address connection worked but the name did not, you'll need to troubleshoot why the DNS name is not working. If you need to edit the name, it's defined and manipulated in a few places in platformio.ini and src/main.cpp. Remember to upload your code to the ESP32 via USB after making changes.

Once you can see the logs page at http://SteamPuckControllerWake.local on your network, you can move on to uploading the code via WiFi.

### Connect the ESP32-S2 back to the multiplexer and BC-250

Unplug your ESP32-S2 from your computer and reconnect it to the multiplexer board. And plug the multiplexer back into the BC-250. You should now be configured so that you can update code remotely (no more USB uploading).

### Compile and upload the code to the ESP32-S2 via WiFi

Edit the file `platformio.ini` again, and this time, uncomment the the lines related to WiFi uploading. The section should look like this again:

    ; ---------------------------------------------------------------------------
    ; Comment out the upload_* lines below for USB firmware uploads.
    ; Uncomment them for WiFi firmware uploads. Note that the WiFi firmware
    ; uploads will not work until after the first USB upload has worked.
    ; ---------------------------------------------------------------------------
    upload_protocol = espota
    upload_port = SteamPuckControllerWake.local  
    upload_flags =
        --port=3232
        --auth=${secrets.ota_pass}

Issue the build command and upload via Wifi.

      cd ~/"Documents/Projects/BC-250/Steam Puck Controller Wake"
      pio run --target upload

If this succeeds, then you can edit and experiment with the code to your heart's content (or, if it's working perfectly, just play games). The only time you should ever need to go back to USB uploading is if you get a new ESP32-S2 chip, or if you upload code changes which break its ability to connect to WiFi.


Notes
-----

#### 2026 Steam Controller Puck, USB ID:

Information that comes up when it's plugged into my Mac:

      /dev/cu.usbmodemFXB99605014FB1
      Hardware ID: USB VID:PID=28DE:1304 SER=FXB99605014FB LOCATION=2-1.1.2.4
      Description: Steam Controller Puck

#### BC-250 Documentation:

  - https://elektricm.github.io/amd-bc250-docs

#### PlatformIO Location:

  - On MacOS, the full path to the `platformio` executable is `~/.platformio/penv/bin/platformio`

