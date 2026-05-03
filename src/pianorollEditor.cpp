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
    rulerOffsetX = scrollbarW + labelsW;

    const int gridH        = numRows * rowHeight;
    const int paramY       = y + rulerH + gridH;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<PianorollEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        int maxOff = std::max(0, PianorollGrid::totalRows - self->grid.numRows);
        self->setRowOffset(maxOff - (int)sb->value());
    }, this);

    paramScrollbar = new GridScrollPane(x, paramY, scrollbarW, kParamAreaH);
    paramScrollbar->linesize(1);
    paramScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<PianorollEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->paramLaneOffset = (int)sb->value();
        self->paramGrid.setLaneOffset(self->paramLaneOffset);
        self->paramLabels.setLaneOffset(self->paramLaneOffset);
    }, this);
    paramScrollbar->hide();

    hScrollbar = new GridScrollPane(x + scrollbarW + labelsW, paramY,
                                    visibleGridW, hScrollH, GridScrollPane::HORIZONTAL);
    hScrollbar->linesize(1);
    hScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<PianorollEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setColOffset((int)sb->value());
    }, this);
    hScrollbar->hide();

    labels.position(x + scrollbarW, y + rulerH);
    grid.position(x + scrollbarW + labelsW, y + rulerH);
    grid.size(visibleGridW, gridH);
    grid.setPlayhead(&playhead);

    paramLabels.hide();
    paramGrid.hide();
    paramGrid.setNumCols(numCols);

    add(*scrollbar);
    add(*paramScrollbar);
    add(*hScrollbar);
    add(labels);
    add(grid);
    add(paramLabels);
    add(paramGrid);

    playhead.setOwner(this);
    seekingEnabled = false;

    setRowOffset(48);  // default view: centre around middle C (MIDI 60)

    int totalGridW = numCols * colWidth;
    if (totalGridW > visibleGridW) {
        hScrollbar->value(0, visibleGridW / colWidth, 0, numCols);
        hScrollbar->show();
    }

    end();
}

PianorollEditor::~PianorollEditor() = default;

void PianorollEditor::onTimelineChanged()
{
    if (!pattern) return;
    int sel = pattern->get().selectedTrackIndex;
    bool trackChanged = (sel != lastSelectedTrack);
    lastSelectedTrack = sel;

    const auto& tracks = pattern->get().tracks;
    if (sel >= 0 && sel < (int)tracks.size()) {
        int patId = tracks[sel].patternId;
        if (trackChanged) {
            playhead.setPatternTrack(sel);
            for (const auto& p : pattern->get().patterns) {
                if (p.id == patId && p.type == PatternType::PIANOROLL) {
                    grid.setPattern(pattern, patId);
                    break;
                }
            }
            paramGrid.setPattern(pattern, patId);
        } else {
            paramGrid.update(pattern, patId);
        }
        paramLabels.setPattern(pattern, patId);
        updateParamScrollbar();
    }
}
