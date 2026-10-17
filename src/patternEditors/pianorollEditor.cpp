// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "pianorollEditor.hpp"
#include "inlineEditDispatch.hpp"
#include "cursors.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <algorithm>

// ---------------------------------------------------------------------------
// PianorollLabels
// ---------------------------------------------------------------------------

static constexpr Fl_Color piFlashCol     = 0x3B82F600;   // briefly lit on click
static constexpr double   piFlashSeconds = 0.15;

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

PianorollLabels::~PianorollLabels() { Fl::remove_timeout(clearFlashCb, this); }

void PianorollLabels::flash(int midi)
{
    flashMidi = midi;
    Fl::remove_timeout(clearFlashCb, this);
    Fl::add_timeout(piFlashSeconds, clearFlashCb, this);
    redraw();
}

void PianorollLabels::clearFlashCb(void* self)
{
    auto* pl = static_cast<PianorollLabels*>(self);
    pl->flashMidi = -1;
    pl->redraw();
}

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
        if (midiNote == flashMidi || liveNotes.lit(midiNote)) {
            fl_color(piFlashCol);
            fl_rectf(x(), ry, w() - 1, rowHeight);
        }
        fl_color(FL_WHITE);
        auto it = noteNames.find(midiNote);
        std::string label = it != noteNames.end() ? it->second
                          : fallbackNoteNames ? midiNoteName(midiNote)
                          : std::to_string(midiNote);
        fl_draw(label.c_str(), x(), ry, w() - 3, rowHeight,
                FL_ALIGN_RIGHT | FL_ALIGN_CENTER | FL_ALIGN_CLIP);
    }
}

bool PianorollLabels::rowAt(int ey, int& midiNote, int& rowY, int& rowH) const
{
    int r = (ey - y()) / rowHeight;
    if (ey < y() || r >= numRows) return false;
    int m = rowOffset + numRows - 1 - r;
    if (m < 0 || m > 127) return false;
    midiNote = m;
    rowY     = y() + r * rowHeight;
    rowH     = rowHeight;
    return true;
}

int PianorollLabels::handle(int event)
{
    if (int r = contextMenuCursorHandle(this, event); r >= 0) return r;
    if (event == FL_PUSH) {
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
            if (onRightClick) onRightClick();
        } else if (Fl::event_button() == FL_LEFT_MOUSE) {
            int midiNote, rowY, rowH;
            if (rowAt(Fl::event_y(), midiNote, rowY, rowH)) {
                flash(midiNote);
                if (onRowClicked) onRowClicked(midiNote);
            }
        }
        return 1;
    }
    return Fl_Widget::handle(event);
}

// ---------------------------------------------------------------------------
// PianorollEditor
// ---------------------------------------------------------------------------

PianorollEditor::PianorollEditor(int x, int y, int visibleW, int numRows, int numCols,
                                 int rowHeight, int colWidth, float snap, NoteContextPopup& popup)
    : BasePatternEditor(x, y, visibleW, numRows, numCols, rowHeight, colWidth, snap, labelsW),
      labels(x + scrollbarW, y + rulerH, labelsW, numRows, rowHeight),
      grid(numRows, numCols, rowHeight, colWidth, snap, popup),
      labelInput(x + scrollbarW, y + rulerH, labelsW, rowHeight)
{
    const int gridH        = numRows * rowHeight;
    const int visibleGridW = visibleW - scrollbarW - labelsW;

    labels.position(x + scrollbarW, y + rulerH);
    grid.position(x + scrollbarW + labelsW, y + rulerH);
    grid.size(visibleGridW, gridH);
    grid.setPlayhead(&playhead);
    grid.setSliceController(&sliceCtl, &paramGrid);
    sliceCtl.addView(&grid);

    labelInput.hide();
    labelInput.box(FL_FLAT_BOX);
    labelInput.color(0x2D374800);
    labelInput.textcolor(FL_WHITE);
    labelInput.cursor_color(FL_WHITE);
    labelInput.when(FL_WHEN_ENTER_KEY);
    labelInput.callback([](Fl_Widget*, void* d) {
        static_cast<PianorollEditor*>(d)->commitLabelEdit();
    }, this);
    labelInput.onUnfocus([this]() { commitLabelEdit(); });

    gridPane.add(labels);
    gridPane.add(grid);
    gridPane.add(labelInput);
    gridPane.add(paramLabels);
    gridPane.add(paramGrid);

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
    int patId = tracks[lastSelectedTrack].lanes.empty() ? 0 : tracks[lastSelectedTrack].lanes[0].patternId;
    auto notes = pattern->buildPatternNotes(patId);
    if (notes.empty()) { setRowOffset(48); return; }
    int lowest = 127;
    for (const auto& n : notes) lowest = std::min(lowest, n.row);
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


void PianorollEditor::afterTimelineChanged(int /*patId*/)
{
    applyCurrentNoteNames();
}

void PianorollEditor::setAllNoteNames(const std::map<int, std::map<int, std::string>>& names)
{
    allNoteNames = names;
    applyCurrentNoteNames();
}

void PianorollEditor::setNoteName(int instrId, int midiNote, const std::string& label)
{
    if (label.empty())
        allNoteNames[instrId].erase(midiNote);
    else
        allNoteNames[instrId][midiNote] = label;
    applyCurrentNoteNames();
}

// Right-click "Rename": resolve the row under the cursor now, return a closure
// that opens the inline editor on it when the menu item is chosen.
std::function<void()> PianorollEditor::labelsRenameHandler()
{
    int midiNote, rowY, rowH;
    if (!labels.rowAt(Fl::event_y(), midiNote, rowY, rowH))
        return {};
    return [this, midiNote, rowY, rowH]() { startLabelEdit(midiNote, rowY, rowH); };
}

void PianorollEditor::startLabelEdit(int midiNote, int rowY, int rowH)
{
    if (editingMidiNote >= 0) commitLabelEdit();

    int instrId = currentInstrumentId();
    if (instrId == 0) return;

    std::string current;
    auto mit = allNoteNames.find(instrId);
    if (mit != allNoteNames.end()) {
        auto nit = mit->second.find(midiNote);
        if (nit != mit->second.end()) current = nit->second;
    }

    editingMidiNote = midiNote;
    labelInput.resize(labels.x(), rowY, labels.w(), rowH);
    labelInput.value(current.c_str());
    labelInput.show();
    labelInput.take_focus();
    labelInput.insert_position(labelInput.size(), 0);
    InlineEditDispatch::install(&labelInput, [this]() { commitLabelEdit(); });
}

// An empty name clears it, falling back to the note name.
void PianorollEditor::commitLabelEdit()
{
    if (editingMidiNote < 0) return;
    int note = editingMidiNote;
    editingMidiNote = -1;
    InlineEditDispatch::uninstall();
    std::string newLabel = labelInput.value();
    labelInput.hide();

    int instrId = currentInstrumentId();
    if (instrId != 0) {
        setNoteName(instrId, note, newLabel);
        if (onNoteLabelChanged) onNoteLabelChanged(instrId, note, newLabel);
    }
    redraw();
}

void PianorollEditor::cancelLabelEdit()
{
    if (editingMidiNote < 0) return;
    editingMidiNote = -1;
    InlineEditDispatch::uninstall();
    labelInput.hide();
    redraw();
}

void PianorollEditor::applyCurrentNoteNames()
{
    const Pattern* pat = nullptr;
    if (pattern) {
        int patId = pattern->get().patternIdForSelectedLane();
        for (const auto& p : pattern->get().patterns)
            if (p.id == patId) { pat = &p; break; }
    }
    auto it = pat ? allNoteNames.find(pat->instrumentId) : allNoteNames.end();
    labels.setNoteNames(it != allNoteNames.end() ? it->second : std::map<int, std::string>{});
    labels.setFallbackNoteNames(!pat || patternShowsNoteNames(*pat));
}

void PianorollEditor::resize(int x, int y, int w, int h)
{
    if (editingMidiNote >= 0) cancelLabelEdit();
    Editor::resize(x, y, w, h);
}

int PianorollEditor::handle(int event)
{
    if (event == FL_KEYDOWN && Fl::event_key() == FL_Escape && editingMidiNote >= 0) {
        cancelLabelEdit();
        return 1;
    }
    return Editor::handle(event);
}
