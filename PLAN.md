# Wi-Fi Motion AI Plan

## Overview
This sketch creates a tiny on-device "micro AI" using simple heuristics on top of Wi-Fi RSSI variance. It does not do true radar ranging, but it can guess a one-word motion label and show a rough distance band while intensifying the lights with stronger motion.

## What the app will say
- **Movement word:** `STILL`, `SHIFT`, `WAVE`, or `WALK`
- **Distance word:** `NEAR`, `MID`, or `FAR`

## How it works
1. **Target Lock:** On boot, the ESP32 scans nearby Wi-Fi networks and picks the strongest access point.
2. **Sniffing:** It enables Wi-Fi promiscuous mode on that channel and listens to packets associated with the chosen access point.
3. **Feature extraction:** For matching packets it keeps a smoothed RSSI average, variance, and change rate.
4. **Tiny classifier:** It maps those live features into one-word motion labels:
   - `STILL` for almost no movement
   - `SHIFT` for light body repositioning
   - `WAVE` for mid-strength short motion
   - `WALK` for strong sustained motion
5. **Rough distance band:** It uses the signal disruption strength as a coarse guess:
   - `NEAR` when the disruption looks strong and immediate
   - `MID` for moderate motion energy
   - `FAR` for weak motion energy

## Display and lights
- **OLED:** Shows the motion word in large text, the distance word below it, and an intensity bar.
- **Blue LED + NeoPixel:** Flash faster as motion increases. The NeoPixel color shifts from green to orange to red.

## Important note
This version is an approximation using only the ESP32-C3 and ambient Wi-Fi signals. It is useful as an experimental motion toy, but it is not true AI perception and it cannot provide physically accurate distance measurements without adding a real distance-capable sensor.
