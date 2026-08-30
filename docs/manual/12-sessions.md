# Sessions and saving

[← BPM and time signatures](11-beats-and-times.md) · [Contents](README.md)

## Export and import

So you started a project in Carla and now decide you want in Ardour? Or maybe as a standalone?
This is what export and import is about.

You can also export projects to give you multiple versions, save backups, etc.

## Song files

TODO: what a `.json` song file contains, saving and opening, and passing one on
the command line (see [Getting started](02-basics.md)).

## Auto-save

TODO: the standalone application auto-saves shortly after you stop making
changes, and saves again on exit. Say plainly what this does and does not
protect against.

## New Session Manager

Luvie is an NSM client, so a session manager such as RaySession can start, save
and stop it along with the rest of your rig.

TODO: adding Luvie to a session.

TODO: the show/hide (eye) control — the window can be hidden and restored by the
session manager.

## LV2 host sessions

When running as a plugin, the host saves Luvie's state as part of the project;
there is nothing separate to save. See
[Configuration](01-configuration.md).

