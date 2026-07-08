# Whirlwind
PC-to-JAMMA interface for arcade cabinets based on Raspberry Pi Pico.

This repo hosts two version of this project: 

- V1 adopts THT components (with the exception of the video amp IC, which is available in SMD-only package) and simplified sync composition circuit and level-shifting. [HERE](https://www.instructables.com/Whirlwind-PC-to-JAMMA-Arcade-Cabinet-Interface/) an Instructable with all details.

- V3 has most components in SMD form. The sync composition circuit is more robust (based on XOR logic) and level shifting done the pro way (dedicated buffer).

**These two versions share the same firmware.**

*What about V2? It has been designed, but never realized. It was similar to V1, but with XOR sync composition circuit.*




