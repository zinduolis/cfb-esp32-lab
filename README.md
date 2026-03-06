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
- A USB-C cable for first-time recovery or non-OTA boards
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
│  │  ┌─────────────┐      ┌──────────────┐                    │  │
│  │  │ Arduino CLI │      │   ESP-IDF    │                    │  │
│  │  │             │      │   (idf.py)   │                    │  │
│  │  └──────┬──────┘      └──────┬───────┘                    │  │
│  │         │                    │                             │  │
│  │         ▼                    ▼                             │  │
│  │  ┌─────────────────────────────────────┐                  │  │
│  │  │  Cross-compiler (riscv32-esp-elf)   │                  │  │
│  │  └──────────────────┬──────────────────┘                  │  │
│  │                     │                                     │  │
│  │                     ▼                                     │  │
│  │              firmware.bin                                 │  │
│  └─────────────────────┬─────────────────────────────────────┘  │
│                        │                                        │
│               ┌────────┴────────┐                               │
│               │                 │                               │
│          WiFi / OTA         USB serial                          │
│          (preferred)        (fallback)                          │
│               │                 │                               │
└───────────────┼─────────────────┼───────────────────────────────┘
                │                 │
                ▼                 ▼
        ┌──────────────────────────────┐
        │        ESP32-C3 Board        │
        │  ┌──────────┐  ┌──────────┐  │
        │  │   OLED   │  │ Blue LED │  │
        │  │  Display  │  │          │  │
        │  └──────────┘  └──────────┘  │
        └──────────────────────────────┘
```

**WiFi/OTA flow (preferred):** The board runs a small HTTP server. The container builds the firmware, then `curl` posts the `.bin` file to `http://<board-ip>/ota`. The board flashes itself and reboots — no cables needed.

**USB flow (fallback):** Only needed if the board has no OTA firmware or WiFi is unavailable. Requires a USB-C data cable and direct serial access from the host.

---

## Docker-first workflow

This repo ships with a Docker toolchain for the two build systems it uses:

- Arduino sketches in `sketches/`
- ESP-IDF firmware in `ota_base_fw/`

Build the shared image once:

```bash
docker compose build
```

Then use the repo-local wrappers:

```bash
bin/arduino-compile sketches/hello_oled
bin/idf-build ota_base_fw
bin/idf-ota-upload 192.168.1.123
bin/docker-shell
```

The container image includes:

- ESP-IDF `v5.5.3` (pinned with SHA256 digest)
- `arduino-cli` `v1.4.1` (pinned with SHA256 checksum)
- the `esp32:esp32` Arduino core
- the `U8g2` library needed by the starter sketch

If you prefer working inside a containerized editor session, the repo also includes `.devcontainer/devcontainer.json`.

### Important limitation on macOS

Docker Desktop on macOS is great for building firmware, but it is not a complete replacement for direct USB serial device access. That means:

- Arduino and ESP-IDF builds work in Docker
- OTA uploads work in Docker
- direct USB flashing from Docker is not the reliable default path on macOS

For this workshop board that is usually fine, because the intended path is WiFi/OTA after the board has been prepared.

---

## The board

Workshop boards are the **ESP32-C3** with a 0.42" OLED display. They come pre-flashed with a starter project that connects to WiFi and shows the board's IP address on screen, enabling wireless flashing from the start.

If you brought your own ESP32, that works too — let your agent know what board you have.

---

## How it works

1. Agent checks your setup and brings up the Docker toolchain
2. Agent flashes hello world sketch to the board
3. You describe what you want to build
4. Agent writes the code, compiles it, and flashes it to your board wirelessly
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
ota_base_fw/    — ESP-IDF base firmware (WiFi + OTA server)
```
