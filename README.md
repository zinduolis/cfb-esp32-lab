# Vibe Hardware

> Build hardware projects with ESP32 using AI coding agents — no experience required.

From the [Coding from Beach](https://www.codingfrombeach.com) workshop series.

---

## Table of contents

- [What is this?](#what-is-this)
- [What you need](#what-you-need)
- [Getting started](#getting-started)
- [Architecture](#architecture)
- [Docker-first workflow](#docker-first-workflow)
- [The board](#the-board)
- [How it works](#how-it-works)
- [Repo structure](#repo-structure)

---

## What is this?

This repo is your starting point for the CfB vibe hardware workshop. You describe what you want your device to do, and your coding agent (Claude Code, Codex, Cursor, etc.) handles the rest — writing the code, compiling it, and flashing it to your board.

No terminal skills needed. No hardware experience needed. Just vibes.

---

## What you need

- A laptop (charged)
- Docker Desktop or Docker Engine
- A USB-C data cable
- A coding agent: Claude Code, Codex, or Cursor
- An ESP32 board - provided

---

## Getting started

Open this repo in your coding agent and say:

> "Check my bootstrap and get me set up"

The agent will take it from there and use the repo's Docker workflow instead of asking you to install firmware toolchains locally.

---

## Architecture

The diagram below shows how code flows from your laptop to the ESP32 board. Everything inside the Docker container is isolated from your host machine.

```
┌─────────────────────────────────────────────────────────────────┐
│  YOUR LAPTOP                                                    │
│                                                                 │
│  ┌───────────────┐    "make it blink"    ┌───────────────────┐  │
│  │  You (human)  │ ───────────────────── │   Coding Agent    │  │
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

**How it works:** Docker compiles the firmware, then `esptool.py` on the host flashes it to the board over USB. One command: `bin/arduino-usb-flash`.

---

## Docker-first workflow

This repo ships with a Docker toolchain for compiling Arduino sketches in `sketches/`.

Build the shared image once:

```bash
docker compose build
```

Then use the repo-local wrappers:

```bash
bin/arduino-compile sketches/hello_oled       # compile in Docker
bin/arduino-usb-flash sketches/hello_oled     # compile in Docker + flash over USB
bin/docker-shell                              # open a shell in the container
```

The container image includes:

- ESP-IDF `v5.5.3` (pinned with SHA256 digest)
- `arduino-cli` `v1.4.1` (pinned with SHA256 checksum)
- the `esp32:esp32` Arduino core
- the `U8g2` and `Adafruit NeoPixel` libraries

### USB flash setup

Docker compiles the firmware, then `esptool.py` on the host flashes it over USB. Docker Desktop on macOS cannot access USB serial devices directly, so `esptool.py` runs on the host.

One-time setup (uses a virtual environment to keep your system Python clean):

```bash
python3 -m venv .venv && .venv/bin/pip install esptool
```

The flash script auto-detects `esptool` in the `.venv` — no need to activate it. Then compile and flash in one step:

```bash
bin/arduino-usb-flash sketches/hello_oled
```

The script auto-detects the USB serial port. If you have multiple boards, pass the port explicitly:

```bash
bin/arduino-usb-flash sketches/hello_oled esp32:esp32:esp32c3:CDCOnBoot=cdc /dev/cu.usbmodem1234
```

> **Note:** `esptool.py` is the only host-side dependency beyond Docker. It is a pure-Python package installed in a project-local `.venv` to avoid polluting your system Python.

---

## The board

Workshop boards are the **ESP32-C3** with a 0.42" OLED display. They come pre-flashed with a starter project. Connect via USB-C data cable to flash new firmware.

If you brought your own ESP32, that works too — let your agent know what board you have.

---

## How it works

1. Agent checks your setup and brings up the Docker toolchain
2. Agent flashes hello world sketch to the board over USB
3. You describe what you want to build
4. Agent writes the code, compiles it, and flashes it to your board
5. Repeat

---

## Repo structure

```
CLAUDE.md       — instructions for your coding agent
AGENTS.md       — same, for non-Claude agents
BOOTSTRAP.md    — setup checklist (agent use only)
BOARD.md        — hardware reference for the ESP32-C3 board
Dockerfile      — shared ESP-IDF + Arduino CLI image
compose.yaml    — container service definition
bin/            — repo-local wrapper commands
sketches/       — Arduino sketch source code
```
