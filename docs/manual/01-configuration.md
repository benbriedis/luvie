
# Configuration

[← MIDI output and instruments](08-midi-output.md) · [Contents](README.md) · [Running as an LV2 plugin →](10-lv2-plugin.md)

## Installation

Installers are provided for most major supported systems. 
I recommend you use one of these if possible.
Note that standalone and a plugin version of Luvie will be installed.

It is also possible to install Luvie from the source code. This should be quick
and painless - the codebase is small and almost all dependencies are static.

## JACK

Modern Linux installations ship with PipeWire that provides a mock JACK server.
JACK is an optional, but recommended, dependency for other platforms.

Jack (or PipeWire) is required for full transport support when Luvie is used as an LV2 plugin.

## Luvie standalone vs Luvie as an LV2 plugin

Luvie can be used in either one of two modes: as a standalone application, or as 
an LV2 plugin. The name "Luvie" is, in fact, a reference to LV2. Both versions are installed.

For Luvie to be used as a plugin a host program is required. Ardour and Carla are
two free programs with LV2 support that can be used. 

Use Ardour if you want to have access to audio tracks in the same project, eg for 
recording or composition. 

Use Carla if you want a simpler setup... eg if you want to use Luvie a bit more like an instrument.

The main advantage of using Luvie as a plugin is that the whole project can be stored
under one roof.  

Q: do Ardour and Carla need their plugin lists refreshed before Luvie is visible after installation?  [FILL IN]

If you prefer to run Luvie as a standalone application then note that it supports NSM (New/Non Session Manager) 
for saving projects that use multiple applications.  You will need a program such as RaySession to manage 
the project in this case.  Alternatively if you are hardcore you can choose to manage a project yourself 
(or with an AI) using scripts.


## MIDI Ports

Both JACK MIDI and native MIDI ports (ALSA, XXX, and XXX) are supported.    [FILL IN]
Its usually simpler to use JACK ports when Jack is available.

## Connecting the transport in plugin mode using JACK

When Luvie is used as a plugin (eg in Carla or Ardour) it is usually desirable
to give Luvie control over the transport. 
To do this JACK is required. (Other, more complex, options *might* be available in future).

To set up the transport in Carla:
1. Set the engine to JACK.
1. Set JACK to auto-connect.
1. Restart the server.
1. Check **Use JACK Transport** in the XXX tab on the left hand side of the Carla window.   [FILL IN]

To set up the transport in Ardour:                                                  [FILL IN]
TODO

## Standalone mode + NSM

Just run 
```
luvie 
```
or better yet
```
luvie myproject.luvie
```
on the command line.

If you are using Jack you would normally start it before Luvie. 
Luvie is chill though and should cope with Jack starting late or disappearing and reappearing.


