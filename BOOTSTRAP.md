# Bootstrap

Agent-only file. Work through each step silently. Keep the user informed with brief status updates but do not ask them to run commands. Do not modify tracked files to record bootstrap state.

For each step: check if already satisfied, skip if so, complete if not.

## Table of contents

- [Step 1: Docker toolchain](#step-1-docker-toolchain)
- [Step 2: USB flash setup](#step-2-usb-flash-setup)

---

## Step 1: Docker toolchain

1. Verify `docker` is available on the host.
2. If missing, install Docker Desktop or Docker Engine for the host OS.
3. Verify the image is built:
   ```
   bin/arduino-cli version
   ```
4. If the image is not built:
   ```
   docker compose build
   ```

---

## Step 2: USB flash setup

1. Check whether `.venv/bin/esptool.py` exists.
2. If not:
   ```
   python3 -m venv .venv && .venv/bin/pip install esptool
   ```
3. Verify the board is connected:
   ```
   ls /dev/cu.usbmodem*
   ```
   If no port is found, ask the user to connect the board via USB-C data cable.
