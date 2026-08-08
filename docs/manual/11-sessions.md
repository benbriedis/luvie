# Sessions and saving

[← Running as an LV2 plugin](10-lv2-plugin.md) · [Contents](README.md)

## Song files

TODO: what a `.json` song file contains, saving and opening, and passing one on
the command line (see [Getting started](02-getting-started.md)).

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
[Running as an LV2 plugin](10-lv2-plugin.md).
