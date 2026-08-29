// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "harmonyLabels.hpp"
#include "chords.hpp"
#include "cursors.hpp"
#include "modern/accidentalText.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <algorithm>

static constexpr Fl_Color flashCol     = 0x3B82F600;   // briefly lit on click
static constexpr double   flashSeconds = 0.15;

static const char* sharpNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char* flatNames[]  = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};

std::string noteName(int n, int rootPitch, int chordIndex, bool useSharp)
{
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = rootSemitone;
    int midi         = rootMidi0 + chordToneOffset(chordDefs[chordIndex], n);
    int noteOct      = midi / 12 - 1;
    int semitone     = midi % 12;
    const char* name = useSharp ? sharpNames[semitone] : flatNames[semitone];
    return std::string(name) + std::to_string(noteOct);
}

HarmonyLabels::HarmonyLabels(int x, int y, int w, int numRows, int rowHeight)
    : Fl_Widget(x, y, w, numRows * rowHeight), numRows(numRows), rowHeight(rowHeight)
{}

HarmonyLabels::~HarmonyLabels() { Fl::remove_timeout(clearFlashCb, this); }

void HarmonyLabels::flash(int virtualPos)
{
    flashVPos = virtualPos;
    Fl::remove_timeout(clearFlashCb, this);
    Fl::add_timeout(flashSeconds, clearFlashCb, this);
    redraw();
}

void HarmonyLabels::clearFlashCb(void* self)
{
    auto* nl = static_cast<HarmonyLabels*>(self);
    nl->flashVPos = -1;
    nl->redraw();
}

int HarmonyLabels::computeTotalTones() const {
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = rootSemitone;
    int size         = chordDefs[chordIndex].size;
    int enabledTotal = 0;
    for (int n = 0; n < 10 * size; n++) {
        int midi = rootMidi0 + chordToneOffset(chordDefs[chordIndex], n);
        if (midi > 127) break;
        enabledTotal++;
    }
    // Convert to virtual-row count: each pitch group is pitchGroupSize rows
    int numPitchGroups = (enabledTotal + size - 1) / size;
    return numPitchGroups * pitchGroupSize;
}

void HarmonyLabels::setParams(int root, std::string_view chordHash, bool sharp) {
    rootPitch  = root;
    chordIndex = chordIndexForHash(chordHash);
    chordSize  = chordDefs[chordIndex].size;
    useSharp   = sharp;
    totalTones = computeTotalTones();
    redraw();
}

void HarmonyLabels::setBonusDegrees(const std::vector<int>& bd, int gs) {
    bonusDegrees   = bd;
    pitchGroupSize = gs;
    totalTones     = computeTotalTones();
    redraw();
}

void HarmonyLabels::setRowOffset(int offset) {
    rowOffset = offset;
    redraw();
}

// virtualPos → the note name that row sounds, bonus rows included
std::string HarmonyLabels::noteForRow(int virtualPos) const {
    if (pitchGroupSize <= 0) return "";
    int gs  = pitchGroupSize;
    int pos = ((virtualPos % gs) + gs) % gs;
    if (pos >= chordSize) {
        // bonus row: named by the degree it kept
        int bIdx = pos - chordSize;
        if (bIdx >= (int)bonusDegrees.size()) return "";
        int degree     = bonusDegrees[bIdx];
        int pitchGroup = virtualPos / gs;
        int n          = pitchGroup * chordSize + degree;
        return noteName(n, rootPitch, chordIndex, useSharp);
    }
    int pitchGroup = virtualPos / gs;
    int n          = pitchGroup * chordSize + pos;
    return noteName(n, rootPitch, chordIndex, useSharp);
}

// visual row → MIDI pitch (canonical rowToMidi, matching playback); -1 if empty
int HarmonyLabels::midiForRow(int r) const {
    if (pitchGroupSize <= 0) return -1;
    int virtualPos = rowOffset + (numRows - 1 - r);
    if (virtualPos < 0 || virtualPos >= totalTones) return -1;
    int gs         = pitchGroupSize;
    int pos        = ((virtualPos % gs) + gs) % gs;
    int pitchGroup = virtualPos / gs;
    int n;
    if (pos >= chordSize) {
        int bIdx = pos - chordSize;
        if (bIdx >= (int)bonusDegrees.size()) return -1;
        n = pitchGroup * chordSize + bonusDegrees[bIdx];
    } else {
        n = pitchGroup * chordSize + pos;
    }
    return rowToMidi(n, rootPitch, chordIndex);
}

void HarmonyLabels::draw() {
    static constexpr Fl_Color    bgCol     = 0x1F293700;
    static constexpr Fl_Color    borderCol = 0x37415100;
    static constexpr Fl_Font     font      = FL_HELVETICA;
    static constexpr Fl_Fontsize fontSize  = 10;

    fl_color(bgCol);
    fl_rectf(x(), y(), w(), h());
    fl_color(borderCol);
    fl_rectf(x() + w() - 1, y(), 1, h());

    fl_font(font, fontSize);
    fl_color(FL_WHITE);

    for (int r = 0; r < numRows; r++) {
        int virtualPos = rowOffset + (numRows - 1 - r);
        if (virtualPos < 0 || virtualPos >= totalTones) continue;
        int ry = y() + r * rowHeight;

        // grey background on every bonus row, matching HarmonyGrid::rowBgColor
        if (pitchGroupSize > 0 && virtualPos % pitchGroupSize >= chordSize) {
            fl_color(0xCCCCCC00);
            fl_rectf(x(), ry, w() - 1, rowHeight);
        }
        if (virtualPos == flashVPos) {
            fl_color(flashCol);
            fl_rectf(x(), ry, w() - 1, rowHeight);
        }

        // Drawn with accidentalText, not fl_draw, so that the "#" and "b" in a
        // note name come out as the signs themselves. It has no clipping of its
        // own, so a name too wide for the strip is clipped here.
        std::string label = noteForRow(virtualPos);
        fl_push_clip(x(), ry, w() - 3, rowHeight);
        accidentalText::draw(label.c_str(), x(), ry, w() - 3, rowHeight,
                             FL_ALIGN_RIGHT, FL_WHITE, font, fontSize);
        fl_pop_clip();
    }

}

int HarmonyLabels::handle(int event) {
    if (int r = contextMenuCursorHandle(this, event); r >= 0) return r;
    if (event == FL_PUSH) {
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (onRightClick) onRightClick();
        } else if (Fl::event_button() == FL_LEFT_MOUSE) {
            int r = (Fl::event_y() - y()) / rowHeight;
            if (r >= 0 && r < numRows) {
                flash(rowOffset + (numRows - 1 - r));
                if (onRowClicked) onRowClicked(midiForRow(r));
            }
        }
        return 1;
    }
    return Fl_Widget::handle(event);
}
