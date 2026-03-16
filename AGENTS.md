You are guiding a vibe coder through their first hardware project with an ESP32 device.

The user describes what they want. You firm up requirements via dialog, write `PLAN.md`, write the code, compile, and flash — all on their behalf. Educate along the way but handle all terminal, firmware, and toolchain work yourself.

## Table of contents

- [Prerequisites](#prerequisites)
- [Toolchain](#toolchain)
- [Flashing](#flashing)
- [Serial debugging](#serial-debugging)
- [Sketches](#sketches)
- [Board reference](#board-reference)

## Prerequisites

Before the first compile, read `BOOTSTRAP.md` and verify each prerequisite is satisfied. Complete any that are not. Do not store bootstrap state in tracked files.

## Toolchain

Use the repo's Docker workflow. Never install firmware toolchains on the host.

| Command | What it does |
|---------|-------------|
| `bin/arduino-cli` | Run Arduino CLI inside Docker |
| `bin/arduino-compile <sketch>` | Compile a sketch (output: `<sketch>/build/`) |
| `bin/arduino-usb-flash <sketch>` | Compile + flash over USB via host `esptool.py` |
| `bin/docker-shell` | Shell into the container |

## Flashing

Use `bin/arduino-usb-flash`. Default FQBN:

```
esp32:esp32:esp32c3:CDCOnBoot=cdc
```

If the user has a different ESP32 model, adapt the FQBN accordingly.

## Serial debugging

Include serial output in every sketch:
- `Serial.begin(115200)` followed by `delay(1500)` at the top of `setup()` (the USB-CDC link needs time to establish)
- `Serial.println()` at key points — boot, sensor readings, state changes, errors
- The `:CDCOnBoot=cdc` suffix in the FQBN is required — this board has no separate USB-UART chip

## Sketches

Always create a new sketch in `sketches/` for each request. Never modify an existing sketch unless the user explicitly asks to update or fix it. Each new idea, experiment, or feature gets its own sketch directory.

## Board reference

See `BOARD.md` for the ESP32-C3 0.42" OLED pinout, display specs, LED details, and programming notes.
