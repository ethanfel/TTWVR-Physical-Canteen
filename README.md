# TTWVR Physical Canteen

Drink from the Vault 13 canteen in VR to save your game.

## Install

1. Install TTWVR 1.1.8, xNVSE, JIP LN NVSE and the Classic Pack for Fallout: New Vegas.
2. Install the [Physical Canteen release ZIP](https://github.com/ethanfel/TTWVR-Physical-Canteen/releases/latest) with your mod manager. Let Physical Canteen win file conflicts with TTWVR and with TTWVR Alcohol 0.1.0 if installed.
3. Enable **TTWVRPhysicalCanteen.esp** and restart Fallout.

In `1.1.8-r1`, `1.1.8` is the tested TTWVR version and `r1` is the canteen release number. The addon detects the tracking interfaces at startup. A future TTWVR update that changes its grip resource pack needs a matching canteen ZIP.

## Drink to save

Carry the original canteen. Assign **Vault 13 Canteen (Drink)** from Weapons to a holster. Draw with either hand, hold grip and tip the canteen to your mouth. Lower it upright between sips. Release grip to holster it.

## INI settings

Edit `Data/NVSE/Plugins/TTWVRPhysicalCanteen.ini` and restart Fallout.

```ini
[Drinking]
; 1 enables drunkenness and the cooldown. 0 saves on every sip.
Enabled=1
; Real minutes between saves and recovery time per sip.
CooldownMinutes=5
; 1 adds sway to the headset view. 0 keeps the horizon level.
ViewSway=1
; 1 adds double vision at levels 2 and 3. The images briefly draw closer at irregular intervals. Set to 0 to disable.
DoubleVision=1
```

The first sip while sober has no penalty. Extra sips gradually blur your vision, make gun sights drift, and rock the horizon from side to side. At stronger levels your avatar also sways at the waist. Your next save still unlocks on time.
