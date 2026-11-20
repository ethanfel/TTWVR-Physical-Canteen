# TTWVR Physical Canteen

Take a drink from your Vault 13 canteen to create a manual Fallout save. A bottle gulp sounds when Fallout saves. Each drink gets a new dated **Canteen** entry in the Load menu.

## Install

Requires Fallout: New Vegas with Classic Pack, TTWVR, xNVSE and JIP LN NVSE. Developed with Tale of Two Wastelands and TTWVR 1.1.7.

1. Download the [release ZIP](https://github.com/ethanfel/TTWVR-Physical-Canteen/releases/latest).
2. Install the ZIP with your mod manager, enable the mod and **TTWVRPhysicalCanteen.esp**, then restart Fallout.

The mesh, icon and gulp come from your installed game. For gesture-only saves, leave the separate *Vault 13 Canteen Auto Save* mod disabled.

## Drink to save

Carry the original **Vault 13 Canteen**. A weightless **Vault 13 Canteen (Drink)** appears under Weapons. Assign it to a normal TTWVR weapon holster and squeeze grip at that holster to draw it. Keep grip held while drinking. Release grip anywhere to put it away. The same slot is ready for your next draw.

Start with the canteen upright and away from your face. Raise its neck to your mouth, tip the base up and hold briefly for a sip. Lower it and bring it upright before drinking again. Carrying it, opening a menu and leaving it at your lips don’t trigger saves.

## Build

Run `make package` with Python 3, i686 MinGW-w64 and a native C++17 compiler. The ZIP is written to `build/`.

[GPL-3.0-only](LICENSE)
