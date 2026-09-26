// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "outputsOverlay.hpp"
#include "pluginPorts.hpp"
#include "observableInstrument.hpp"
#include "midnamParser.hpp"
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include "gridScrollPane.hpp"
#include "collapsiblePane.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl.H>
#include <algorithm>

// ── Layout ────────────────────────────────────────────────────────────────────

static constexpr int headerH  = OverlayWindow::headerH;
static constexpr int colH     = 22;
static constexpr int defaultTypeRowH = 34;  // "Default port type" row height
static constexpr int defaultLabelW   = 110; // label preceding the default-type dropdown
static constexpr int rowH     = 40;
static constexpr int inputH   = 26;
static constexpr int addBtnH  = 32;
static constexpr int addBtnW  = 150;
static constexpr int addBtnPad= 10;
static constexpr int pad      = 16;
static constexpr int delBtnSz = 26;
static constexpr int backendW = 90;   // per-port Jack/Native/Debug dropdown
static constexpr int backendGap = 8;

static constexpr int chanColH2   = 22;
static constexpr int chanGap     = 6;
static constexpr int chanMidiW   = 70;
static constexpr int typeLabelW  = 62;

static constexpr int progRowH          = 28;
// The sub-rows under an instrument's name. "MIDI input" and "MIDI output" are
// indented a little from the type label, and the rest of the output settings
// (bank, program, the drum map) a little further again, so they read as
// belonging to "MIDI output" without being pushed out past it.
static constexpr int subRowX           = pad + 40;        // "MIDI input" / "MIDI output"
static constexpr int subChildX         = subRowX + 44;    // bank, program, drum map
static constexpr int subLabelW         = 76;
static constexpr int subPortW          = 220;   // input / output port dropdowns
static constexpr int progBtnH          = 20;

static constexpr int drumRowH          = 32;
static constexpr int drumBtnH          = 22;
static constexpr int drumImportW       = 115;
static constexpr int drumGmW           = 105;
static constexpr int drumGsW           = 100;
static constexpr int drumExportW       = 115;
static constexpr int drumClearW        = 65;
static constexpr int drumFallbackLabelW = 58;
static constexpr int drumFallbackChoiceW = 90;
static constexpr int drumBtnGap        = 8;
static constexpr int scrollbarW        = OverlayWindow::scrollbarW;


// ── Colors ────────────────────────────────────────────────────────────────────

static constexpr Fl_Color bgCol      = OverlayWindow::bgCol;
static constexpr Fl_Color borderCol  = OverlayWindow::borderCol;
static constexpr Fl_Color dividerCol = OverlayWindow::dividerCol;
static constexpr Fl_Color textCol    = OverlayWindow::textCol;
static constexpr Fl_Color subTextCol = OverlayWindow::subTextCol;
static constexpr Fl_Color inputBgCol = 0xF9FAFB00;
static constexpr Fl_Color delRedCol  = 0xEF444400;
static constexpr Fl_Color addBtnBg   = 0xF3F4F600;

// ── GM1 instrument list ───────────────────────────────────────────────────────

static constexpr const char* kGm1Names[128] = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
    "Honky-Tonk Piano",     "Electric Piano 1",      "Electric Piano 2",
    "Harpsichord",          "Clavi",
    "Celesta",              "Glockenspiel",           "Music Box",
    "Vibraphone",           "Marimba",                "Xylophone",
    "Tubular Bells",        "Dulcimer",
    "Drawbar Organ",        "Percussive Organ",       "Rock Organ",
    "Church Organ",         "Reed Organ",             "Accordion",
    "Harmonica",            "Tango Accordion",
    "Acoustic Guitar (Nylon)", "Acoustic Guitar (Steel)", "Electric Guitar (Jazz)",
    "Electric Guitar (Clean)", "Electric Guitar (Muted)", "Overdriven Guitar",
    "Distortion Guitar",    "Guitar Harmonics",
    "Acoustic Bass",        "Electric Bass (Finger)", "Electric Bass (Pick)",
    "Fretless Bass",        "Slap Bass 1",            "Slap Bass 2",
    "Synth Bass 1",         "Synth Bass 2",
    "Violin",               "Viola",                  "Cello",
    "Contrabass",           "Tremolo Strings",        "Pizzicato Strings",
    "Orchestral Harp",      "Timpani",
    "String Ensemble 1",    "String Ensemble 2",      "Synth Strings 1",
    "Synth Strings 2",      "Choir Aahs",             "Voice Oohs",
    "Synth Voice",          "Orchestra Hit",
    "Trumpet",              "Trombone",               "Tuba",
    "Muted Trumpet",        "French Horn",            "Brass Section",
    "Synth Brass 1",        "Synth Brass 2",
    "Soprano Sax",          "Alto Sax",               "Tenor Sax",
    "Baritone Sax",         "Oboe",                   "English Horn",
    "Bassoon",              "Clarinet",
    "Piccolo",              "Flute",                  "Recorder",
    "Pan Flute",            "Blown Bottle",           "Shakuhachi",
    "Whistle",              "Ocarina",
    "Lead 1 (Square)",      "Lead 2 (Sawtooth)",      "Lead 3 (Calliope)",
    "Lead 4 (Chiff)",       "Lead 5 (Charang)",       "Lead 6 (Voice)",
    "Lead 7 (Fifths)",      "Lead 8 (Bass + Lead)",
    "Pad 1 (New Age)",      "Pad 2 (Warm)",           "Pad 3 (Polysynth)",
    "Pad 4 (Choir)",        "Pad 5 (Bowed)",          "Pad 6 (Metallic)",
    "Pad 7 (Halo)",         "Pad 8 (Sweep)",
    "FX 1 (Rain)",          "FX 2 (Soundtrack)",      "FX 3 (Crystal)",
    "FX 4 (Atmosphere)",    "FX 5 (Brightness)",      "FX 6 (Goblins)",
    "FX 7 (Echoes)",        "FX 8 (Sci-Fi)",
    "Sitar",                "Banjo",                  "Shamisen",
    "Koto",                 "Kalimba",                "Bag Pipe",
    "Fiddle",               "Shanai",
    "Tinkle Bell",          "Agogo",                  "Steel Drums",
    "Woodblock",            "Taiko Drum",             "Melodic Tom",
    "Synth Drum",           "Reverse Cymbal",
    "Guitar Fret Noise",    "Breath Noise",           "Seashore",
    "Bird Tweet",           "Telephone Ring",         "Helicopter",
    "Applause",             "Gunshot",
};

static int parseOptionalInt(const char* v, int lo, int hi) {
    if (!v || !v[0]) return -1;
    char* end;
    long n = std::strtol(v, &end, 10);
    if (end == v || *end) return -1;
    if (n < lo || n > hi) return -1;
    return static_cast<int>(n);
}

// ── NameInput ─────────────────────────────────────────────────────────────

class NameInput : public Fl_Input {
    void draw() override {
        Fl_Input::draw();
        // Fl_Input greys its own text when deactivated; the border is ours, so it has
        // to be greyed to match or a read-only field still looks editable.
        const bool foc = Fl::focus() == this;
        fl_color(!active_r() ? fl_inactive(borderCol) : (foc ? 0x3B82F600 : borderCol));
        fl_line_style(FL_SOLID, foc ? 2 : 1);
        fl_rect(x(), y(), w(), h());
        fl_line_style(0);
    }
public:
    NameInput(int x, int y, int w, int h) : Fl_Input(x, y, w, h) {
        when(FL_WHEN_NEVER);
        box(FL_BORDER_BOX);
    }
    int handle(int event) override {
        if (event == FL_KEYBOARD) {
            int k = Fl::event_key();
            if (k == FL_Enter || k == FL_KP_Enter) {
                do_callback();
                return 0;  // bubble to overlay for focus advance
            }
        }
        int r = Fl_Input::handle(event);
        if (event == FL_UNFOCUS) do_callback();
        return r;
    }
};

// ── Constructor ───────────────────────────────────────────────────────────────

// Item order must match the MidiBackend enum: both dropdowns treat the item index
// as the enum value. Backends the current mode cannot drive are added but greyed,
// so a port keeps showing (and keeps) the setting it was saved with.
static void fillBackendChoice(ModernChoice* c, bool pluginMode)
{
    static const char* const names[] = { "Jack", "Native", "Debug", "Plugin" };
    for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++) {
        c->add(names[i]);
        if (!backendSupported(static_cast<MidiBackend>(i), pluginMode))
            c->mode(i, c->mode(i) | FL_MENU_INACTIVE);
    }
}

// The MIDI input's type dropdown. Debug is absent (it is an output-only sink), so
// unlike fillBackendChoice() above the item index is NOT the enum value — hence
// inputBackendFromIndex/ToIndex rather than a cast. The greying rule is the same
// one the port dropdown uses, so an input keeps showing the type it was saved with
// even in a mode that cannot drive it.
static void fillInputTypeChoice(ModernChoice* c, bool pluginMode)
{
    for (int i = 0; i < kNumInputBackends; i++) {
        c->add(inputBackendName(kInputBackends[i]));
        if (!backendSupported(kInputBackends[i], pluginMode))
            c->mode(i, c->mode(i) | FL_MENU_INACTIVE);
    }
}

// "Any" then 1-16, so the item index is the stored channel value directly.
static void fillInputChanChoice(ModernChoice* c)
{
    c->add("Any");
    for (int i = 1; i <= 16; i++)
        c->add(std::to_string(i).c_str());
}

static void styleChoice(ModernChoice* c)
{
    c->color(inputBgCol);
    c->labelcolor(textCol);
    c->textsize(12);
    c->setBorderColor(borderCol);
    c->setArrowColor(subTextCol);
    c->setHoverColor(0xF3F4F600);
}

static ModernButton* makeDeleteBtn(int x, int y)
{
    auto* del = new ModernButton(x, y, delBtnSz, delBtnSz, "\xc3\x97");
    del->labelsize(14);
    del->labelcolor(delRedCol);
    del->color(bgCol);
    del->setBorderWidth(0);
    return del;
}

OutputsOverlay::OutputsOverlay(int x, int y, int w, int h, bool pluginMode)
    : OverlayWindow(x, y, w, h, "Instruments and I/O"),
      pluginMode_(pluginMode),
      defaultBackend_(defaultBackendFor(pluginMode))
{
    begin();
    auto makePane = [this](const char* title) {
        // Inset by the border, so the panes' backgrounds leave it showing.
        auto* p = new CollapsiblePane(1, headerH, this->w() - scrollbarW - 1, title);
        p->setColors(bgCol, textCol, subTextCol, dividerCol);
        p->setExpanded(false);   // everything starts folded away
        p->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<OutputsOverlay*>(d);
            self->relayoutPanes();
            self->redraw();
        }, this);
        return p;
    };
    // Inputs first: they are what most sessions start by setting up.
    inPane_    = makePane("MIDI Input Ports");
    portsPane_ = makePane("MIDI Output Ports");
    instrPane_ = makePane("Instruments");
    end();
    buildPaneBodies();

    portsPane_->begin();
    auto* dtc = new ModernChoice(0, 0, backendW, inputH);
    dtc->color(inputBgCol);
    dtc->labelcolor(textCol);
    dtc->textsize(12);
    dtc->setBorderColor(borderCol);
    dtc->setArrowColor(subTextCol);
    dtc->setHoverColor(0xF3F4F600);
    fillBackendChoice(dtc, pluginMode_);
    dtc->value(static_cast<int>(defaultBackend_));
    dtc->callback(defaultTypeChoiceCb, this);
    defaultTypeChoice = dtc;

    addBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Port");
    addBtn->labelsize(12);
    addBtn->labelcolor(textCol);
    addBtn->color(addBtnBg);
    addBtn->setBorderWidth(1);
    addBtn->setBorderColor(borderCol);
    addBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<OutputsOverlay*>(d);
        std::string name = self->nextDefaultPortName();
        self->outputs_.push_back({self->nextPortId_++, name, self->defaultBackend_});
        self->rebuildRows();
        if (self->onPortAdded) self->onPortAdded(name);
    }, this);

    portsPane_->end();

    instrPane_->begin();
    addInstrBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Instrument");
    addInstrBtn->labelsize(12);
    addInstrBtn->labelcolor(textCol);
    addInstrBtn->color(addBtnBg);
    addInstrBtn->setBorderWidth(1);
    addInstrBtn->setBorderColor(borderCol);
    addInstrBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<OutputsOverlay*>(d);
        const std::string portName = self->outputs_.empty()
            ? "" : self->outputs_[0].portName;
        const std::string name = self->nextDefaultInstrName(false);
        int id = self->instrObs_ ? self->instrObs_->add(name, false) : 0;
        self->instruments_.push_back({id, name, portName, 1, {}, false});
        self->instruments_.back().inputName = self->inputs_[0].name;
        self->rebuildInstrumentRows();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
    }, this);

    addDrumInstrBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Drumkit");
    addDrumInstrBtn->labelsize(12);
    addDrumInstrBtn->labelcolor(textCol);
    addDrumInstrBtn->color(addBtnBg);
    addDrumInstrBtn->setBorderWidth(1);
    addDrumInstrBtn->setBorderColor(borderCol);
    addDrumInstrBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<OutputsOverlay*>(d);
        const std::string portName = self->outputs_.empty()
            ? "" : self->outputs_[0].portName;
        const std::string name = self->nextDefaultInstrName(true);
        int id = self->instrObs_ ? self->instrObs_->add(name, true) : 0;
        self->instruments_.push_back({id, name, portName, 10, {}, true});
        self->instruments_.back().inputName = self->inputs_[0].name;
        self->rebuildInstrumentRows();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
    }, this);

    instrPane_->end();

    inPane_->begin();
    addInputBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Input");
    addInputBtn->labelsize(12);
    addInputBtn->labelcolor(textCol);
    addInputBtn->color(addBtnBg);
    addInputBtn->setBorderWidth(1);
    addInputBtn->setBorderColor(borderCol);
    addInputBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<OutputsOverlay*>(d);
        if ((int)self->inputs_.size() >= kMaxMidiInputs) return;
        self->syncFromInputs();
        self->inputs_.push_back({self->uniqueInputName(kDefaultMidiInputName),
                                 defaultBackendFor(self->pluginMode_)});
        self->rebuildInputRows();
        if (self->onMidiInputsChanged) self->onMidiInputsChanged();
    }, this);

    inPane_->end();

    addDefaultOutputs();
    inputs_ = { {kDefaultMidiInputName, defaultBackendFor(pluginMode_)} };
    rebuildAll();
    hide();
}

// ── Public API ────────────────────────────────────────────────────────────────

void OutputsOverlay::show() {
    if (instrObs_) {
        const auto& tlInstrs = instrObs_->get().instruments;
        for (int i = 0; i < (int)instruments_.size(); i++) {
            for (const auto& tl : tlInstrs) {
                if (tl.id != instruments_[i].id) continue;
                instruments_[i].name = tl.name;
                if (i < (int)instrRows_.size() && instrRows_[i].nameInput) {
                    instrRows_[i].nameInput->value(tl.name.c_str());
                    instrRows_[i].committedName = tl.name;
                }
                break;
            }
        }
    }
    BasePopup::show();
}

void OutputsOverlay::hide() {
    syncFromInputs();
    // Port rename safety net (for inputs that didn't fire unfocus before hide)
    for (int i = 0; i < (int)rows_.size() && i < (int)outputs_.size(); i++) {
        const std::string newName = outputs_[i].portName;
        const std::string oldName = rows_[i].committedName;
        if (newName == oldName) continue;
        for (auto& instr : instruments_)
            if (instr.portName == oldName) instr.portName = newName;
        if (onPortRenamed) onPortRenamed(oldName, newName);
    }
    // Instrument name safety net
    bool instrChanged = false;
    for (int i = 0; i < (int)instrRows_.size() && i < (int)instruments_.size(); i++) {
        if (!instrRows_[i].nameInput) continue;
        std::string newName = instrRows_[i].nameInput->value();
        const std::string oldInstrName = instrRows_[i].committedName;
        if (!newName.empty() && newName != oldInstrName) {
            newName = uniqueInstrName(newName, i);
            instruments_[i].name = newName;
            if (instrObs_) instrObs_->rename(instruments_[i].id, newName);
            instrChanged = true;
        }
    }
    if (instrChanged && onInstrumentsChanged) onInstrumentsChanged();
    OverlayWindow::hide();
}

void OutputsOverlay::onResized() {
    rebuildAll();
}

void OutputsOverlay::setOutputs(const std::vector<std::string>& portNames) {
    outputs_.clear();
    for (const auto& n : portNames)
        if (!n.empty()) outputs_.push_back({nextPortId_++, n, defaultBackend_});
    if (outputs_.empty())
        addDefaultOutputs();
    rebuildRows();
}

void OutputsOverlay::setOutputs(const std::vector<JackOutput>& ports) {
    outputs_.clear();
    for (const auto& p : ports)
        if (!p.portName.empty()) outputs_.push_back({nextPortId_++, p.portName, p.backend});
    if (outputs_.empty())
        addDefaultOutputs();
    rebuildRows();
}

// Loading a project. Like setDefaultBackend(), a stored type this mode cannot
// drive falls back to the one it can, so no input is left dead — but the greyed
// item stays in the list so it is clear what such an input could be.
void OutputsOverlay::setMidiInputs(const std::vector<MidiInputPort>& ins) {
    inputs_.clear();
    for (const auto& in : ins) {
        if (in.name.empty() || (int)inputs_.size() >= kMaxMidiInputs) continue;
        inputs_.push_back({in.name, backendSupported(in.backend, pluginMode_)
                                        ? in.backend : defaultBackendFor(pluginMode_)});
    }
    if (inputs_.empty())
        inputs_.push_back({kDefaultMidiInputName, defaultBackendFor(pluginMode_)});
    rebuildInputRows();
}

void OutputsOverlay::setAllInputBackends(MidiBackend b) {
    syncFromInputs();
    for (auto& in : inputs_) in.backend = b;
    rebuildInputRows();
}

std::vector<std::string> OutputsOverlay::getOutputs() const {
    std::vector<std::string> result;
    for (const auto& c : outputs_)
        result.push_back(c.portName);
    return result;
}

std::vector<JackOutput> OutputsOverlay::getOutputsFull() const {
    std::vector<JackOutput> result;
    for (const auto& c : outputs_)
        result.push_back({c.portName, c.backend});
    return result;
}

void OutputsOverlay::setDefaultBackend(MidiBackend backend) {
    // A project saved in the other mode can carry a default this one cannot drive;
    // new ports would then be created dead. Fall back to this mode's own default.
    if (!backendSupported(backend, pluginMode_))
        backend = defaultBackendFor(pluginMode_);
    defaultBackend_ = backend;
    if (defaultTypeChoice) defaultTypeChoice->value(static_cast<int>(backend));
}

void OutputsOverlay::setJackWarning(bool show) {
    if (jackWarning_ == show) return;
    jackWarning_ = show;
    updateSummaries();
}

void OutputsOverlay::setInstruments(const std::vector<InstrumentInfo>& instrs) {
    instruments_.clear();
    for (const auto& ci : instrs)
        instruments_.push_back({ci.id, ci.name, ci.portName, ci.midiChannel, ci.drumMap,
                                ci.isDrum, ci.fallbackNoteNames, ci.programNumber, ci.bankMsb, ci.bankLsb,
                                ci.gm1Instrument,
                                ci.inputName.empty() ? inputs_[0].name : ci.inputName,
                                std::clamp(ci.inputChannel, 0, 16)});
    rebuildInstrumentRows();
}

void OutputsOverlay::setObservableInstrument(ObservableInstrument* instr)
{
    instrObs_ = instr;
}

std::vector<OutputsOverlay::InstrumentInfo> OutputsOverlay::getInstruments() const {
    std::vector<InstrumentInfo> result;
    for (const auto& instr : instruments_)
        result.push_back({instr.id, instr.name, instr.portName, instr.midiChannel, instr.drumMap,
                          instr.isDrum, instr.fallbackNoteNames, instr.programNumber, instr.bankMsb, instr.bankLsb,
                          instr.gm1Instrument, instr.inputName, instr.inputChannel});
    return result;
}

bool OutputsOverlay::instrumentInput(int instrId, std::string& inputName, int& channel) const
{
    for (const auto& instr : instruments_) {
        if (instr.id != instrId) continue;
        inputName = instr.inputName;
        channel   = instr.inputChannel;
        return true;
    }
    return false;
}

void OutputsOverlay::updateInstrumentDrumMap(int instrId, int midiNote, const std::string& label)
{
    for (auto& instr : instruments_) {
        if (instr.id == instrId) {
            if (label.empty())
                instr.drumMap.erase(midiNote);
            else
                instr.drumMap[midiNote] = label;
            return;
        }
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

// A fresh project starts with one melodic and one drum port.
void OutputsOverlay::addDefaultOutputs() {
    outputs_.push_back({nextPortId_++, "inst1",  defaultBackend_});
    outputs_.push_back({nextPortId_++, "drums1", defaultBackend_});
}

// "inst1", "inst2", ... — the lowest number not already taken by a port.
std::string OutputsOverlay::nextDefaultPortName() const {
    auto inUse = [&](const std::string& name) {
        return portOrInputNameTaken(name, -1, -1);
    };
    for (int n = 1; ; n++) {
        std::string name = "inst" + std::to_string(n);
        if (!inUse(name)) return name;
    }
}

bool OutputsOverlay::portOrInputNameTaken(const std::string& name,
                                          int excludeOut, int excludeIn) const {
    for (int i = 0; i < (int)outputs_.size(); i++)
        if (i != excludeOut && outputs_[i].portName == name) return true;
    for (int i = 0; i < (int)inputs_.size(); i++)
        if (i != excludeIn && inputs_[i].name == name) return true;
    return false;
}

std::string OutputsOverlay::uniqueInputName(const std::string& base, int excludeIdx) const {
    if (!portOrInputNameTaken(base, -1, excludeIdx)) return base;
    for (int n = 2; ; n++) {
        std::string c = base + "_" + std::to_string(n);
        if (!portOrInputNameTaken(c, -1, excludeIdx)) return c;
    }
}

bool OutputsOverlay::inputReferenced(const std::string& name) const {
    for (const auto& instr : instruments_)
        if (instr.inputName == name) return true;
    return false;
}

std::string OutputsOverlay::uniquePortName(const std::string& base, int excludeIdx) const {
    auto isUnique = [&](const std::string& name) {
        return !portOrInputNameTaken(name, excludeIdx, -1);
    };
    if (isUnique(base)) return base;
    for (int n = 2; ; n++) {
        std::string c = base + "_" + std::to_string(n);
        if (isUnique(c)) return c;
    }
}

std::string OutputsOverlay::uniqueInstrName(const std::string& base, int excludeIdx) const {
    auto isUnique = [&](const std::string& name) {
        for (int i = 0; i < (int)instruments_.size(); i++) {
            if (i == excludeIdx) continue;
            if (instruments_[i].name == name) return false;
        }
        return true;
    };
    if (isUnique(base)) return base;
    for (int n = 2; ; n++) {
        std::string c = base + "_" + std::to_string(n);
        if (isUnique(c)) return c;
    }
}

// 1 -> "A", 2 -> "B", ..., 26 -> "Z", 27 -> "AA", 28 -> "AB", ...
static std::string letterSuffix(int n) {
    std::string s;
    while (n > 0) {
        int rem = (n - 1) % 26;
        s.insert(s.begin(), char('A' + rem));
        n = (n - 1) / 26;
    }
    return s;
}

std::string OutputsOverlay::nextDefaultInstrName(bool isDrum) const {
    const std::string prefix = isDrum ? "Drums " : "Instrument ";
    auto inUse = [&](const std::string& name) {
        for (const auto& instr : instruments_)
            if (instr.name == name) return true;
        return false;
    };
    for (int n = 1; ; n++) {
        std::string name = prefix + letterSuffix(n);
        if (!inUse(name)) return name;
    }
}

std::string OutputsOverlay::displayInputName(int i) const {
    if (!pluginMode_ || inputs_[i].backend != MidiBackend::Plugin)
        return inputs_[i].name;
    const int idx = pluginInputIndex(inputs_, inputs_[i].name);
    return pluginInputName(idx < 0 ? 0 : idx);
}

std::string OutputsOverlay::displayPortName(int i) const {
    if (!pluginMode_ || outputs_[i].backend != MidiBackend::Plugin)
        return outputs_[i].portName;
    // Positional, exactly as the DSP resolves it: count the Plugin-backed ports
    // ahead of this one. Keep the two in step — pluginPorts.hpp owns the rule.
    int slot = 0;
    for (int j = 0; j < i; j++)
        if (outputs_[j].backend == MidiBackend::Plugin) slot++;
    if (slot >= kMaxPluginOutputs) slot = kMaxPluginOutputs - 1;   // overflow shares the last
    return pluginOutputName(slot);
}

void OutputsOverlay::syncFromInputs() {
    for (int i = 0; i < (int)rows_.size() && i < (int)outputs_.size(); i++)
        // Skip a disabled input: it is showing a derived name, not the port's own,
        // so reading it back would overwrite the real name with the LV2 output's.
        if (rows_[i].input && rows_[i].input->active())
            outputs_[i].portName = rows_[i].input->value();
    // An input's rename has to reach the instruments and the live port, so an
    // uncommitted edit goes through the same path as Enter would have taken it.
    for (int i = 0; i < (int)inRows_.size() && i < (int)inputs_.size(); i++) {
        Fl_Input* in = inRows_[i].input;
        if (in && in->active() && inRows_[i].committedName != in->value())
            inputNameCb(in, this);
    }
}

// Removes a row widget from whichever pane holds it and deletes it.
static void discard(Fl_Widget* w)
{
    if (!w) return;
    if (w->parent()) w->parent()->remove(w);
    Fl::delete_widget(w);
}

static std::string countOf(int n, const char* one, const char* many)
{
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

void OutputsOverlay::rebuildAll() {
    rebuildInputRows();
    rebuildRows();
    rebuildInstrumentRows();
}

void OutputsOverlay::updateSummaries() {
    inPane_->setSummary(countOf((int)inputs_.size(), "input", "inputs"));
    // The warning goes on the heading, so it is seen with the section folded too.
    if (jackWarning_) portsPane_->setSummary("JACK server not running", delRedCol);
    else              portsPane_->setSummary(countOf((int)outputs_.size(), "port", "ports"));
    instrPane_->setSummary(countOf((int)instruments_.size(), "instrument", "instruments"));
}

void OutputsOverlay::relayoutPanes() {
    CollapsiblePane* const panes[] = { inPane_, portsPane_, instrPane_ };
    // Size the extent first: updateScrollbar() may clamp scrollY_, and the panes
    // have to be placed with the clamped value.
    int total = 0;
    for (auto* p : panes) total += p->h();
    totalContentH_ = total + addBtnPad;
    updateScrollbar();

    int y = headerH - scrollY_;
    for (auto* p : panes) {
        p->resize(1, y, w() - scrollbarW - 1, p->h());   // carries the body with it
        y += p->h();
    }
    updateSummaries();
    redraw();
}

void OutputsOverlay::buildPaneBodies() {
    auto colHeading = [](const char* text, int x, int y, int w) {
        fl_font(FL_HELVETICA, 10);
        fl_color(subTextCol);
        fl_draw(text, x, y, w, chanColH2, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    };
    auto rule = [this](int y) {
        fl_color(dividerCol);
        fl_line_style(FL_SOLID, 1);
        fl_line(pad, y, w() - scrollbarW - pad, y);
        fl_line_style(0);
    };

    inPane_->drawBody = [=]() {
        const int top = inPane_->bodyY();
        colHeading("PORT NAME", pad, top, w() - 2*pad);
        rule(top + chanColH2);
    };
    portsPane_->drawBody = [=]() {
        const int top = portsPane_->bodyY();
        fl_font(FL_HELVETICA, 11);
        fl_color(subTextCol);
        fl_draw("Default port type", pad, top, defaultLabelW, inputH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        colHeading("PORT NAME", pad, top + defaultTypeRowH, w() - 2*pad);
        rule(top + defaultTypeRowH + colH);
    };
    instrPane_->drawBody = [=]() {
        const int top   = instrPane_->bodyY();
        const int nameX = pad + typeLabelW + chanGap;
        colHeading("TYPE", pad,   top, typeLabelW);
        colHeading("NAME", nameX, top, instrNameW_);
        rule(top + chanColH2);
    };
}

void OutputsOverlay::rebuildRows() {
    for (auto& row : rows_) {
        if (row.input)         discard(row.input);
        if (row.backendChoice) discard(row.backendChoice);
        if (row.deleteBtn)     discard(row.deleteBtn);
    }
    rows_.clear();

    const int top = portsPane_->bodyY();
    defaultTypeChoice->position(pad + defaultLabelW + backendGap, top);

    const int inputW = w() - scrollbarW - 2*pad - delBtnSz - 8 - backendW - backendGap;
    int y = top + defaultTypeRowH + colH;

    portsPane_->begin();
    for (int i = 0; i < (int)outputs_.size(); i++) {
        const int iy = y + (rowH - inputH) / 2;

        auto* inp = new NameInput(pad, iy, inputW, inputH);
        inp->color(inputBgCol);
        inp->textcolor(textCol);
        inp->textsize(12);
        inp->value(displayPortName(i).c_str());
        inp->callback(inputCb, this);
        // Hosted, a Plugin port's name is the LV2 output it drives, so it is shown
        // rather than entered. syncFromInputs() skips inactive inputs, which is what
        // keeps the port's real name from being overwritten by the displayed one.
        if (pluginMode_ && outputs_[i].backend == MidiBackend::Plugin)
            inp->deactivate();

        const int backendX = pad + inputW + backendGap;
        auto* be = new ModernChoice(backendX, iy, backendW, inputH);
        be->color(inputBgCol);
        be->labelcolor(textCol);
        be->textsize(12);
        be->setBorderColor(borderCol);
        be->setArrowColor(subTextCol);
        be->setHoverColor(0xF3F4F600);
        fillBackendChoice(be, pluginMode_);
        be->value(static_cast<int>(outputs_[i].backend));
        be->callback(backendChoiceCb, this);

        auto* del = new ModernButton(
            w() - scrollbarW - pad - delBtnSz, y + (rowH - delBtnSz) / 2,
            delBtnSz, delBtnSz, "\xc3\x97");
        del->labelsize(14);
        del->labelcolor(delRedCol);
        del->color(bgCol);
        del->setBorderWidth(0);
        del->callback(deleteCb, this);

        const std::string& pname = outputs_[i].portName;
        bool referenced = std::any_of(instruments_.begin(), instruments_.end(),
            [&](const Instrument& instr){ return instr.portName == pname; });
        if (referenced) del->deactivate();

        rows_.push_back({inp, be, del, outputs_[i].portName});
        y += rowH;
    }
    portsPane_->end();

    addBtn->position(w() - scrollbarW - pad - addBtnW, y + addBtnPad);
    y += addBtnPad + addBtnH + addBtnPad;

    portsPane_->setBodyHeight(y - top);
    // The instruments' port dropdowns list these ports, so they follow.
    rebuildPortChoices();
    relayoutPanes();
}

void OutputsOverlay::rebuildInstrumentRows() {
    for (auto& row : instrRows_) {
        if (row.typeLabel)     discard(row.typeLabel);
        if (row.nameInput)     discard(row.nameInput);
        if (row.portChoice)    discard(row.portChoice);
        if (row.midiChanChoice)discard(row.midiChanChoice);
        if (row.deleteBtn)     discard(row.deleteBtn);
        if (row.importBtn)      discard(row.importBtn);
        if (row.gmBtn)          discard(row.gmBtn);
        if (row.gsBtn)          discard(row.gsBtn);
        if (row.exportBtn)      discard(row.exportBtn);
        if (row.clearBtn)       discard(row.clearBtn);
        if (row.fallbackLabel)   discard(row.fallbackLabel);
        if (row.fallbackChoice)  discard(row.fallbackChoice);
        if (row.programInput)    discard(row.programInput);
        if (row.programDropdown) discard(row.programDropdown);
        if (row.bankMsbInput)    discard(row.bankMsbInput);
        if (row.bankLsbInput)    discard(row.bankLsbInput);
        if (row.bankLabel)       discard(row.bankLabel);
        if (row.msbLabel)        discard(row.msbLabel);
        if (row.lsbLabel)        discard(row.lsbLabel);
        if (row.progLabel)       discard(row.progLabel);
        if (row.gm1Label)        discard(row.gm1Label);
        if (row.inputLabel)      discard(row.inputLabel);
        if (row.inputChoice)     discard(row.inputChoice);
        if (row.inChanLabel)     discard(row.inChanLabel);
        if (row.inChanChoice)    discard(row.inChanChoice);
        if (row.outputLabel)     discard(row.outputLabel);
        if (row.outChanLabel)    discard(row.outChanLabel);
    }
    instrRows_.clear();

    // The name has the main row to itself; everything else is on the labelled
    // sub-rows below it, input settings first, then output.
    instrNameW_ = w() - scrollbarW - 2*pad - delBtnSz - 8 - typeLabelW - chanGap;

    const int top = instrPane_->bodyY();
    int y = top + chanColH2;

    instrPane_->begin();
    for (int i = 0; i < (int)instruments_.size(); i++) {
        const int iy    = y + (rowH - inputH) / 2;
        const bool drum = instruments_[i].isDrum;

        // Type label
        auto* typeLbl = new Fl_Box(pad, iy, typeLabelW, inputH,
            drum ? "Drum" : "Standard");
        typeLbl->box(FL_NO_BOX);
        typeLbl->labelcolor(subTextCol);
        typeLbl->labelsize(10);
        // Flush with the TYPE heading: centred, a short word like "Drum" would
        // look indented next to "Standard".
        typeLbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        const int nameX = pad + typeLabelW + chanGap;
        auto* nameInp = new NameInput(nameX, iy, instrNameW_, inputH);
        nameInp->color(inputBgCol);
        nameInp->textcolor(textCol);
        nameInp->textsize(12);
        nameInp->value(instruments_[i].name.c_str());
        nameInp->callback(instrNameCb, this);

        auto* del = new ModernButton(
            w() - scrollbarW - pad - delBtnSz, y + (rowH - delBtnSz) / 2,
            delBtnSz, delBtnSz, "\xc3\x97");
        del->labelsize(14);
        del->labelcolor(delRedCol);
        del->color(bgCol);
        del->setBorderWidth(0);
        del->callback(instrDeleteCb, this);

        {
            int typeCount = 0;
            for (const auto& instr : instruments_) if (instr.isDrum == drum) ++typeCount;
            bool inUse = isInstrumentInUse && isInstrumentInUse(instruments_[i].id);
            if (typeCount <= 1 || inUse) del->deactivate();
        }

        // Drum mappings sub-row — only for drum kits, and last, under program
        ModernButton* imp  = nullptr;
        ModernButton* gm   = nullptr;
        ModernButton* gs   = nullptr;
        ModernButton* exp  = nullptr;
        ModernButton* clr  = nullptr;
        Fl_Box*       fbLbl = nullptr;
        Fl_Choice*    fbCh  = nullptr;
        if (drum) {
            const int drumBtnY = y + rowH + 4 * progRowH + (drumRowH - drumBtnH) / 2;

            const int drumStartX = subChildX;
            imp = new ModernButton(drumStartX, drumBtnY, drumImportW, drumBtnH, "Import drum map");
            imp->labelsize(11);
            imp->labelcolor(textCol);
            imp->color(addBtnBg);
            imp->setBorderWidth(1);
            imp->setBorderColor(borderCol);
            imp->callback(importDrumMapCb, this);

            const int gmX = drumStartX + drumImportW + drumBtnGap;
            gm = new ModernButton(gmX, drumBtnY, drumGmW, drumBtnH, "Load GM map");
            gm->labelsize(11);
            gm->labelcolor(textCol);
            gm->color(addBtnBg);
            gm->setBorderWidth(1);
            gm->setBorderColor(borderCol);
            gm->callback(loadGmMapCb, this);

            const int gsX = gmX + drumGmW + drumBtnGap;
            gs = new ModernButton(gsX, drumBtnY, drumGsW, drumBtnH, "Load GS map");
            gs->labelsize(11);
            gs->labelcolor(textCol);
            gs->color(addBtnBg);
            gs->setBorderWidth(1);
            gs->setBorderColor(borderCol);
            gs->callback(loadGsMapCb, this);

            const int expX = gsX + drumGsW + drumBtnGap;
            exp = new ModernButton(expX, drumBtnY, drumExportW, drumBtnH, "Export drum map");
            exp->labelsize(11);
            exp->labelcolor(textCol);
            exp->color(addBtnBg);
            exp->setBorderWidth(1);
            exp->setBorderColor(borderCol);
            exp->callback(exportDrumMapCb, this);

            const int clrX = expX + drumExportW + drumBtnGap;
            clr = new ModernButton(clrX, drumBtnY, drumClearW, drumBtnH, "Clear");
            clr->labelsize(11);
            clr->labelcolor(textCol);
            clr->color(addBtnBg);
            clr->setBorderWidth(1);
            clr->setBorderColor(borderCol);
            clr->callback(clearDrumMapCb, this);

            const int fbLabelX  = clrX + drumClearW + drumBtnGap;
            const int fbChoiceX = fbLabelX + drumFallbackLabelW;
            fbLbl = new Fl_Box(fbLabelX, drumBtnY, drumFallbackLabelW, drumBtnH, "Fallback");
            fbLbl->box(FL_NO_BOX);
            fbLbl->labelcolor(subTextCol);
            fbLbl->labelsize(11);
            fbLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

            auto* fbMc = new ModernChoice(fbChoiceX, drumBtnY, drumFallbackChoiceW, drumBtnH);
            fbMc->color(inputBgCol);
            fbMc->labelcolor(textCol);
            fbMc->textsize(11);
            fbMc->setBorderColor(borderCol);
            fbMc->setArrowColor(subTextCol);
            fbMc->setHoverColor(0xF3F4F600);
            fbMc->add("Notes");
            fbMc->add("Numbers");
            fbMc->value(instruments_[i].fallbackNoteNames ? 0 : 1);
            fbMc->callback(fallbackChoiceCb, this);
            fbCh = fbMc;
        }

        // MIDI input sub-row — where the instrument is played from, all channels
        const int inSubY = y + rowH;
        const int inWidY = inSubY + (progRowH - progBtnH) / 2;

        int ix = subRowX;
        auto* inLbl = new Fl_Box(ix, inWidY, subLabelW, progBtnH, "MIDI input");
        inLbl->box(FL_NO_BOX);
        inLbl->labelcolor(subTextCol);
        inLbl->labelsize(11);
        inLbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ix += subLabelW;

        auto* inCh = new ModernChoice(ix, inWidY, subPortW, progBtnH);
        styleChoice(inCh);
        inCh->textsize(11);
        inCh->callback(instrInputCb, this);   // filled by rebuildInputChoices()
        ix += subPortW + 12;

        auto* inChanLbl = new Fl_Box(ix, inWidY, 50, progBtnH, "Channel");
        inChanLbl->box(FL_NO_BOX);
        inChanLbl->labelcolor(subTextCol);
        inChanLbl->labelsize(11);
        inChanLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        ix += 50 + 4;

        auto* inChanCh = new ModernChoice(ix, inWidY, chanMidiW, progBtnH);
        styleChoice(inChanCh);
        inChanCh->textsize(11);
        fillInputChanChoice(inChanCh);
        inChanCh->value(instruments_[i].inputChannel);
        inChanCh->callback(instrInChanCb, this);

        // MIDI output sub-row — where the instrument sends, laid out to match
        const int outSubY = inSubY + progRowH;
        const int outWidY = outSubY + (progRowH - progBtnH) / 2;

        int ox = subRowX;
        auto* outLbl = new Fl_Box(ox, outWidY, subLabelW, progBtnH, "MIDI output");
        outLbl->box(FL_NO_BOX);
        outLbl->labelcolor(subTextCol);
        outLbl->labelsize(11);
        outLbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        ox += subLabelW;

        auto* portCh = new ModernChoice(ox, outWidY, subPortW, progBtnH);
        styleChoice(portCh);
        portCh->textsize(11);
        int selPort = 0;
        for (int j = 0; j < (int)outputs_.size(); j++) {
            portCh->add(displayPortName(j).c_str());
            if (outputs_[j].portName == instruments_[i].portName) selPort = j;
        }
        portCh->value(selPort);
        portCh->callback(portChoiceCb, this);
        ox += subPortW + 12;

        auto* outChanLbl = new Fl_Box(ox, outWidY, 50, progBtnH, "Channel");
        outChanLbl->box(FL_NO_BOX);
        outChanLbl->labelcolor(subTextCol);
        outChanLbl->labelsize(11);
        outChanLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        ox += 50 + 4;

        auto* midiCh = new ModernChoice(ox, outWidY, chanMidiW, progBtnH);
        styleChoice(midiCh);
        midiCh->textsize(11);
        for (int ch = 1; ch <= 16; ch++)
            midiCh->add(std::to_string(ch).c_str());
        midiCh->value(instruments_[i].midiChannel - 1);
        midiCh->callback(midiChanChoiceCb, this);

        // Bank sub-row — above program row, all channels
        const int bankSubY = outSubY + progRowH;
        const int bankWidY = bankSubY + (progRowH - progBtnH) / 2;

        int bx = subChildX;
        auto* bankLbl = new Fl_Box(bx, bankWidY, 40, progBtnH, "Bank:");
        bankLbl->box(FL_NO_BOX);
        bankLbl->labelcolor(subTextCol);
        bankLbl->labelsize(11);
        bankLbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        bx += 40;

        auto* msbLbl = new Fl_Box(bx, bankWidY, 28, progBtnH, "MSB");
        msbLbl->box(FL_NO_BOX);
        msbLbl->labelcolor(subTextCol);
        msbLbl->labelsize(11);
        msbLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        bx += 28 + 4;

        auto* msbInp = new NameInput(bx, bankWidY, 38, progBtnH);
        msbInp->color(inputBgCol);
        msbInp->textcolor(textCol);
        msbInp->textsize(11);
        msbInp->callback(bankMsbInputCb, this);
        if (instruments_[i].bankMsb >= 0)
            msbInp->value(std::to_string(instruments_[i].bankMsb).c_str());
        bx += 38 + 12;

        auto* lsbLbl = new Fl_Box(bx, bankWidY, 28, progBtnH, "LSB");
        lsbLbl->box(FL_NO_BOX);
        lsbLbl->labelcolor(subTextCol);
        lsbLbl->labelsize(11);
        lsbLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        bx += 28 + 4;

        auto* lsbInp = new NameInput(bx, bankWidY, 38, progBtnH);
        lsbInp->color(inputBgCol);
        lsbInp->textcolor(textCol);
        lsbInp->textsize(11);
        lsbInp->callback(bankLsbInputCb, this);
        if (instruments_[i].bankLsb >= 0)
            lsbInp->value(std::to_string(instruments_[i].bankLsb).c_str());

        // Program / instrument sub-row — all channels
        const int progSubY = bankSubY + progRowH;
        const int progWidY = progSubY + (progRowH - progBtnH) / 2;

        int px = subChildX;
        auto* progLbl = new Fl_Box(px, progWidY, 100, progBtnH, "Program number");
        progLbl->box(FL_NO_BOX);
        progLbl->labelcolor(subTextCol);
        progLbl->labelsize(11);
        progLbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        px += 100;

        auto* progInp = new NameInput(px, progWidY, 40, progBtnH);
        progInp->color(inputBgCol);
        progInp->textcolor(textCol);
        progInp->textsize(11);
        progInp->callback(programInputCb, this);
        if (instruments_[i].programNumber >= 0)
            progInp->value(std::to_string(instruments_[i].programNumber + 1).c_str());
        px += 40 + 12;

        auto* gm1Lbl = new Fl_Box(px, progWidY, 125, progBtnH, "Set to GM1 instrument");
        gm1Lbl->box(FL_NO_BOX);
        gm1Lbl->labelcolor(subTextCol);
        gm1Lbl->labelsize(11);
        gm1Lbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        px += 125 + 4;

        auto* progDrop = new ModernChoice(px, progWidY, 170, progBtnH);
        progDrop->color(inputBgCol);
        progDrop->labelcolor(textCol);
        progDrop->textsize(11);
        progDrop->setBorderColor(borderCol);
        progDrop->setArrowColor(subTextCol);
        progDrop->setHoverColor(0xF3F4F600);
        progDrop->add("(none)");
        for (int n = 0; n < 128; n++) {
            std::string item = std::to_string(n + 1) + " \xe2\x80\x93 " + kGm1Names[n];
            progDrop->add(item.c_str());
        }
        progDrop->value(instruments_[i].gm1Instrument >= 0 ? instruments_[i].gm1Instrument + 1 : 0);
        progDrop->callback(programDropdownCb, this);

        instrRows_.push_back({typeLbl, nameInp, portCh, midiCh, del, imp, gm, gs, exp, clr, fbLbl, fbCh,
                              progInp, progDrop, msbInp, lsbInp,
                              bankLbl, msbLbl, lsbLbl, progLbl, gm1Lbl,
                              inLbl, inCh, inChanLbl, inChanCh, outLbl, outChanLbl,
                              instruments_[i].name});
        y += rowH + 4 * progRowH + (drum ? drumRowH : 0);
    }
    instrPane_->end();

    addInstrBtn->position(w() - scrollbarW - pad - addBtnW, y + addBtnPad);
    addDrumInstrBtn->position(w() - scrollbarW - pad - 2*addBtnW - chanGap, y + addBtnPad);

    y += addBtnPad + addBtnH + addBtnPad;

    instrPane_->setBodyHeight(y - top);
    // Labels the port dropdowns as the port rows show them, and re-checks which
    // ports are now referenced and so cannot be deleted; likewise the inputs.
    rebuildPortChoices();
    rebuildInputChoices();
    relayoutPanes();
}

void OutputsOverlay::rebuildInputRows() {
    for (auto& row : inRows_) {
        if (row.input)      discard(row.input);
        if (row.typeChoice) discard(row.typeChoice);
        if (row.deleteBtn)  discard(row.deleteBtn);
    }
    inRows_.clear();

    const int inputW = w() - scrollbarW - 2*pad - delBtnSz - 8 - backendW - backendGap;
    const int top = inPane_->bodyY();
    int y = top + chanColH2;

    inPane_->begin();
    for (int i = 0; i < (int)inputs_.size(); i++) {
        const int iy = y + (rowH - inputH) / 2;

        auto* inp = new NameInput(pad, iy, inputW, inputH);
        inp->color(inputBgCol);
        inp->textcolor(textCol);
        inp->textsize(12);
        inp->value(displayInputName(i).c_str());
        inp->callback(inputNameCb, this);
        // As for the ports: hosted, a Plugin input's name is the LV2 input it
        // listens on, shown rather than entered.
        if (pluginMode_ && inputs_[i].backend == MidiBackend::Plugin)
            inp->deactivate();

        auto* tc = new ModernChoice(pad + inputW + backendGap, iy, backendW, inputH);
        styleChoice(tc);
        fillInputTypeChoice(tc, pluginMode_);
        tc->value(std::max(0, inputBackendToIndex(inputs_[i].backend)));
        tc->callback(inputTypeCb, this);

        auto* del = makeDeleteBtn(w() - scrollbarW - pad - delBtnSz,
                                  y + (rowH - delBtnSz) / 2);
        del->callback(inputDeleteCb, this);
        if (inputReferenced(inputs_[i].name)) del->deactivate();

        inRows_.push_back({inp, tc, del, inputs_[i].name});
        y += rowH;
    }
    inPane_->end();

    addInputBtn->position(w() - scrollbarW - pad - addBtnW, y + addBtnPad);
    if ((int)inputs_.size() >= kMaxMidiInputs) addInputBtn->deactivate();
    else                                       addInputBtn->activate();
    y += addBtnPad + addBtnH + addBtnPad;

    inPane_->setBodyHeight(y - top);
    // The instruments' input dropdowns list these inputs, so they follow.
    rebuildInputChoices();
    relayoutPanes();
}

void OutputsOverlay::rebuildInputChoices() {
    for (int i = 0; i < (int)instrRows_.size() && i < (int)instruments_.size(); i++) {
        auto* ch = instrRows_[i].inputChoice;
        if (!ch) continue;
        ch->clear();
        int sel = 0;
        for (int j = 0; j < (int)inputs_.size(); j++) {
            ch->add(displayInputName(j).c_str());
            if (inputs_[j].name == instruments_[i].inputName) sel = j;
        }
        if (!inputs_.empty()) {
            ch->value(sel);
            instruments_[i].inputName = inputs_[sel].name;
        }
    }
    for (int i = 0; i < (int)inRows_.size() && i < (int)inputs_.size(); i++) {
        if (!inRows_[i].deleteBtn) continue;
        if (inputReferenced(inputs_[i].name)) inRows_[i].deleteBtn->deactivate();
        else                                  inRows_[i].deleteBtn->activate();
    }
    redraw();
}

void OutputsOverlay::onScroll(int delta) {
    // Each pane carries its own widgets with it.
    for (auto* p : { inPane_, portsPane_, instrPane_ })
        p->position(p->x(), p->y() + delta);
}

void OutputsOverlay::rebuildPortChoices() {
    for (int i = 0; i < (int)instrRows_.size() && i < (int)instruments_.size(); i++) {
        auto* ch = instrRows_[i].portChoice;
        if (!ch) continue;
        ch->clear();
        int selIdx = 0;
        for (int j = 0; j < (int)outputs_.size(); j++) {
            // Shown the same way as the port row, so the two never disagree about
            // what a port is called. Selection is still by index and the value
            // stored below is the port's real name, so routing is unaffected.
            ch->add(displayPortName(j).c_str());
            if (outputs_[j].portName == instruments_[i].portName) selIdx = j;
        }
        if (!outputs_.empty()) {
            ch->value(selIdx);
            instruments_[i].portName = outputs_[selIdx].portName;
        }
    }
    // Update delete button state for each port.
    for (int i = 0; i < (int)rows_.size() && i < (int)outputs_.size(); i++) {
        if (!rows_[i].deleteBtn) continue;
        const std::string& pname = outputs_[i].portName;
        bool referenced = std::any_of(instruments_.begin(), instruments_.end(),
            [&](const Instrument& instr){ return instr.portName == pname; });
        if (referenced) rows_[i].deleteBtn->deactivate();
        else            rows_[i].deleteBtn->activate();
    }
    redraw();
}

// ── Static callbacks ──────────────────────────────────────────────────────────

void OutputsOverlay::inputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->rows_.size(); i++) {
        if (w != self->rows_[i].input) continue;
        std::string newName = static_cast<Fl_Input*>(w)->value();
        const std::string oldName = self->rows_[i].committedName;
        if (newName.empty() || newName == oldName) return;
        newName = self->uniquePortName(newName, i);
        static_cast<Fl_Input*>(w)->value(newName.c_str());
        self->rows_[i].committedName   = newName;
        self->outputs_[i].portName = newName;
        for (auto& instr : self->instruments_)
            if (instr.portName == oldName) instr.portName = newName;
        if (self->onPortRenamed) self->onPortRenamed(oldName, newName);
        self->rebuildPortChoices();
        return;
    }
}

void OutputsOverlay::backendChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->rows_.size(); i++) {
        if (w != self->rows_[i].backendChoice) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx < 0) return;
        self->outputs_[i].backend = static_cast<MidiBackend>(idx);
        // Switching to/from Plugin changes whether the name is the port's own or the
        // LV2 output's, so both the row and the instrument dropdowns are redrawn.
        self->rebuildRows();
        self->rebuildPortChoices();
        if (self->onPortBackendChanged) self->onPortBackendChanged();
        return;
    }
}

void OutputsOverlay::defaultTypeChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    int idx = static_cast<Fl_Choice*>(w)->value();
    if (idx < 0) return;
    self->defaultBackend_ = static_cast<MidiBackend>(idx);
}

// ── Input callbacks ──────────────────────────────────────────────────────────

void OutputsOverlay::inputNameCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->inRows_.size(); i++) {
        if (w != self->inRows_[i].input) continue;
        std::string newName = static_cast<Fl_Input*>(w)->value();
        const std::string oldName = self->inRows_[i].committedName;
        if (newName.empty()) {
            static_cast<Fl_Input*>(w)->value(oldName.c_str());
            return;
        }
        if (newName == oldName) return;
        newName = self->uniqueInputName(newName, i);
        static_cast<Fl_Input*>(w)->value(newName.c_str());
        self->inRows_[i].committedName = newName;
        self->inputs_[i].name = newName;
        for (auto& instr : self->instruments_)
            if (instr.inputName == oldName) instr.inputName = newName;
        if (self->onMidiInputRenamed) self->onMidiInputRenamed(oldName, newName);
        self->rebuildInputChoices();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::inputTypeCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->inRows_.size(); i++) {
        if (w != self->inRows_[i].typeChoice) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx < 0) return;
        self->syncFromInputs();
        // Item index is not the enum value here: Debug is missing from this list.
        self->inputs_[i].backend = inputBackendFromIndex(idx);
        // To or from Plugin changes what the input is shown as, here and in the
        // instruments' dropdowns, so both are rebuilt.
        self->rebuildInputRows();
        if (self->onMidiInputsChanged) self->onMidiInputsChanged();
        return;
    }
}

void OutputsOverlay::inputDeleteCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->inRows_.size(); i++) {
        if (w != self->inRows_[i].deleteBtn) continue;
        if (self->inputReferenced(self->inputs_[i].name)) return;
        self->syncFromInputs();
        self->inputs_.erase(self->inputs_.begin() + i);
        self->rebuildInputRows();
        if (self->onMidiInputsChanged) self->onMidiInputsChanged();
        return;
    }
}

void OutputsOverlay::instrInputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].inputChoice) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx >= 0 && idx < (int)self->inputs_.size())
            self->instruments_[i].inputName = self->inputs_[idx].name;
        self->rebuildInputChoices();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::instrInChanCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].inChanChoice) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx < 0) return;
        self->instruments_[i].inputChannel = idx;   // item 0 is "Any", items 1-16 the channel
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::deleteCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->rows_.size(); i++) {
        if (w != self->rows_[i].deleteBtn) continue;
        const std::string name = self->rows_[i].committedName;
        self->outputs_.erase(self->outputs_.begin() + i);
        self->rebuildRows();
        if (self->onPortRemoved) self->onPortRemoved(name);
        return;
    }
}

void OutputsOverlay::instrNameCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].nameInput) continue;
        std::string newName = static_cast<Fl_Input*>(w)->value();
        const std::string oldName = self->instrRows_[i].committedName;
        if (newName.empty()) {
            static_cast<Fl_Input*>(w)->value(oldName.c_str());
            return;
        }
        if (newName == oldName) return;
        newName = self->uniqueInstrName(newName, i);
        static_cast<Fl_Input*>(w)->value(newName.c_str());
        self->instrRows_[i].committedName = newName;
        self->instruments_[i].name = newName;
        if (self->instrObs_) self->instrObs_->rename(self->instruments_[i].id, newName);
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::refreshInstrumentButtons() {
    int drumCount = 0, stdCount = 0;
    for (const auto& instr : instruments_) instr.isDrum ? ++drumCount : ++stdCount;
    for (int i = 0; i < (int)instrRows_.size() && i < (int)instruments_.size(); i++) {
        if (!instrRows_[i].deleteBtn) continue;
        bool inUse = isInstrumentInUse && isInstrumentInUse(instruments_[i].id);
        int typeCount = instruments_[i].isDrum ? drumCount : stdCount;
        if (typeCount <= 1 || inUse) instrRows_[i].deleteBtn->deactivate();
        else                         instrRows_[i].deleteBtn->activate();
    }
    redraw();
}

void OutputsOverlay::instrDeleteCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].deleteBtn) continue;
        if (self->isInstrumentInUse && self->isInstrumentInUse(self->instruments_[i].id)) return;
        bool isDrum = self->instruments_[i].isDrum;
        int typeCount = 0;
        for (const auto& instr : self->instruments_) if (instr.isDrum == isDrum) ++typeCount;
        if (typeCount <= 1) return;
        int delId = self->instruments_[i].id;
        self->instruments_.erase(self->instruments_.begin() + i);
        if (self->instrObs_) self->instrObs_->remove(delId);
        self->rebuildInstrumentRows();
        self->rebuildPortChoices();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::portChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].portChoice) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx >= 0 && idx < (int)self->outputs_.size())
            self->instruments_[i].portName = self->outputs_[idx].portName;
        self->rebuildPortChoices();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::midiChanChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].midiChanChoice) continue;
        self->instruments_[i].midiChannel = static_cast<Fl_Choice*>(w)->value() + 1;
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::importDrumMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].importBtn) continue;
        Fl_Native_File_Chooser fc;
        fc.title("Import Drum Mappings");
        fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
        fc.filter("MIDNAM Files\t*.midnam\nAll Files\t*");
        if (fc.show() != 0) return;
        const char* path = fc.filename();
        if (!path || !path[0]) return;
        self->instruments_[i].drumMap = parseMidnam(path);
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::loadGmMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].gmBtn) continue;
        self->instruments_[i].drumMap = gmPercussionMap();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::loadGsMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].gsBtn) continue;
        self->instruments_[i].drumMap = gsPercussionMap();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::exportDrumMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].exportBtn) continue;
        Fl_Native_File_Chooser fc;
        fc.title("Export Drum Mappings");
        fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        fc.filter("MIDNAM Files\t*.midnam\nAll Files\t*");
        fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        if (fc.show() != 0) return;
        std::string path = fc.filename();
        if (path.empty()) return;
        if (path.size() < 7 || path.substr(path.size() - 7) != ".midnam")
            path += ".midnam";
        exportMidnam(path, self->instruments_[i].drumMap, self->instruments_[i].name);
        return;
    }
}

void OutputsOverlay::clearDrumMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].clearBtn) continue;
        self->instruments_[i].drumMap.clear();
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

void OutputsOverlay::fallbackChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].fallbackChoice) continue;
        // index 0 = "Notes" (show note names), index 1 = "Numbers" (show MIDI numbers)
        self->instruments_[i].fallbackNoteNames = (static_cast<Fl_Choice*>(w)->value() == 0);
        if (self->onInstrumentsChanged) self->onInstrumentsChanged();
        return;
    }
}

// ── Program / bank callbacks ──────────────────────────────────────────────────

void OutputsOverlay::programInputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].programInput) continue;
        int v = parseOptionalInt(static_cast<Fl_Input*>(w)->value(), 1, 128);
        self->instruments_[i].programNumber = (v >= 0) ? v - 1 : -1;
        self->instruments_[i].gm1Instrument = -1;
        if (self->instrRows_[i].programDropdown)
            self->instrRows_[i].programDropdown->value(0);
        if (self->onProgramChanged) self->onProgramChanged(self->instruments_[i].name);
        return;
    }
}

void OutputsOverlay::programDropdownCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].programDropdown) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx <= 0) {
            self->instruments_[i].programNumber = -1;
            self->instruments_[i].gm1Instrument = -1;
            if (self->instrRows_[i].programInput) self->instrRows_[i].programInput->value("");
        } else {
            self->instruments_[i].programNumber = idx - 1;
            self->instruments_[i].gm1Instrument = idx - 1;
            if (self->instrRows_[i].programInput)
                self->instrRows_[i].programInput->value(std::to_string(idx).c_str());
        }
        if (self->onProgramChanged) self->onProgramChanged(self->instruments_[i].name);
        return;
    }
}

void OutputsOverlay::bankMsbInputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].bankMsbInput) continue;
        self->instruments_[i].bankMsb = parseOptionalInt(static_cast<Fl_Input*>(w)->value(), 0, 127);
        if (self->onProgramChanged) self->onProgramChanged(self->instruments_[i].name);
        return;
    }
}

void OutputsOverlay::bankLsbInputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<OutputsOverlay*>(d);
    for (int i = 0; i < (int)self->instrRows_.size(); i++) {
        if (w != self->instrRows_[i].bankLsbInput) continue;
        self->instruments_[i].bankLsb = parseOptionalInt(static_cast<Fl_Input*>(w)->value(), 0, 127);
        if (self->onProgramChanged) self->onProgramChanged(self->instruments_[i].name);
        return;
    }
}

// ── Focus navigation ──────────────────────────────────────────────────────────

std::vector<Fl_Widget*> OutputsOverlay::getFocusOrder() const {
    std::vector<Fl_Widget*> order;
    auto add = [&](Fl_Widget* w) { if (w && w->visible() && w->active()) order.push_back(w); };

    // Each heading is a stop of its own (Space toggles it); its rows follow only
    // while it is open, since hidden widgets fail visible().
    add(inPane_);
    for (const auto& row : inRows_) { add(row.input); add(row.typeChoice); add(row.deleteBtn); }
    add(addInputBtn);

    add(portsPane_);
    add(defaultTypeChoice);
    for (const auto& row : rows_) { add(row.input); add(row.backendChoice); add(row.deleteBtn); }
    add(addBtn);

    add(instrPane_);
    for (const auto& row : instrRows_) {
        add(row.nameInput); add(row.deleteBtn);
        add(row.inputChoice); add(row.inChanChoice);
        add(row.portChoice);  add(row.midiChanChoice);
    }
    add(addDrumInstrBtn);
    add(addInstrBtn);

    add(closeBtn_);
    return order;
}

void OutputsOverlay::advanceFocusBy(int dir) {
    auto order = getFocusOrder();
    if (order.empty()) return;
    Fl_Widget* cur = Fl::focus();
    int idx = -1;
    for (int i = 0; i < (int)order.size(); i++)
        if (order[i] == cur) { idx = i; break; }
    int next = idx == -1 ? (dir > 0 ? 0 : (int)order.size() - 1)
                         : (idx + dir + (int)order.size()) % (int)order.size();
    Fl::focus(order[next]);
    order[next]->redraw();
}

int OutputsOverlay::handle(int event) {
    if (event == FL_KEYBOARD) {
        int k = Fl::event_key();
        if (k == FL_Tab) {
            advanceFocusBy(Fl::event_state() & FL_SHIFT ? -1 : 1);
            return 1;
        }
        if (k == FL_Enter || k == FL_KP_Enter) {
            advanceFocusBy(1);
            return 1;
        }
    }
    return OverlayWindow::handle(event);
}
