<img src="/logo/logo.svg" alt="Luvie logo" width="200" height="200">

# Description
A free music sequencer for Linux, Mac and Windows. (On Mac and Windows the window cannot yet
be moved or resized — the custom window decorations are still to be ported.)

# The Story
The main things that make this sequencer special in the world of Linux audio are:
1. It combines harmony-based patterns (defined using chords and scales) with a song editor - basically making it a mash-up of HarmonySeq and Seq24.
1. It has the option to run as an LV2 plugin as well as standalone, simplifying session management. 
1. It has decent support for driving drumkits.
1. It has been redesigned and rebuild ground-up, so hopefully it is a bit easier to understand and use than some earlier efforts.

# Screenshots
TODO

# Manual
The [Luvie manual](docs/manual/README.md) covers installation, the song and
pattern editors, harmony patterns, MIDI output, and connecting Luvie to other
software.

# Dependencies
JACK is optional, but desirable.

# Distribution

[![Latest release](https://img.shields.io/github/v/release/benbriedis/luvie)](https://github.com/benbriedis/luvie/releases/latest)

Download from the [latest release](https://github.com/benbriedis/luvie/releases/latest). Every
download carries both the standalone application and the LV2 plugin.

| Platform | |
| --- | --- |
| Debian / Ubuntu | `.deb` |
| Fedora / openSUSE | `.rpm` |
| Arch Linux | [`luvie`](https://aur.archlinux.org/packages/luvie) in the AUR |
| macOS | `.dmg` |
| Windows | `setup.exe`, or the `.zip`, or `scoop install luvie` |

On any other Linux distribution, build from source — see [BUILD](BUILD).

Luvie is not code-signed: certificates cost money annually on both platforms. So macOS and
Windows will both question the download. The release notes say exactly what to click, and the
macOS `.dmg` carries a `README.txt` with the one command the plugin needs — without it your
DAW will not list Luvie and will not say why. Installing on Windows through
[Scoop](https://scoop.sh) sidesteps the warning entirely:

```
scoop bucket add luvie https://github.com/benbriedis/scoop-luvie
scoop install luvie
```


