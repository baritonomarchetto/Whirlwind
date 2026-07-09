# Whirlwind
PC-to-JAMMA interface for arcade cabinets based on Raspberry Pi Pico.

This repo hosts two version of this project: 

- V1 adopts THT components (with the exception of the video amp IC, which is available in SMD-only package) and simplified sync composition circuit and level-shifting. [HERE](https://www.instructables.com/Whirlwind-PC-to-JAMMA-Arcade-Cabinet-Interface/) an Instructable with all details.

- V3 has most components in SMD form. The sync composition circuit is more robust (based on XOR logic) and level shifting done the pro way (dedicated buffer).

**These two versions share the same firmware.**

*What about V2? It has been designed, but never realized. It was similar to V1, but with XOR sync composition circuit.*

# Main Features

- Based on RP2040 microcontroller (cheap, fast and readily available)
- Open firmware
- Joystick and keyboard emulation (composite-HID device)
- CRT protection from out-of-spec sync frequencies
- Built-in XOR sync composition circuit (V3 only)
- Built-in video amplification circuit
- Built-in audio amplifier (mono)
- built-in voltage buffering and shifting (V3 only)
- Extended JAMMA pinout (2 players, up to 6 buttons per player)
    
The main circuit revision since V1 is the sync composition and monitoring one. Where I first used a simple AND circuit made of one diode and a resistor to sum horizontal and vertical sync signals, I have now adopted a more robust integrated XOR gate.

Also related to the sync circuit is how the 5V negative sync signal is shifted to 3.3V (RP2040 is not 5V tolerant). The former voltage divider is now replaced by a dedicated buffer gate.

Yes, there's also another obvious difference: the adoption of surface mount devices (SMD). Not that I am getting lazier with age (It's more difficult for me to design SMT boards than THT), but some necessary component is already SMD-only (the video amp, with it's sub-mm pitch) so an assembly service is advised to be foreseen anyway.

Please notice: this board cannot force the VGA video signal to CGA or EGA resolutions by itself, but special softwares like [soft15KHz](http://forum.arcadecontrols.com/index.php?topic=66402.0) or [CRT emudrivers](https://geedorah.com/eiusdemmodi/forum/viewtopic.php?id=295) are needed.

# Schematics and Circuits Description

## Microcontroller Board
Raspberry Pi Pico (30 pin variant clone) is the brain of the project. It's in charge of:

(a) keeping track of user inputs

(b) Human Interface Device (HID) emulation (keyboard and/or joystick)

(c) monitoring the video sync frequency

There's not only the Pico, anyway. The description of all circuits follows.

## Audio Amplifier
The built-in audio amplifier circuit is built around an LM386N. This is a power amplifier designed for use in low voltage consumer applications, which works well in single rail mode.

The gain is internally set to 20, which is more than enought for our use.

The volume is adjusted with a 10K on-board trimmer in voltage divider configuration.

The circuit is nothing particularly elaborate, with very low part count. It's mostly the one reported in the datasheet, with bass bost values set by ear.

This amplifier is good enought to drive a classic 3 W, 8 ohms speaker (those commonly installed in arcade cabinets), but will fry if you try to juice a lower impedance (i.e. 4 ohms) or higher power (i.e. 5 W) speaker.

My experience with these amps is that it's very important to use a legit IC... lesson learned the hard way.

This circuit calls for a source of +12V. It could be the cabinet power supply, or an external source. Even if two protection diodes are there, avoid juicing the board with bot +12V from the cabinet and +12V from an external source.

## Video Amplification
PC video card RGB signals are a little weak for a genuine arcade cabinet (0.7 Vpp vs 2-3 Vpp), then call for amplification. The amplifier of chioice is a THS7374, 4-Ch SDTV video amp with 9.5MHz filters and fixed 6dB gain (2X gain).

This video amplifier not only was born with this application in mind (which is always a good thing) but also has a very handy feature: a disable pin. This allows it's direct control from the microcontroller monitoring the sync frequency.

In all my previous "Jammarduino" iterations, the video amp output stage had a set of tree trimpots to help adjusting the R, G or B signal to fine tune the image colors. In practical application, these have little use. I then preferred to place three fixed current liming resistors in their place, with a value that should be ok for both arcade and TV CRT monitors (100 ohms).

Please notice that the 2X gain the THS7374 provides could be not sufficient to get a decent RGB amplification in some arcade monitors (i.e. Nanao). In such case a THS7375 (5X gain) could be a better choice. Untested!

## Sync Management
JAMMA standard calls for composite sync, but VGA delivers separate horizontal/vertical sync signal.

While it may seem convenient to simply join the two H and V wires, this solution is strongly discouraged in electronics. Connecting two output stages directly can cause electrical conflicts and, in the long run, damage the video card's sync generation circuits. A simple diode combiner (AND logic) can be used (and I did with excellent results on my CRTs) but does not properly handle the polarity inversion of the pulses, producing a composite signal that could be unusable by some monitor.

The circuit adopted here makes use of a XOR logic IC (74HC86). It can handle both positive and negative polarity sync signal at its inputs, then outputs a negative-polarity composite-sync signal.

Two RC networks (10 kΩ + capacitor) convert each sync signal into a steady DC voltage: high if the signal is positive, low if it is negative. Two XOR gates receive the H and V signals and the steady DC voltage. The XOR gates function as "logical rectifiers": If the signal is positive, it pass unchanged; if it is negative, it's inverted. After this stage, both H and V are always positive. The third XOR gate mixes the two positive signals. The output is what an arcade monitor needs: a stable, negative composite syncronization signal.

A branch of the C-sync is lowened to 3.3V by a buffer stage (74LVC2G125) powered @ 3.3V, ready to feed the Pi Pico GPIOs monitoring the frequency.

## Power Management
The main voltage juicing most IC's and the Pi Pico itself (+5V) comes from the USB line.

The audio amplifier circuit is juiced by +12V coming from the cabinet power supply on the jamma connector, or an external power supply through the dedicated DC barrel. Two schottky diodes (one per line) protect in case you have both 12V sources connected. Avoid connecting both, anyway.

The voltage lowener on the sync line is powered @ 3.3V by the Pi Pico built-in voltage regulator.

The video amplifier can be powered @ 3.3V or 5V. Powering at 5V gives more room before video artifacts start, so you name the obvious choice.
