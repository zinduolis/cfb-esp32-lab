# Bootstrap Checklist

This file is for the agent. Work through each step silently. Keep the user informed with brief, friendly status updates but do not ask them to run commands or look at this file. Do not install unnecessary things like emulators etc unless explicitly asked to.

## Table of contents

- [Step 1: Docker toolchain](#step-1-docker-toolchain)
- [Step 2: USB flash setup](#step-2-usb-flash-setup)
- [Step 3: Test build and flash](#step-3-test-build-and-flash)
- [Done](#done)

---

## Step 1: Docker toolchain

1. Check whether `docker` is available.
2. If Docker is missing, install Docker Desktop or Docker Engine for the host OS.
3. From the repo root, build the shared image:
   ```
   docker compose build
   ```
4. Confirm the containerized tools work:
   ```
   bin/arduino-cli version
   ```

[] Docker toolchain ready

---

## Step 2: USB flash setup

1. Set up the virtual environment for `esptool`:
   ```
   python3 -m venv .venv && .venv/bin/pip install esptool
   ```
   `bin/arduino-usb-flash` auto-detects `esptool` in `.venv/` — no activation needed.
2. Ask the user to connect the board via USB-C data cable.
3. Confirm the serial port is detected:
   ```
   ls /dev/cu.usbmodem*
   ```

[] USB flash ready

---

## Step 3: Test build and flash

- Compile `sketches/hello_oled` with `bin/arduino-compile`
- Flash with `bin/arduino-usb-flash`
- Confirm the board reboots with the new firmware

[] Test build and flash succeeded

---

## Done

Once all steps are checked, update CLAUDE.md:
- Change `[] Bootstrap complete` to `[x] Bootstrap complete`
- Do not bother the user with details — just let them know they're ready to go
