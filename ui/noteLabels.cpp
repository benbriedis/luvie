#include "noteLabels.hpp"
#include <FL/fl_draw.H>

static const char* sharpNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char* flatNames[]  = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};

// Semitone intervals from root for each chord type
static const int intervals[][4] = {
    {0, 4, 7,  0},  // Major   (3 notes — 4th entry unused)
    {0, 3, 7,  0},  // Minor
    {0, 4, 7, 11},  // Major 7 (4 notes)
    {0, 3, 7, 10},  // Minor 7
};
static const int chordSizes[] = {3, 3, 4, 4};

NoteLabels::NoteLabels(int x, int y, int w, int numRows, int rowHeight)
    : Fl_Widget(x, y, w, numRows * rowHeight), numRows(numRows), rowHeight(rowHeight)
{}

void NoteLabels::setParams(int oct, int root, int chord, bool sharp)
{
    octave    = oct;
    rootPitch = root;
    chordType = chord;
    useSharp  = sharp;
    redraw();
}

std::string NoteLabels::noteForRow(int n) const
{
    // rootPitch index: A=0..G#=11; convert to C-based semitone: (idx+9)%12
    int rootSemitone = (rootPitch + 9) % 12;
    int rootMidi     = (octave + 1) * 12 + rootSemitone;
    int size         = chordSizes[chordType];
    int midi         = rootMidi + intervals[chordType][n % size] + (n / size) * 12;
    int noteOct      = midi / 12 - 1;
    int semitone     = midi % 12;
    const char* name = useSharp ? sharpNames[semitone] : flatNames[semitone];
    return std::string(name) + std::to_string(noteOct);
}

void NoteLabels::draw()
{
    static constexpr Fl_Color bgCol     = 0x1F293700;
    static constexpr Fl_Color borderCol = 0x37415100;

    fl_color(bgCol);
    fl_rectf(x(), y(), w(), h());
    fl_color(borderCol);
    fl_rectf(x() + w() - 1, y(), 1, h());  // right border separating from grid

    fl_font(FL_HELVETICA, 10);
    fl_color(FL_WHITE);
    for (int r = 0; r < numRows; r++) {
        int n = numRows - 1 - r;
        std::string label = noteForRow(n);
        int ry = y() + r * rowHeight;
        fl_draw(label.c_str(), x(), ry, w() - 3, rowHeight,
                FL_ALIGN_RIGHT | FL_ALIGN_CENTER | FL_ALIGN_CLIP);
    }
}
