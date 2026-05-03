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
    : BasePatternEditor(x, y, visibleW, numRows, numCols, rowHeight, colWidth, snap, labelsW),
      drumLabels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      drumGrid(numRows, numCols, rowHeight, colWidth, snap, popup),
      drumLabelInput(x + scrollbarW, y + rulerH, labelsW, rowHeight)
{
    const int gridH        = numRows * rowHeight;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

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

    add(drumLabels);
    add(drumGrid);
    add(drumLabelInput);
    add(paramLabels);
    add(paramGrid);

    playhead.setOwner(this);

    setRowOffset(24);  // default view around MIDI 36 (bass drum area)

    end();
}

DrumPatternEditor::~DrumPatternEditor() = default;

void DrumPatternEditor::focusPattern()
{
    if (!pattern || lastSelectedTrack < 0) { setRowOffset(24); return; }
    const auto& tracks = pattern->get().tracks;
    if (lastSelectedTrack >= (int)tracks.size()) { setRowOffset(24); return; }
    int patId = tracks[lastSelectedTrack].patternId;
    auto notes = pattern->buildDrumPatternNotes(patId);
    if (notes.empty()) { setRowOffset(24); return; }
    int lowest = 127;
    for (const auto& n : notes) lowest = std::min(lowest, n.note);
    setRowOffset(std::max(0, lowest - 1));
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

void DrumPatternEditor::setGridPattern(int patId)
{
    for (const auto& p : pattern->get().patterns) {
        if (p.id == patId && p.type == PatternType::DRUM) {
            drumGrid.setPattern(pattern, patId);
            break;
        }
    }
}

void DrumPatternEditor::afterTimelineChanged(int /*patId*/)
{
    applyCurrentDrumMap();
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
    return BasePatternEditor::handle(event);
}
