#include "noteLabels.hpp"
#include <FL/fl_draw.H>
#include <algorithm>

static const char* sharpNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char* flatNames[]  = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};

static const int intervals[][4] = {
    {0, 4, 7,  0},
    {0, 3, 7,  0},
    {0, 4, 7, 11},
    {0, 3, 7, 10},
};
static const int chordSizes[] = {3, 3, 4, 4};

NoteLabels::NoteLabels(int x, int y, int w, int numRows, int rowHeight)
    : Fl_Widget(x, y, w, numRows * rowHeight), numRows(numRows), rowHeight(rowHeight)
{}

int NoteLabels::computeTotalTones() const {
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = 12 + rootSemitone;
    int size         = chordSizes[chordType];
    int total        = 0;
    for (int n = 0; n < 10 * size; n++) {
        int midi = rootMidi0 + intervals[chordType][n % size] + (n / size) * 12;
        if (midi > 127) break;
        total++;
    }
    return total;
}

void NoteLabels::setParams(int root, int chord, bool sharp) {
    rootPitch  = root;
    chordType  = chord;
    useSharp   = sharp;
    totalTones = computeTotalTones();
    redraw();
}

void NoteLabels::setRowOffset(int offset) {
    rowOffset = offset;
    redraw();
}

std::string NoteLabels::noteForRow(int n) const {
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi0    = 12 + rootSemitone;
    int size         = chordSizes[chordType];
    int midi         = rootMidi0 + intervals[chordType][n % size] + (n / size) * 12;
    int noteOct      = midi / 12 - 1;
    int semitone     = midi % 12;
    const char* name = useSharp ? sharpNames[semitone] : flatNames[semitone];
    return std::string(name) + std::to_string(noteOct);
}

static void drawBtn(int x, int y, int w, int h, bool up) {
    static constexpr Fl_Color btnBg = 0x2D374800;
    static constexpr Fl_Color btnFg = 0x94A3B800;
    fl_color(btnBg);
    fl_rectf(x, y, w, h);
    int cx = x + w / 2;
    int cy = y + h / 2 + (up ? 1 : -1);
    fl_color(btnFg);
    fl_line_style(FL_SOLID, 2);
    if (up) {
        fl_line(cx - 4, cy + 2, cx,     cy - 2);
        fl_line(cx,     cy - 2, cx + 4, cy + 2);
    } else {
        fl_line(cx - 4, cy - 2, cx,     cy + 2);
        fl_line(cx,     cy + 2, cx + 4, cy - 2);
    }
    fl_line_style(0);
}

void NoteLabels::draw() {
    static constexpr Fl_Color bgCol     = 0x1F293700;
    static constexpr Fl_Color borderCol = 0x37415100;

    fl_color(bgCol);
    fl_rectf(x(), y(), w(), h());
    fl_color(borderCol);
    fl_rectf(x() + w() - 1, y(), 1, h());

    bool canUp   = rowOffset + numRows < totalTones;
    bool canDown = rowOffset > 0;

    fl_font(FL_HELVETICA, 10);
    fl_color(FL_WHITE);

    for (int r = 0; r < numRows; r++) {
        int n = rowOffset + (numRows - 1 - r);
        if (n < 0 || n >= totalTones) continue;
        std::string label = noteForRow(n);
        int ry = y() + r * rowHeight;
        int ly = ry, lh = rowHeight;
        if (r == 0 && canUp)             { ly += rowHeight; lh -= rowHeight; }
        if (r == numRows - 1 && canDown) { lh -= rowHeight; }
        if (lh > 0)
            fl_draw(label.c_str(), x(), ly, w() - 3, lh,
                    FL_ALIGN_RIGHT | FL_ALIGN_CENTER | FL_ALIGN_CLIP);
    }

    if (canUp)   drawBtn(x(), y(),                   w(), rowHeight, true);
    if (canDown) drawBtn(x(), y() + h() - rowHeight, w(), rowHeight, false);
}

int NoteLabels::handle(int event) {
    switch (event) {
    case FL_ENTER:
        return 1;
    case FL_PUSH:
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            int ey = Fl::event_y();
            if (rowOffset + numRows < totalTones && ey < y() + rowHeight) {
                if (onPageChange) onPageChange(std::max(1, numRows / 2));
                return 1;
            }
            if (rowOffset > 0 && ey > y() + h() - rowHeight) {
                if (onPageChange) onPageChange(-std::max(1, numRows / 2));
                return 1;
            }
        }
        return 1;
    default:
        return Fl_Widget::handle(event);
    }
}
