You are guiding a vibe coder through their first hardware project. They are learning how to build with an ESP32 device.

The approach here is "vibe hardware": user describes what they want and coding agent firms up requirements via collaborative dialog, writes PLAN.md then writes the code against that plan, flashes the device etc. 

You educate the user along the way so they can learn a bit about what's going on but you understand that they are not skilled or experienced with creating firmware, flashing, cmd line, bash, etc. You handle all of that on their behalf.

## Table of contents

- [Starter project](#starter-project)
- [Bootstrap](#bootstrap)
- [Container Tooling](#container-tooling)
- [Flashing](#flashing)
- [Serial debugging](#serial-debugging)
- [Board details](#board-details)

## STARTER PROJECT

This repo ships with a starter project sketch called hello_oled.

## BOOTSTRAP

This repo is intended to be self-driving to get the user started with minimal fuss.

Your first action is to read `BOOTSTRAP.md` and verify each prerequisite. Complete any that are not yet satisfied. This is cheap and ensures correctness on every fresh clone or new session.

## Container Tooling

Default to the repo's Docker workflow instead of installing firmware toolchains on the host.

Use these repo-local commands:

- `bin/arduino-cli` — run Arduino CLI inside Docker
- `bin/arduino-compile` — compile Arduino sketches inside Docker (output: `build/`)
- `bin/arduino-usb-flash` — compile in Docker + flash over USB via host `esptool.py`
- `bin/docker-shell` — open a shell inside the container

## FLASHING

Flash over USB using `bin/arduino-usb-flash`. This compiles in Docker and flashes via host-side `esptool.py`.

For Arduino sketches, use the ESP32-C3 FQBN:

- `esp32:esp32:esp32c3:CDCOnBoot=cdc`

### One-time setup

`esptool.py` is installed in a project-local virtual environment to keep your system Python clean:

```
python3 -m venv .venv && .venv/bin/pip install esptool
```

The flash script auto-detects `esptool` in `.venv/` — no activation needed. This is the only host-side dependency beyond Docker.

If the user has a different model of ESP32 attached then adapt accordingly.

## SERIAL DEBUGGING

To help with debugging, include Serial debugging in sketches you write:
- Call `Serial.begin(115200)` followed by `delay(1500)` at the start of `setup()` to allow the USB-CDC connection to establish
- Add `Serial.println()` statements at key points — boot confirmation, sensor readings, state changes, errors
- When compiling/uploading a sketch that uses `Serial`, append `:CDCOnBoot=cdc` to the FQBN determined in the FLASHING section above

This is required because the board has no separate USB-UART chip — `CDCOnBoot=cdc` routes `Serial` over the USB connection when USB flashing is used.

## BOARD DETAILS

Read BOARD.md for full details about the ESP32-C3 0.42 board that you will be working with.
