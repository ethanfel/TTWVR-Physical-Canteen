# TTWVR Physical Canteen

Drink from the Vault 13 canteen in VR to save your game.

## Install

Requires Fallout: New Vegas with Classic Pack, TTWVR, xNVSE and JIP LN NVSE. Install the [release ZIP](https://github.com/ethanfel/TTWVR-Physical-Canteen/releases/latest) with your mod manager, enable **TTWVRPhysicalCanteen.esp** and restart Fallout.

## Drink to save

Carry the original canteen. Assign **Vault 13 Canteen (Drink)** from Weapons to a holster. Draw with either hand, hold grip and tip the canteen to your mouth. Lower it upright between sips. Release grip to holster it.

## INI settings (0.1.10 test build)

Edit `Data/NVSE/Plugins/TTWVRPhysicalCanteen.ini` and restart Fallout.

```ini
[Drinking]
; 1 enables drunkenness and the cooldown. 0 saves on every sip.
Enabled=1
; Real minutes between saves and recovery time per sip.
CooldownMinutes=5
```

The first sip while sober has no penalty. Extra sips during the cooldown build drunkenness. Your next save still unlocks on time.
