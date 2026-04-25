#include "drumPatternEditor.hpp"
#include "inlineEditDispatch.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

// ---------------------------------------------------------------------------
// DrumNoteLabels
// ---------------------------------------------------------------------------

DrumNoteLabels::DrumNoteLabels(int x, int y, int w, int numRows, int rowHeight)
    : Fl_Widget(x, y, w, numRows * rowHeight),
      numRows(numRows), rowHeight(rowHeight)
{}

void DrumNoteLabels::draw()
{
    static constexpr Fl_Color bgCol     = 0x1F293700;
    static constexpr Fl_Color borderCol = 0x37415100;

    fl_color(bgCol);
    fl_rectf(x(), y(), w(), h());
    fl_color(borderCol);
    fl_rectf(x() + w() - 1, y(), 1, h());

    static constexpr const char* noteNames[] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
    };
    fl_font(FL_HELVETICA, 10);
    for (int r = 0; r < numRows; r++) {
        int midiNote = rowOffset + numRows - 1 - r;
        if (midiNote < 0 || midiNote > 127) continue;
        int ry = y() + r * rowHeight;
        fl_color(FL_WHITE);
        std::string label;
        auto it = drumMap.find(midiNote);
        if (it != drumMap.end()) {
            label = it->second;
        } else if (fallbackNoteNames) {
            label = std::string(noteNames[midiNote % 12]) + std::to_string(midiNote / 12 - 1);
        } else {
            label = std::to_string(midiNote);
        }
        fl_draw(label.c_str(), x() + 4, ry, w() - 8, rowHeight,
                FL_ALIGN_LEFT | FL_ALIGN_CENTER | FL_ALIGN_CLIP);
    }
}

int DrumNoteLabels::handle(int event)
{
    if (event == FL_PUSH) {
        if (Fl::event_clicks() >= 1 && onRowDoubleClicked) {
            int r = (Fl::event_y() - y()) / rowHeight;
            if (r >= 0 && r < numRows) {
                int midiNote = rowOffset + numRows - 1 - r;
                if (midiNote >= 0 && midiNote <= 127)
                    onRowDoubleClicked(midiNote, y() + r * rowHeight, rowHeight);
            }
        }
        return 1;
    }
    return Fl_Widget::handle(event);
}

// ---------------------------------------------------------------------------
// DrumPatternEditor
// ---------------------------------------------------------------------------

DrumPatternEditor::DrumPatternEditor(int x, int y, int visibleW, int numRows, int numCols,
                                     int rowHeight, int colWidth, float snap, Popup& popup)
    : Editor(x, y, visibleW, rulerH + numRows * rowHeight + hScrollH, numCols, colWidth),
      drumLabels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      drumGrid(numRows, numCols, rowHeight, colWidth, snap, popup),
      drumLabelInput(x + scrollbarW, y + rulerH, labelsW, rowHeight)
{
    rulerOffsetX = scrollbarW + labelsW;

    const int gridH       = numRows * rowHeight;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<DrumPatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        int maxOff = std::max(0, DrumGrid::totalRows - self->drumGrid.numRows);
        self->setRowOffset(maxOff - (int)sb->value());
    }, this);

    hScrollbar = new GridScrollPane(x + scrollbarW + labelsW, y + rulerH + gridH,
                                    visibleGridW, hScrollH, GridScrollPane::HORIZONTAL);
    hScrollbar->linesize(1);
    hScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<DrumPatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        self->setColOffset((int)sb->value());
    }, this);
    hScrollbar->hide();

    drumLabels.position(x + scrollbarW, y + rulerH);
    drumGrid.position(x + scrollbarW + labelsW, y + rulerH);
    drumGrid.size(visibleGridW, gridH);
    drumGrid.setPlayhead(&playhead);

    drumLabels.onRowDoubleClicked = [this](int midiNote, int rowY, int rh) {
        startDrumLabelEdit(midiNote, rowY, rh);
    };

    drumLabelInput.hide();
    drumLabelInput.box(FL_FLAT_BOX);
    drumLabelInput.color(0x2D374800);
    drumLabelInput.textcolor(FL_WHITE);
    drumLabelInput.cursor_color(FL_WHITE);
    drumLabelInput.when(FL_WHEN_ENTER_KEY);
    drumLabelInput.callback([](Fl_Widget*, void* d) {
        static_cast<DrumPatternEditor*>(d)->commitDrumLabelEdit();
    }, this);
    drumLabelInput.onUnfocus([this]() { commitDrumLabelEdit(); });

    add(*scrollbar);
    add(*hScrollbar);
    add(drumLabels);
    add(drumGrid);
    add(drumLabelInput);

    playhead.setOwner(this);
    seekingEnabled = false;

    // Initialise vertical scroll to show notes around MIDI 36 (bass drum area)
    const int defaultOffset = 24;
    setRowOffset(defaultOffset);

    // Initialise horizontal scroll
    int totalGridW = numCols * colWidth;
    if (totalGridW > visibleGridW) {
        hScrollbar->value(0, visibleGridW / colWidth, 0, numCols);
        hScrollbar->show();
    }

    end();
}

DrumPatternEditor::~DrumPatternEditor()
{
    swapObserver(timeline, nullptr, this);
}

void DrumPatternEditor::setPatternPlayhead(ITransport* t, ObservableTimeline* tl, int trackIndex)
{
    swapObserver(timeline, tl, this);
    playhead.setTransport(t, tl);
    playhead.setPatternTrack(trackIndex);
}

void DrumPatternEditor::setAllDrumMaps(const std::map<std::string, std::map<int, std::string>>& maps,
                                       const std::map<std::string, bool>& fallbacks)
{
    allDrumMaps      = maps;
    allFallbackModes = fallbacks;
    applyCurrentDrumMap();
}

std::string DrumPatternEditor::currentChannelName() const
{
    if (!timeline) return {};
    int sel = timeline->get().selectedTrackIndex;
    const auto& tracks = timeline->get().tracks;
    if (sel < 0 || sel >= (int)tracks.size()) return {};
    int patId = tracks[sel].patternId;
    for (const auto& p : timeline->get().patterns)
        if (p.id == patId) return p.outputChannelName;
    return {};
}

void DrumPatternEditor::startDrumLabelEdit(int midiNote, int rowY, int rowH)
{
    if (editingMidiNote >= 0) commitDrumLabelEdit();

    std::string chanName = currentChannelName();
    if (chanName.empty()) return;

    std::string current;
    auto mit = allDrumMaps.find(chanName);
    if (mit != allDrumMaps.end()) {
        auto nit = mit->second.find(midiNote);
        if (nit != mit->second.end()) current = nit->second;
    }

    editingMidiNote = midiNote;
    drumLabelInput.resize(drumLabels.x(), rowY, drumLabels.w(), rowH);
    drumLabelInput.value(current.c_str());
    drumLabelInput.show();
    drumLabelInput.take_focus();
    drumLabelInput.insert_position(drumLabelInput.size(), 0);
    InlineEditDispatch::install(this, [this]() { commitDrumLabelEdit(); });
}

void DrumPatternEditor::commitDrumLabelEdit()
{
    if (editingMidiNote < 0) return;
    int note = editingMidiNote;
    editingMidiNote = -1;
    InlineEditDispatch::uninstall();
    std::string newLabel = drumLabelInput.value();
    drumLabelInput.hide();

    std::string chanName = currentChannelName();
    if (!chanName.empty()) {
        if (newLabel.empty())
            allDrumMaps[chanName].erase(note);
        else
            allDrumMaps[chanName][note] = newLabel;
        applyCurrentDrumMap();
        if (onDrumLabelChanged) onDrumLabelChanged(chanName, note, newLabel);
    }
    redraw();
}

void DrumPatternEditor::cancelDrumLabelEdit()
{
    if (editingMidiNote < 0) return;
    editingMidiNote = -1;
    InlineEditDispatch::uninstall();
    drumLabelInput.hide();
    redraw();
}

void DrumPatternEditor::applyCurrentDrumMap()
{
    if (!timeline) { drumLabels.setDrumMap({}); drumLabels.setFallbackNoteNames(false); return; }
    int sel = timeline->get().selectedTrackIndex;
    const auto& tracks = timeline->get().tracks;
    if (sel < 0 || sel >= (int)tracks.size()) { drumLabels.setDrumMap({}); drumLabels.setFallbackNoteNames(false); return; }
    int patId = tracks[sel].patternId;
    for (const auto& p : timeline->get().patterns) {
        if (p.id == patId) {
            auto mit = allDrumMaps.find(p.outputChannelName);
            drumLabels.setDrumMap(mit != allDrumMaps.end()
                ? mit->second : std::map<int, std::string>{});
            auto fit = allFallbackModes.find(p.outputChannelName);
            drumLabels.setFallbackNoteNames(fit != allFallbackModes.end() && fit->second);
            return;
        }
    }
    drumLabels.setDrumMap({});
    drumLabels.setFallbackNoteNames(false);
}

void DrumPatternEditor::onTimelineChanged()
{
    if (!timeline) return;
    int sel = timeline->get().selectedTrackIndex;
    if (sel == lastSelectedTrack) return;
    lastSelectedTrack = sel;
    const auto& tracks = timeline->get().tracks;
    if (sel >= 0 && sel < (int)tracks.size()) {
        playhead.setPatternTrack(sel);
        int patId = tracks[sel].patternId;
        // Only load if it's a drum pattern
        for (const auto& p : timeline->get().patterns) {
            if (p.id == patId && p.type == PatternType::DRUM) {
                drumGrid.setTimeline(timeline, patId);
                break;
            }
        }
    }
    applyCurrentDrumMap();
}

void DrumPatternEditor::setRowOffset(int offset)
{
    int maxOff = std::max(0, DrumGrid::totalRows - drumGrid.numRows);
    offset = std::clamp(offset, 0, maxOff);
    drumLabels.setRowOffset(offset);
    drumGrid.setRowOffset(offset);
    if (scrollbar)
        scrollbar->value(maxOff - offset, drumGrid.numRows, 0, DrumGrid::totalRows);
}

void DrumPatternEditor::setColOffset(int offset)
{
    if (!hScrollbar) return;
    int visibleCols = drumGrid.w() / drumGrid.colWidth;
    colOffset = std::clamp(offset, 0, std::max(0, drumGrid.numCols - visibleCols));
    hScrollPixel = colOffset * drumGrid.colWidth;
    drumGrid.setColOffset(colOffset);
    hScrollbar->value(colOffset, visibleCols, 0, drumGrid.numCols);

    int totalGridW = drumGrid.numCols * drumGrid.colWidth;
    if (totalGridW > drumGrid.w())
        hScrollbar->show();
    else
        hScrollbar->hide();
    redraw();
}

void DrumPatternEditor::resize(int x, int /*y*/, int w, int h)
{
    if (editingMidiNote >= 0) cancelDrumLabelEdit();
    Fl_Widget::resize(x, y(), w, h);

    int gy           = y();
    int newNumRows   = std::max(1, (h - rulerH - hScrollH) / drumGrid.rowHeight);
    int visibleGridW = std::max(1, w - scrollbarW - labelsW);
    int gridH        = newNumRows * drumGrid.rowHeight;

    scrollbar->resize(x, gy + rulerH, scrollbarW, gridH);

    drumLabels.setNumRows(newNumRows);
    drumLabels.resize(x + scrollbarW, gy + rulerH, labelsW, gridH);

    drumGrid.setNumRows(newNumRows);
    drumGrid.resize(x + scrollbarW + labelsW, gy + rulerH, visibleGridW, gridH);

    hScrollbar->resize(x + scrollbarW + labelsW, gy + rulerH + gridH, visibleGridW, hScrollH);

    // Recompute vertical scroll
    setRowOffset(drumGrid.getRowOffset());

    // Recompute horizontal scroll
    int totalGridW = drumGrid.numCols * drumGrid.colWidth;
    if (totalGridW > visibleGridW) {
        int visibleCols = visibleGridW / drumGrid.colWidth;
        colOffset    = std::clamp(colOffset, 0, std::max(0, drumGrid.numCols - visibleCols));
        hScrollPixel = colOffset * drumGrid.colWidth;
        drumGrid.setColOffset(colOffset);
        hScrollbar->value(colOffset, visibleCols, 0, drumGrid.numCols);
        hScrollbar->show();
    } else {
        colOffset    = 0;
        hScrollPixel = 0;
        drumGrid.setColOffset(0);
        hScrollbar->hide();
    }

    redraw();
}

int DrumPatternEditor::handle(int event)
{
    if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape && editingMidiNote >= 0) {
        cancelDrumLabelEdit();
        return 1;
    }
    if (event == FL_MOUSEWHEEL) {
        if (Fl::event_dx() != 0)
            setColOffset(colOffset + Fl::event_dx());
        else
            setRowOffset(drumGrid.getRowOffset() - Fl::event_dy());
        return 1;
    }
    return Editor::handle(event);
}
