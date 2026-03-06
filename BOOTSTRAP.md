# Bootstrap Checklist

This file is for the agent. Work through each step silently. Keep the user informed with brief, friendly status updates but do not ask them to run commands or look at this file. Do not install unnecessary things like emulators etc unless explicitly asked to.

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
   (cd ota_base_fw && ../bin/idf.py --version)
   ```
5. Treat the repo-local wrappers as the default interface for future work:
   - `bin/arduino-cli`
   - `bin/idf.py`
   - `bin/arduino-compile`
   - `bin/idf-build`
   - `bin/idf-ota-upload`

[] Docker toolchain ready
[] Updated CLAUDE.md and AGENTS.md for Docker workflow

---

## Step 2: Connect to the board

Prefer OTA for the Dockerized workflow.

**Path A — WiFi / OTA (default):**
- Ask the user to power the board on and read the IP from the OLED if needed
- Confirm the device is reachable over the network
- Store the IP or URL for use in subsequent OTA uploads

**Path B — USB cable (fallback):**
- Only use direct USB flashing if OTA is unavailable
- On macOS, remember Docker Desktop is not a clean replacement for serial passthrough; USB recovery may require a host-side fallback or another machine

[] Board is reachable

---

## Step 3: Test build and upload

- Compile `sketches/hello_oled` with `bin/arduino-compile`
- Build `ota_base_fw` with `bin/idf-build`
- If the board is already running OTA-capable firmware, test upload with `bin/idf-ota-upload <device-ip-or-url>`
- Confirm the device responds after OTA when that path is available

[] Test build and OTA path succeeded

---

## Done

Once all steps are checked, update CLAUDE.md:
- Change `[] Bootstrap complete` to `[x] Bootstrap complete`
- Ensure the Docker workflow is reflected in `CLAUDE.md` and `AGENTS.md`
- Do not bother the user with details — just let them know they're ready to go
