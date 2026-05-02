#include "noteLabels.hpp"
#include "chords.hpp"
#include <FL/fl_draw.H>
#include <algorithm>

static const char* sharpNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char* flatNames[]  = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};

std::string noteName(int n, int rootPitch, int chordType, bool useSharp)
{
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = rootSemitone;
    int size         = chordDefs[chordType].size;
    int midi         = rootMidi0 + chordDefs[chordType].intervals[n % size] + (n / size) * 12;
    int noteOct      = midi / 12 - 1;
    int semitone     = midi % 12;
    const char* name = useSharp ? sharpNames[semitone] : flatNames[semitone];
    return std::string(name) + std::to_string(noteOct);
}

NoteLabels::NoteLabels(int x, int y, int w, int numRows, int rowHeight)
    : Fl_Widget(x, y, w, numRows * rowHeight), numRows(numRows), rowHeight(rowHeight)
{}

int NoteLabels::computeTotalTones() const {
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = rootSemitone;
    int size         = chordDefs[chordType].size;
    int enabledTotal = 0;
    for (int n = 0; n < 10 * size; n++) {
        int midi = rootMidi0 + chordDefs[chordType].intervals[n % size] + (n / size) * 12;
        if (midi > 127) break;
        enabledTotal++;
    }
    // Convert to virtual-row count: each octave group is groupSize rows
    int numOctaves = (enabledTotal + size - 1) / size;
    return numOctaves * groupSize;
}

void NoteLabels::setParams(int root, int chord, bool sharp) {
    rootPitch  = root;
    chordType  = chord;
    chordSize  = chordDefs[chord].size;
    useSharp   = sharp;
    totalTones = computeTotalTones();
    redraw();
}

void NoteLabels::setDisabledDegrees(const std::vector<int>& dd, int gs, const std::set<int>& occupied) {
    disabledDegrees     = dd;
    groupSize           = gs;
    occupiedDisabledVPos = occupied;
    totalTones          = computeTotalTones();
    redraw();
}

void NoteLabels::setRowOffset(int offset) {
    rowOffset = offset;
    redraw();
}

// virtualPos → chord-space tone index (or -1 for disabled slot)
std::string NoteLabels::noteForRow(int virtualPos) const {
    if (groupSize <= 0) return "";
    int gs  = groupSize;
    int pos = ((virtualPos % gs) + gs) % gs;
    if (pos >= chordSize) {
        // disabled slot: show the degree name with a dash
        int ddIdx  = pos - chordSize;
        if (ddIdx >= (int)disabledDegrees.size()) return "";
        int degree = disabledDegrees[ddIdx];
        int octave = virtualPos / gs;
        int n      = octave * chordSize + degree;
        return noteName(n, rootPitch, chordType, useSharp);
    }
    int octave = virtualPos / gs;
    int n      = octave * chordSize + pos;
    return noteName(n, rootPitch, chordType, useSharp);
}

static void drawFocusBtn(int x, int y, int w, int h) {
    static constexpr Fl_Color btnBg = 0x2D374800;
    static constexpr Fl_Color btnFg = 0x94A3B800;
    fl_color(btnBg);
    fl_rectf(x, y, w, h);
    int cx = x + w / 2;
    int cy = y + h / 2;
    int r  = std::min(w, h) / 4;
    int tk = 3;  // tick length
    fl_color(btnFg);
    fl_line_style(FL_SOLID, 1);
    fl_arc(cx - r, cy - r, 2 * r, 2 * r, 0, 360);
    fl_line(cx,       cy - r - 1,   cx,       cy - r - 1 - tk);
    fl_line(cx,       cy + r + 1,   cx,       cy + r + 1 + tk);
    fl_line(cx - r - 1, cy,         cx - r - 1 - tk, cy);
    fl_line(cx + r + 1, cy,         cx + r + 1 + tk, cy);
    fl_line_style(0);
}

void NoteLabels::draw() {
    static constexpr Fl_Color bgCol     = 0x1F293700;
    static constexpr Fl_Color borderCol = 0x37415100;

    fl_color(bgCol);
    fl_rectf(x(), y(), w(), h());
    fl_color(borderCol);
    fl_rectf(x() + w() - 1, y(), 1, h());

    fl_font(FL_HELVETICA, 10);
    fl_color(FL_WHITE);

    for (int r = 0; r < numRows; r++) {
        int virtualPos = rowOffset + (numRows - 1 - r);
        if (virtualPos < 0 || virtualPos >= totalTones) continue;
        int ry = y() + r * rowHeight;

        // grey background only where a disabled note actually exists
        if (occupiedDisabledVPos.count(virtualPos)) {
            fl_color(0xCCCCCC00);
            fl_rectf(x(), ry, w() - 1, rowHeight);
        }

        // suppress label on the bottom row — occupied by focus button
        if (r == numRows - 1) continue;
        std::string label = noteForRow(virtualPos);
        fl_color(FL_WHITE);
        fl_draw(label.c_str(), x(), ry, w() - 3, rowHeight,
                FL_ALIGN_RIGHT | FL_ALIGN_CENTER | FL_ALIGN_CLIP);
    }

    drawFocusBtn(x(), y() + h() - rowHeight, w(), rowHeight);
}

int NoteLabels::handle(int event) {
    switch (event) {
    case FL_ENTER:
        return 1;
    case FL_PUSH:
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (onRightClick) onRightClick();
            return 1;
        }
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            if (Fl::event_y() >= y() + h() - rowHeight) {
                if (onFocus) onFocus();
            }
        }
        return 1;
    default:
        return Fl_Widget::handle(event);
    }
}
