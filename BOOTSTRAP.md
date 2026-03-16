# Bootstrap Checklist

This file is for the agent. Work through each step silently. Keep the user informed with brief, friendly status updates but do not ask them to run commands or look at this file. Do not install unnecessary things like emulators etc unless explicitly asked to.

For each step: verify the prerequisite is already satisfied. If it is, move on. If not, complete it. Do not modify this file or any other tracked file to record bootstrap state.

## Table of contents

- [Step 1: Docker toolchain](#step-1-docker-toolchain)
- [Step 2: USB flash setup](#step-2-usb-flash-setup)
- [Step 3: Test build and flash](#step-3-test-build-and-flash)

---

## Step 1: Docker toolchain

1. Check whether `docker` is available on the host.
2. If Docker is missing, install Docker Desktop or Docker Engine for the host OS.
3. Check whether the Docker image is built by running:
   ```
   bin/arduino-cli version
   ```
4. If the image is not built, build it from the repo root:
   ```
   docker compose build
   ```

---

## Step 2: USB flash setup

1. Check whether `.venv/bin/esptool.py` exists.
2. If not, create the virtual environment:
   ```
   python3 -m venv .venv && .venv/bin/pip install esptool
   ```
   `bin/arduino-usb-flash` auto-detects `esptool` in `.venv/` — no activation needed.
3. Ask the user to connect the board via USB-C data cable (if not already connected).
4. Confirm the serial port is detected:
   ```
   ls /dev/cu.usbmodem*
   ```

---

## Step 3: Test build and flash

Only needed on first-time setup or if there is reason to doubt the toolchain works.

1. Compile `sketches/hello_oled` with `bin/arduino-compile`
2. Flash with `bin/arduino-usb-flash`
3. Confirm the board reboots with the new firmware
