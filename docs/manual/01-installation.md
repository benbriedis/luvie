# Installation

[← Contents](README.md) · [Getting started →](02-getting-started.md)

Every release ships both a standalone application and an LV2 plugin. They are
the same sequencer; see [Running as an LV2 plugin](10-lv2-plugin.md) for what
differs.

Download from the [latest release](https://github.com/benbriedis/luvie/releases/latest).

## Linux

TODO: `.AppImage` (no installation, `chmod +x` and run), `.deb`
(`sudo apt install ./luvie_*.deb`), and `.tar.gz` (unpack anywhere).

TODO: the two `.deb`s — `luvie` and `luvie-lv2` — install independently.

## macOS

TODO: the `.zip` contains `Luvie.app` and `luvie.lv2`. Drag the app to
Applications. Mention Gatekeeper if the build is unsigned.

## Windows

TODO: unpack the `.zip` and run `luvie.exe`. Mention the SmartScreen warning if
the build is unsigned.

## Installing the LV2 plugin

Copy the `luvie.lv2` folder into whichever directory your host scans:

| Platform | Location |
| --- | --- |
| Linux | `~/.lv2` |
| macOS | `~/Library/Audio/Plug-Ins/LV2` |
| Windows | `%APPDATA%\LV2` |

The Linux `.deb` puts it on the system LV2 path for you.

## JACK

JACK is optional on every platform. Luvie looks for it at startup and uses it if
it is there; if it is not, the application falls back to its own internal
transport and sends MIDI through the platform's native interface. Nothing needs
to be configured to get this behaviour, and no JACK installation is required to
start Luvie.

TODO: what you gain by running JACK — transport sync with other applications,
and MIDI ports other JACK clients can connect to.
