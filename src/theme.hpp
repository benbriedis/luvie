// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <FL/Enumerations.H>
#include <functional>
#include <span>
#include <vector>

// UI colour themes. A Theme names the colours of the app's chrome — editor and
// grid backgrounds, rulers, tabs, the transport bar, popups and overlays — by
// role. Accent and status colours (notes, loops, record/mute/solo, the amber
// learn colour) and the already-dark slate panels are the same in every theme
// and stay as constants where they are used.
//
// Draw code reads theme() as it draws. Colours handed to widgets when they are
// built (btn->color(...)) are set inside ThemeObserver::onTheme(), which re-runs
// them whenever the theme changes. GUI thread only.
struct Theme {
    const char* name;

    // Editors and grids
    Fl_Color editorBg;         // editor, grid and transport-bar background
    Fl_Color rulerBg;
    Fl_Color rulerBorder;      // ruler underline, loop/marker ruler frame
    Fl_Color rulerText;
    Fl_Color gridSubdiv;       // sub-beat lines
    Fl_Color gridBeat;
    Fl_Color gridBar;
    Fl_Color gridRowLine;      // pitch row separators
    Fl_Color gridGroupLine;    // octave / pitch-group boundaries
    Fl_Color pianoBlackRow;    // black-key rows in the pianoroll
    Fl_Color unusedRow;        // harmony rows outside the scale
    Fl_Color paramRowBg;
    Fl_Color paramCentre;
    Fl_Color paramLine;
    Fl_Color paramRowSep;
    Fl_Color loopRulerBg;
    Fl_Color tempoRulerBg;
    Fl_Color timeSigRulerBg;
    Fl_Color playheadDim;

    // Tab bar, gear button and transport bar
    Fl_Color tabBarBg;
    Fl_Color tabInactive;
    Fl_Color tabSeparator;
    Fl_Color tabText;
    Fl_Color gearIcon;
    Fl_Color gearIconInactive;
    Fl_Color transportBorder;
    Fl_Color transportIcon;

    // Popups and context menus
    Fl_Color popupBg;
    Fl_Color popupText;
    Fl_Color popupInputBg;
    Fl_Color popupHover;
    Fl_Color popupDivider;
    Fl_Color popupHeading;
    Fl_Color popupCtrlBorder;  // choice / input borders inside popups

    // Overlays (Instruments and I/O, Transport, Key Bindings, Themes, ...)
    Fl_Color overlayBg;
    Fl_Color overlayBorder;    // overlay frame and its control borders
    Fl_Color overlayDivider;
    Fl_Color overlayText;
    Fl_Color overlaySubText;
    Fl_Color overlayInputBg;
    Fl_Color overlayButtonBg;  // also the hover tint of overlay choices
    Fl_Color overlayStripe;
    Fl_Color sliderTrack;

    // FLTK's own defaults, for stock widgets (Fl_Input, unstyled labels, fl_choice).
    // 0 leaves FLTK's original colour in place.
    Fl_Color fltkForeground;
    Fl_Color fltkBackground2;
};

const Theme& theme();
std::span<const Theme* const> themes();
int  themeIndex();
// Switch theme: re-applies widget colours via ThemeObserver and redraws every window.
void setTheme(int index);

// Base for anything holding theme colours outside of draw(). Registers itself on
// construction and unregisters on destruction.
class ThemeObserver {
public:
    ThemeObserver();
    ThemeObserver(const ThemeObserver&) = delete;
    ThemeObserver& operator=(const ThemeObserver&) = delete;
    virtual ~ThemeObserver();

    // Called after the theme changes, before the windows redraw. The default
    // re-runs everything registered with onTheme(); overrides should chain to it.
    virtual void onThemeChanged();

protected:
    // Runs `style` now and again after every theme change. Whatever it touches
    // must live as long as this observer.
    void onTheme(std::function<void()> style);

private:
    std::vector<std::function<void()>> stylers_;
};
