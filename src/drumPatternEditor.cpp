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
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (onRightClick) onRightClick();
            return 1;
        }
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
      drumLabelInput(x + scrollbarW, y + rulerH, labelsW, rowHeight),
      paramLabels(x + scrollbarW, y + rulerH + numRows * rowHeight, labelsW),
      paramGrid(x + scrollbarW + labelsW, y + rulerH + numRows * rowHeight,
                visibleW - scrollbarW - labelsW, colWidth, snap)
{
    rulerOffsetX = scrollbarW + labelsW;

    const int gridH       = numRows * rowHeight;
    const int paramY      = y + rulerH + gridH;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    scrollbar = new GridScrollPane(x, y + rulerH, scrollbarW, gridH);
    scrollbar->linesize(1);
    scrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<DrumPatternEditor*>(d);
        auto* sb   = static_cast<GridScrollPane*>(w);
        int maxOff = std::max(0, DrumGrid::totalRows - self->drumGrid.numRows);
        self->setRowOffset(maxOff - (int)sb->value());
    }, this);

    paramScrollbar = new GridScrollPane(x, paramY, scrollbarW, kParamAreaH);
    paramScrollbar->linesize(1);
    paramScrollbar->callback([](Fl_Widget* w, void* d) {
        auto* self = static_cast<DrumPatternEditor*>(d);
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

    paramLabels.hide();
    paramGrid.hide();
    paramGrid.setNumCols(numCols);

    add(*scrollbar);
    add(*paramScrollbar);
    add(*hScrollbar);
    add(drumLabels);
    add(drumGrid);
    add(drumLabelInput);
    add(paramLabels);
    add(paramGrid);

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
    swapObserver(pattern, nullptr, this);
}

void DrumPatternEditor::setPatternPlayhead(ITransport* t, ObservablePattern* pat, int trackIndex)
{
    swapObserver(pattern, pat, this);
    playhead.setTransport(t, pat ? pat->song() : nullptr);
    playhead.setPatternTrack(trackIndex);
}

void DrumPatternEditor::setNoteLabelsContextPopup(NoteLabelsContextPopup* popup)
{
    drumLabels.onRightClick = [this, popup]() {
        if (!popup || !pattern || lastSelectedTrack < 0) return;
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack >= (int)tracks.size()) return;
        int patId = tracks[lastSelectedTrack].patternId;
        popup->open(
            Fl::event_x_root(), Fl::event_y_root(),
            [this, patId](const char* type) { return pattern->hasPatternParamLane(patId, type); },
            [this, patId](const char* type) { pattern->addPatternParamLane(patId, type); }
        );
    };
}

void DrumPatternEditor::setParamLabelsContextPopup(NoteLabelsContextPopup* popup)
{
    paramLabels.onRightClick = [this, popup](int laneId) {
        if (!popup || !pattern || lastSelectedTrack < 0) return;
        const auto& tracks = pattern->get().tracks;
        if (lastSelectedTrack >= (int)tracks.size()) return;
        int patId = tracks[lastSelectedTrack].patternId;
        std::function<void()> onRemove;
        if (laneId >= 0)
            onRemove = [this, laneId]() { pattern->removePatternParamLane(laneId); };
        popup->open(
            Fl::event_x_root(), Fl::event_y_root(),
            [this, patId](const char* type) { return pattern->hasPatternParamLane(patId, type); },
            [this, patId](const char* type) { pattern->addPatternParamLane(patId, type); },
            std::move(onRemove)
        );
    };
}

void DrumPatternEditor::setAllDrumMaps(const std::map<std::string, std::map<int, std::string>>& maps,
                                       const std::map<std::string, bool>& fallbacks)
{
    allDrumMaps      = maps;
    allFallbackModes = fallbacks;
    applyCurrentDrumMap();
}

std::string DrumPatternEditor::currentInstrumentName() const
{
    if (!pattern) return {};
    int sel = pattern->get().selectedTrackIndex;
    const auto& tracks = pattern->get().tracks;
    if (sel < 0 || sel >= (int)tracks.size()) return {};
    int patId = tracks[sel].patternId;
    for (const auto& p : pattern->get().patterns)
        if (p.id == patId) return p.outputInstrumentName;
    return {};
}

void DrumPatternEditor::startDrumLabelEdit(int midiNote, int rowY, int rowH)
{
    if (editingMidiNote >= 0) commitDrumLabelEdit();

    std::string instrName = currentInstrumentName();
    if (instrName.empty()) return;

    std::string current;
    auto mit = allDrumMaps.find(instrName);
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

    std::string instrName = currentInstrumentName();
    if (!instrName.empty()) {
        if (newLabel.empty())
            allDrumMaps[instrName].erase(note);
        else
            allDrumMaps[instrName][note] = newLabel;
        applyCurrentDrumMap();
        if (onDrumLabelChanged) onDrumLabelChanged(instrName, note, newLabel);
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
    if (!pattern) { drumLabels.setDrumMap({}); drumLabels.setFallbackNoteNames(false); return; }
    int sel = pattern->get().selectedTrackIndex;
    const auto& tracks = pattern->get().tracks;
    if (sel < 0 || sel >= (int)tracks.size()) { drumLabels.setDrumMap({}); drumLabels.setFallbackNoteNames(false); return; }
    int patId = tracks[sel].patternId;
    for (const auto& p : pattern->get().patterns) {
        if (p.id == patId) {
            auto mit = allDrumMaps.find(p.outputInstrumentName);
            drumLabels.setDrumMap(mit != allDrumMaps.end()
                ? mit->second : std::map<int, std::string>{});
            auto fit = allFallbackModes.find(p.outputInstrumentName);
            drumLabels.setFallbackNoteNames(fit != allFallbackModes.end() && fit->second);
            return;
        }
    }
    drumLabels.setDrumMap({});
    drumLabels.setFallbackNoteNames(false);
}

void DrumPatternEditor::onTimelineChanged()
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
                if (p.id == patId && p.type == PatternType::DRUM) {
                    drumGrid.setPattern(pattern, patId);
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
    paramGrid.setColOffset(colOffset);
    hScrollbar->value(colOffset, visibleCols, 0, drumGrid.numCols);

    int totalGridW = drumGrid.numCols * drumGrid.colWidth;
    if (totalGridW > drumGrid.w())
        hScrollbar->show();
    else
        hScrollbar->hide();
    redraw();
}

void DrumPatternEditor::updateParamScrollbar()
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

void DrumPatternEditor::relayout()
{
    const int bx = x(), gy = y(), bw = w(), bh = h();
    const int lanes      = paramGrid.numLanes();
    const int visRows    = std::min(lanes, kMaxVisParams);
    const int paramAreaH = visRows * kParamRowH;

    const int newNumRows   = std::max(1, (bh - rulerH - paramAreaH - hScrollH) / drumGrid.rowHeight);
    const int visibleGridW = std::max(1, bw - scrollbarW - labelsW);
    const int gridH        = newNumRows * drumGrid.rowHeight;
    const int paramY       = gy + rulerH + gridH;

    scrollbar->resize(bx, gy + rulerH, scrollbarW, gridH);
    drumLabels.setNumRows(newNumRows);
    drumLabels.resize(bx + scrollbarW, gy + rulerH, labelsW, gridH);
    drumGrid.setNumRows(newNumRows);
    drumGrid.resize(bx + scrollbarW + labelsW, gy + rulerH, visibleGridW, gridH);

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

    setRowOffset(drumGrid.getRowOffset());
    setColOffset(colOffset);
}

void DrumPatternEditor::resize(int x, int /*y*/, int w, int h)
{
    if (editingMidiNote >= 0) cancelDrumLabelEdit();
    Fl_Widget::resize(x, y(), w, h);
    relayout();
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
