# Vibe Hardware

> Build hardware projects with ESP32 using AI coding agents — no experience required.

From the [Coding from Beach](https://www.codingfrombeach.com) workshop series.

---

## What is this?

This repo is your starting point for the CfB vibe hardware workshop. You describe what you want your device to do, and your coding agent (Claude Code, Codex, Cursor, etc.) handles the rest — writing the code, compiling it, and flashing it to your board.

No terminal skills needed. No hardware experience needed. Just vibes.

---

## What you need

- A laptop (charged)
- A USB-C cable
- A coding agent: Claude Code, Codex, or Cursor
- An ESP32 board - provided

---

## Getting started

Open this repo in your coding agent and say:

> "Check my bootstrap and get me set up"

The agent will take it from there — installing any tools needed and connecting to your board.

---

## The board

Workshop boards are the **ESP32-C3** with a 0.42" OLED display. They come pre-flashed with a starter project that connects to WiFi and shows the board's IP address on screen, enabling wireless flashing from the start.

If you brought your own ESP32, that works too — let your agent know what board you have.

---

## How it works

1. Agent checks your setup and installs anything missing
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
sketches/hello_oled — starter project source
```
