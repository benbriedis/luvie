// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef MIDI_LEARN_BADGE_HPP
#define MIDI_LEARN_BADGE_HPP

#include "midiLearn.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <string>

// The MIDI-learn state of a param-lane type, drawn in a param row's label: an amber
// "Learning…" while waiting for a control, otherwise a dot, the bound control and
// its last value ("-" until one arrives). Nothing at all when the type is unbound.
// Shared by the pattern editors' param labels and the Song Editor's param rows so
// the two read the same.
inline void drawMidiLearnBadge(const MidiLearnMap* map, const std::string& type,
                               int x, int y, int w, int h, Fl_Align align)
{
    static constexpr Fl_Color kLearning = 0xF59E0B00;   // amber
    static constexpr Fl_Color kBound    = 0x22C55E00;   // green
    if (!map) return;

    fl_font(FL_HELVETICA, 9);
    if (map->isLearning(type)) {
        fl_color(kLearning);
        fl_draw("Learning…", x, y, w, h, align | FL_ALIGN_CLIP);
        return;
    }
    const MidiSrc* src = map->bindingFor(type);
    if (!src) return;

    const auto        v    = map->value(type);
    const std::string text = MidiLearnMap::describe(*src) + "  " + (v ? std::to_string(*v) : "-");
    const int         dot  = 5;
    const int         tw   = (int)fl_width(text.c_str());
    // The dot sits just before the text, wherever the alignment puts it.
    int tx = x + dot + 4;
    if (align & FL_ALIGN_RIGHT) tx = x + w - tw;
    if (tx - dot - 4 < x) tx = x + dot + 4;
    fl_color(kBound);
    fl_pie(tx - dot - 4, y + (h - dot) / 2, dot, dot, 0, 360);
    fl_draw(text.c_str(), tx, y, x + w - tx, h, FL_ALIGN_LEFT | FL_ALIGN_CLIP);
}

#endif
