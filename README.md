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


