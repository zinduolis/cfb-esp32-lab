# Vibe Hardware

> Build hardware projects with ESP32 using AI coding agents — no experience required.

From the [Coding from Beach](https://www.codingfrombeach.com) workshop series.

---

## Table of contents

- [What you need](#what-you-need)
- [Getting started](#getting-started)
- [Architecture](#architecture)
- [Docker toolchain](#docker-toolchain)
- [The board](#the-board)
- [Repo structure](#repo-structure)

---

## What you need

- A laptop with Docker Desktop (or Docker Engine)
- A USB-C **data** cable
- A coding agent: Cursor, Claude Code, or Codex
- An ESP32 board (provided at the workshop, or bring your own)

---

## Getting started

Open this repo in your coding agent and describe what you want your device to do. The agent handles everything — setup, code, compile, flash.

Example prompts:

> "Make the LED blink rainbow colors"

> "Show a scrolling message on the display"

> "Detect nearby Wi-Fi devices and show the count on screen"

The agent will check prerequisites (Docker, esptool) and set them up automatically before building your project. No terminal skills or hardware experience needed.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  YOUR LAPTOP                                                    │
│                                                                 │
│  ┌───────────────┐    "make it blink"    ┌───────────────────┐  │
│  │  You (human)  │ ──────────────────▶   │   Coding Agent    │  │
│  └───────────────┘                       │ (Cursor / Claude  │  │
│                                          │  Code / Codex)    │  │
│                                          └────────┬──────────┘  │
│                                                   │             │
│                                            writes & runs        │
│                                                   │             │
│  ┌────────────────────────────────────────────────▼──────────┐  │
│  │  DOCKER CONTAINER                                         │  │
│  │                                                           │  │
│  │  ┌─────────────┐                                          │  │
│  │  │ Arduino CLI │                                          │  │
│  │  └──────┬──────┘                                          │  │
│  │         │                                                 │  │
│  │         ▼                                                 │  │
│  │  ┌─────────────────────────────────────┐                  │  │
│  │  │  Cross-compiler (riscv32-esp-elf)   │                  │  │
│  │  └──────────────────┬──────────────────┘                  │  │
│  │                     │                                     │  │
│  │                     ▼                                     │  │
│  │              firmware.bin                                 │  │
│  └─────────────────────┬─────────────────────────────────────┘  │
│                        │                                        │
│                   USB serial                                    │
│                  (esptool.py)                                   │
│                        │                                        │
└────────────────────────┼────────────────────────────────────────┘
                         │
                         ▼
        ┌──────────────────────────────┐
        │        ESP32-C3 Board        │
        │  ┌──────────┐  ┌──────────┐  │
        │  │   OLED   │  │ Blue LED │  │
        │  │  Display  │  │          │  │
        │  └──────────┘  └──────────┘  │
        └──────────────────────────────┘
```

Docker compiles the firmware, then `esptool.py` on the host flashes it to the board over USB. One command: `bin/arduino-usb-flash`.

---

## Docker toolchain

The container image includes ESP-IDF, `arduino-cli`, the `esp32:esp32` Arduino core, and common libraries (`U8g2`, `Adafruit NeoPixel`).

```bash
bin/arduino-compile sketches/hello_oled       # compile only
bin/arduino-usb-flash sketches/hello_oled     # compile + flash over USB
bin/docker-shell                              # open a shell in the container
```

> `esptool.py` is the only host-side dependency beyond Docker. It runs from a project-local `.venv` — no system Python pollution.

---

## The board

Workshop boards are the **ESP32-C3** with a 0.42" OLED display. Connect via USB-C data cable to flash. If you brought your own ESP32, let your agent know what board you have.

See `BOARD.md` for full pinout and hardware reference.

---

## Repo structure

```
CLAUDE.md       — agent instructions (Claude Code / Cursor)
AGENTS.md       — agent instructions (Codex / other agents)
BOOTSTRAP.md    — prerequisite checklist (agent use only)
BOARD.md        — hardware reference for the ESP32-C3 board
Dockerfile      — ESP-IDF + Arduino CLI image
compose.yaml    — container service definition
bin/            — repo-local wrapper commands
sketches/       — Arduino sketch source code
```
