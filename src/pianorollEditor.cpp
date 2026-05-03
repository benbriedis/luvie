#include "pianorollEditor.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <algorithm>

// ---------------------------------------------------------------------------
// PianorollLabels
// ---------------------------------------------------------------------------

static const char* piSharpNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

static std::string midiNoteName(int midiNote)
{
    if (midiNote < 0 || midiNote > 127) return "";
    int octave   = midiNote / 12 - 1;
    int semitone = midiNote % 12;
    return std::string(piSharpNames[semitone]) + std::to_string(octave);
}

PianorollLabels::PianorollLabels(int x, int y, int w, int numRows, int rowHeight)
    : Fl_Widget(x, y, w, numRows * rowHeight),
      numRows(numRows), rowHeight(rowHeight)
{}

void PianorollLabels::draw()
{
    static constexpr Fl_Color bgCol     = 0x1F293700;
    static constexpr Fl_Color borderCol = 0x37415100;

    fl_color(bgCol);
    fl_rectf(x(), y(), w(), h());
    fl_color(borderCol);
    fl_rectf(x() + w() - 1, y(), 1, h());

    fl_font(FL_HELVETICA, 10);
    for (int r = 0; r < numRows; r++) {
        int midiNote = rowOffset + numRows - 1 - r;
        if (midiNote < 0 || midiNote > 127) continue;
        int ry = y() + r * rowHeight;
        fl_color(FL_WHITE);
        std::string label = midiNoteName(midiNote);
        fl_draw(label.c_str(), x(), ry, w() - 3, rowHeight,
                FL_ALIGN_RIGHT | FL_ALIGN_CENTER | FL_ALIGN_CLIP);
    }
}

int PianorollLabels::handle(int event)
{
    if (event == FL_PUSH) {
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (onRightClick) onRightClick();
            return 1;
        }
        return 1;
    }
    return Fl_Widget::handle(event);
}

// ---------------------------------------------------------------------------
// PianorollEditor
// ---------------------------------------------------------------------------

PianorollEditor::PianorollEditor(int x, int y, int visibleW, int numRows, int numCols,
                                 int rowHeight, int colWidth, float snap, Popup& popup)
    : BasePatternEditor(x, y, visibleW, numRows, numCols, rowHeight, colWidth, snap, labelsW),
      labels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      grid(numRows, numCols, rowHeight, colWidth, snap, popup)
{
    const int gridH        = numRows * rowHeight;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    labels.position(x + scrollbarW, y + rulerH);
    grid.position(x + scrollbarW + labelsW, y + rulerH);
    grid.size(visibleGridW, gridH);
    grid.setPlayhead(&playhead);

    add(labels);
    add(grid);
    add(paramLabels);
    add(paramGrid);

    playhead.setOwner(this);

    setRowOffset(48);  // default view: centre around middle C (MIDI 60)

    end();
}

PianorollEditor::~PianorollEditor() = default;

void PianorollEditor::focusPattern()
{
    if (!pattern || lastSelectedTrack < 0) { setRowOffset(48); return; }
    const auto& tracks = pattern->get().tracks;
    if (lastSelectedTrack >= (int)tracks.size()) { setRowOffset(48); return; }
    int patId = tracks[lastSelectedTrack].patternId;
    auto notes = pattern->buildPatternNotes(patId);
    if (notes.empty()) { setRowOffset(48); return; }
    int lowest = 127;
    for (const auto& n : notes) lowest = std::min(lowest, n.pitch);
    setRowOffset(std::max(0, lowest - 1));
}

void PianorollEditor::setGridPattern(int patId)
{
    for (const auto& p : pattern->get().patterns) {
        if (p.id == patId && p.type == PatternType::PIANOROLL) {
            grid.setPattern(pattern, patId);
            break;
        }
    }
}
