# Sessions and saving

[← BPM and time signatures](10-beats-and-times.md) · [Contents](README.md)

## Export and import

So you started a project in Carla and now decide you want to move over to Ardour? Or maybe as a standalone?
This is what export and import is about.

You can also export projects to give you multiple versions, save backups, etc.

## Standalone project files

When run as a plugin Luvie does not save its own project file. When use New Session Manager (see below)
it does, and it will be bundled into the overall project directory.

When used in standalone mode project files have a '.luvie' suffix. These can be specified 
on the command line (see [Getting started](02-basics.md)).

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

