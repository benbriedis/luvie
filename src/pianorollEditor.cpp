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
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      labels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      grid(numRows, numCols, rowHeight, colWidth, snap, popup),
      paramLabels(x + scrollbarW, y + rulerH + numRows * rowHeight, labelsW),
      paramGrid(x + scrollbarW + labelsW, y + rulerH + numRows * rowHeight,
                visibleW - scrollbarW - labelsW, colWidth, snap)
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

    // Default view: centre around middle C (MIDI 60)
    setRowOffset(48);

    int totalGridW = numCols * colWidth;
    if (totalGridW > visibleGridW) {
        hScrollbar->value(0, visibleGridW / colWidth, 0, numCols);
        hScrollbar->show();
    }

    end();
}

PianorollEditor::~PianorollEditor()
{
    swapObserver(timeline, nullptr, this);
}

void PianorollEditor::setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex)
{
    swapObserver(timeline, tl, this);
    playhead.setTransport(t, tl);
    playhead.setPatternTrack(trackIndex);
}

void PianorollEditor::setNoteLabelsContextPopup(NoteLabelsContextPopup* popup)
{
    labels.onRightClick = [this, popup]() {
        if (!popup || !timeline || lastSelectedTrack < 0) return;
        const auto& tracks = timeline->get().tracks;
        if (lastSelectedTrack >= (int)tracks.size()) return;
        int patId = tracks[lastSelectedTrack].patternId;
        popup->open(
            Fl::event_x_root(), Fl::event_y_root(),
            [this, patId](const char* type) { return timeline->hasPatternParamLane(patId, type); },
            [this, patId](const char* type) { timeline->addPatternParamLane(patId, type); }
        );
    };
}

void PianorollEditor::onTimelineChanged()
{
    if (!timeline) return;
    int sel = timeline->get().selectedTrackIndex;
    bool trackChanged = (sel != lastSelectedTrack);
    lastSelectedTrack = sel;

    const auto& tracks = timeline->get().tracks;
    if (sel >= 0 && sel < (int)tracks.size()) {
        int patId = tracks[sel].patternId;
        if (trackChanged) {
            playhead.setPatternTrack(sel);
            for (const auto& p : timeline->get().patterns) {
                if (p.id == patId && p.type == PatternType::PIANOROLL) {
                    grid.setTimeline(timeline, patId);
                    break;
                }
            }
            paramGrid.setTimeline(timeline, patId);
        } else {
            paramGrid.update(timeline, patId);
        }
        paramLabels.setTimeline(timeline, patId);
        updateParamScrollbar();
    }
}

void PianorollEditor::setRowOffset(int offset)
{
    int maxOff = std::max(0, PianorollGrid::totalRows - grid.numRows);
    offset = std::clamp(offset, 0, maxOff);
    labels.setRowOffset(offset);
    grid.setRowOffset(offset);
    if (scrollbar)
        scrollbar->value(maxOff - offset, grid.numRows, 0, PianorollGrid::totalRows);
}

void PianorollEditor::setColOffset(int offset)
{
    if (!hScrollbar) return;
    int visibleCols = grid.w() / grid.colWidth;
    colOffset = std::clamp(offset, 0, std::max(0, grid.numCols - visibleCols));
    hScrollPixel = colOffset * grid.colWidth;
    grid.setColOffset(colOffset);
    paramGrid.setColOffset(colOffset);
    hScrollbar->value(colOffset, visibleCols, 0, grid.numCols);

    int totalGridW = grid.numCols * grid.colWidth;
    if (totalGridW > grid.w())
        hScrollbar->show();
    else
        hScrollbar->hide();
    redraw();
}

void PianorollEditor::updateParamScrollbar()
{
    int total = paramGrid.numLanes();
    if (total > kMaxVisParams) {
        int maxOff = total - kMaxVisParams;
        paramLaneOffset = std::clamp(paramLaneOffset, 0, maxOff);
        if (paramScrollbar)
            paramScrollbar->value(paramLaneOffset, kMaxVisParams, 0, total);
    } else {
        paramLaneOffset = 0;
    }
    paramGrid.setLaneOffset(paramLaneOffset);
    paramLabels.setLaneOffset(paramLaneOffset);
    relayout();
}

void PianorollEditor::relayout()
{
    const int bx = x(), gy = y(), bw = w(), bh = h();
    const int lanes      = paramGrid.numLanes();
    const int visRows    = std::min(lanes, kMaxVisParams);
    const int paramAreaH = visRows * kParamRowH;

    const int newNumRows   = std::max(1, (bh - rulerH - paramAreaH - hScrollH) / grid.rowHeight);
    const int visibleGridW = std::max(1, bw - scrollbarW - labelsW);
    const int gridH        = newNumRows * grid.rowHeight;
    const int paramY       = gy + rulerH + gridH;

    scrollbar->resize(bx, gy + rulerH, scrollbarW, gridH);
    labels.setNumRows(newNumRows);
    labels.resize(bx + scrollbarW, gy + rulerH, labelsW, gridH);
    grid.setNumRows(newNumRows);
    grid.resize(bx + scrollbarW + labelsW, gy + rulerH, visibleGridW, gridH);

    if (paramAreaH > 0) {
        paramLabels.resize(bx + scrollbarW, paramY, labelsW, paramAreaH);
        paramGrid.resize(bx + scrollbarW + labelsW, paramY, visibleGridW, paramAreaH);
        paramLabels.show();
        paramGrid.show();
        if (lanes > kMaxVisParams) {
            paramScrollbar->resize(bx, paramY, scrollbarW, paramAreaH);
            paramScrollbar->show();
        } else {
            paramScrollbar->hide();
        }
    } else {
        paramLabels.hide();
        paramGrid.hide();
        paramScrollbar->hide();
    }

    hScrollbar->resize(bx + scrollbarW + labelsW, paramY + paramAreaH, visibleGridW, hScrollH);

    setRowOffset(grid.getRowOffset());
    setColOffset(colOffset);
}

void PianorollEditor::resize(int x, int /*y*/, int w, int h)
{
    Fl_Widget::resize(x, y(), w, h);
    relayout();
}

int PianorollEditor::handle(int event)
{
    if (event == FL_MOUSEWHEEL) {
        if (Fl::event_dx() != 0)
            setColOffset(colOffset + Fl::event_dx());
        else
            setRowOffset(grid.getRowOffset() - Fl::event_dy());
        return 1;
    }
    return Editor::handle(event);
}
