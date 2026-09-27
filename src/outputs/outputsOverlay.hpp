// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "overlayWindow.hpp"
#include "midiBackend.hpp"
#include "timelineIO.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

class ObservableInstrument;

class ModernButton;
class CollapsiblePane;
class Fl_Box;
class Fl_Input;
class Fl_Choice;
class Fl_Check_Button;

class OutputsOverlay : public OverlayWindow {
    ModernButton* addBtn          = nullptr;
    ModernButton* addInstrBtn     = nullptr;
    ModernButton* addDrumInstrBtn = nullptr;
    Fl_Choice*    defaultTypeChoice = nullptr;  // "Default port type" for new ports
    ModernButton* addInputBtn     = nullptr;

    // The three sections, top to bottom in this order. Each owns its section's
    // widgets; all start collapsed.
    CollapsiblePane* inPane_    = nullptr;   // MIDI Input Ports
    CollapsiblePane* portsPane_ = nullptr;   // MIDI Output Ports
    CollapsiblePane* instrPane_ = nullptr;   // Instruments

    int nextPortId_ = 1;
    // True when hosted as an LV2 plugin. Decides which backends the port dropdowns
    // offer: the ones this mode cannot drive stay listed but greyed, so a project
    // moved between standalone and plugin still shows what it was set to.
    bool pluginMode_ = false;
    MidiBackend defaultBackend_ = MidiBackend::Jack;  // type assigned to newly added ports
    ObservableInstrument* instrObs_ = nullptr;

    // ── Port data ──────────────────────────────────────────────────────────────
    struct Output {
        int id;
        std::string portName;
        MidiBackend backend = MidiBackend::Jack;
    };
    struct RowWidgets {
        Fl_Input*     input         = nullptr;
        Fl_Choice*    backendChoice = nullptr;
        ModernButton* deleteBtn     = nullptr;
        std::string   committedName;
    };

    // ── Instrument data ────────────────────────────────────────────────────────
    struct Instrument {
        int id;
        std::string name;
        std::string portName;
        int midiChannel = 1;
        std::map<int, std::string> drumMap;
        bool isDrum            = false;
        bool fallbackNoteNames = false;
        int  programNumber     = -1;
        int  bankMsb           = -1;
        int  bankLsb           = -1;
        int  gm1Instrument     = -1;
        std::string inputName;               // the MIDI input it is played from
        int  inputChannel      = 0;          // 0 = Any; 1-16
        KeySplit split         = KeySplit::None;
        int  splitNote         = 60;
        bool passThrough       = true;
    };
    struct InstrumentRow {
        Fl_Box*       typeLabel      = nullptr;
        Fl_Input*     nameInput      = nullptr;
        Fl_Choice*    portChoice     = nullptr;
        Fl_Choice*    midiChanChoice = nullptr;
        ModernButton* deleteBtn      = nullptr;
        ModernButton* importBtn       = nullptr;
        ModernButton* gmBtn           = nullptr;
        ModernButton* gsBtn           = nullptr;
        ModernButton* exportBtn       = nullptr;
        ModernButton* clearBtn        = nullptr;
        Fl_Box*       fallbackLabel   = nullptr;
        Fl_Choice*    fallbackChoice  = nullptr;
        Fl_Input*     programInput    = nullptr;
        Fl_Choice*    programDropdown = nullptr;
        Fl_Input*     bankMsbInput    = nullptr;
        Fl_Input*     bankLsbInput    = nullptr;
        Fl_Box*       bankLabel       = nullptr;
        Fl_Box*       msbLabel        = nullptr;
        Fl_Box*       lsbLabel        = nullptr;
        Fl_Box*       progLabel       = nullptr;
        Fl_Box*       gm1Label        = nullptr;
        Fl_Box*       inputLabel      = nullptr;
        Fl_Choice*    inputChoice     = nullptr;
        Fl_Box*       inChanLabel     = nullptr;
        Fl_Choice*    inChanChoice    = nullptr;
        Fl_Box*       splitLabel      = nullptr;
        Fl_Choice*    splitChoice     = nullptr;
        Fl_Box*       splitNoteLabel  = nullptr;
        ModernButton* splitLearnBtn   = nullptr;
        Fl_Check_Button* passThroughCheck = nullptr;
        Fl_Box*       outputLabel     = nullptr;
        Fl_Box*       outChanLabel    = nullptr;
        std::string   committedName;
    };

    // ── MIDI input data ────────────────────────────────────────────────────────
    // Held directly as the AppState structs: an input has nothing the UI adds.
    struct InputRow {
        Fl_Input*     input      = nullptr;
        Fl_Choice*    typeChoice = nullptr;
        ModernButton* deleteBtn  = nullptr;
        std::string   committedName;
    };

    std::vector<Output>  outputs_;
    std::vector<RowWidgets>  rows_;
    std::vector<Instrument>    instruments_;
    std::vector<InstrumentRow> instrRows_;
    std::vector<MidiInputPort>  inputs_;
    std::vector<InputRow>       inRows_;

    // The instrument whose split point is waiting for a key, or -1. By id, so it
    // survives the rows being rebuilt.
    int splitLearnId_ = -1;
    // The key that completed a split learn, on its input: its release is swallowed
    // too, so the key sounds nowhere. -1 when there is none.
    int         splitSwallowNote_ = -1;
    std::string splitSwallowInput_;

    bool jackWarning_ = false;  // show "JACK server not running" on the ports heading

    // Column widths (recomputed by the rebuilds, read by the column headings)
    int instrNameW_       = 0;

    // Each rebuild recreates one section's rows inside its pane, then restacks
    // the panes; none of them touches another section's widgets.
    void rebuildAll();
    void rebuildRows();              // MIDI Output Ports
    void rebuildInstrumentRows();
    void rebuildPortChoices();
    void rebuildInputRows();
    // Stacks the panes under the title, sizes the scroll extent to fit them, and
    // refreshes the counts on their headings.
    void relayoutPanes();
    void updateSummaries();
    void buildPaneBodies();          // column headings for each pane
    // Refills each instrument's input dropdown and the inputs' delete buttons.
    void rebuildInputChoices();
    void syncFromInputs();
    // Shows instrument i's split — the note, and whether it is learning — on its row.
    void updateSplitRow(int i);

    // What a port is shown as. Hosted, a Plugin-backed port is displayed as the LV2
    // output it actually drives ("MIDI Out 2") rather than its own name, and its
    // name field is disabled — the mapping is positional, so the name is not the
    // user's to choose there. Display only: outputs_[i].portName stays the routing
    // key and keeps its meaning when the project goes back to the standalone app.
    std::string displayPortName(int i) const;
    // The same for input i: hosted, a Plugin input shows the LV2 input it listens on.
    std::string displayInputName(int i) const;


    std::vector<Fl_Widget*> getFocusOrder() const;
    void advanceFocusBy(int dir);

    // Seeds outputs_ with the default port set for a fresh project.
    void addDefaultOutputs();

    std::string nextDefaultPortName() const;
    std::string uniquePortName(const std::string& base, int excludeIdx = -1) const;
    std::string uniqueInstrName(const std::string& base, int excludeIdx = -1) const;
    std::string nextDefaultInstrName(bool isDrum) const;
    // Input and output ports share one JACK client, so a name must be unique
    // across both lists, not just its own.
    bool        portOrInputNameTaken(const std::string& name, int excludeOut, int excludeIn) const;
    std::string uniqueInputName(const std::string& base, int excludeIdx = -1) const;
    bool        inputReferenced(const std::string& name) const;

    static void inputCb            (Fl_Widget*, void*);
    static void backendChoiceCb    (Fl_Widget*, void*);
    static void defaultTypeChoiceCb(Fl_Widget*, void*);
    static void inputNameCb     (Fl_Widget*, void*);
    static void inputTypeCb     (Fl_Widget*, void*);
    static void inputDeleteCb   (Fl_Widget*, void*);
    static void instrInputCb    (Fl_Widget*, void*);
    static void instrInChanCb   (Fl_Widget*, void*);
    static void splitChoiceCb   (Fl_Widget*, void*);
    static void splitLearnCb    (Fl_Widget*, void*);
    static void passThroughCb   (Fl_Widget*, void*);
    static void deleteCb        (Fl_Widget*, void*);
    static void instrNameCb     (Fl_Widget*, void*);
    static void instrDeleteCb   (Fl_Widget*, void*);
    static void portChoiceCb    (Fl_Widget*, void*);
    static void midiChanChoiceCb(Fl_Widget*, void*);
    static void importDrumMapCb   (Fl_Widget*, void*);
    static void loadGmMapCb       (Fl_Widget*, void*);
    static void loadGsMapCb       (Fl_Widget*, void*);
    static void exportDrumMapCb   (Fl_Widget*, void*);
    static void clearDrumMapCb    (Fl_Widget*, void*);
    static void fallbackChoiceCb  (Fl_Widget*, void*);
    static void programInputCb    (Fl_Widget*, void*);
    static void programDropdownCb (Fl_Widget*, void*);
    static void bankMsbInputCb    (Fl_Widget*, void*);
    static void bankLsbInputCb    (Fl_Widget*, void*);

    void onResized() override;
    void onScroll(int delta) override;
    int  handle(int event) override;

public:
    OutputsOverlay(int x, int y, int w, int h, bool pluginMode);
    void show() override;
    void hide() override;

    // Port API
    void setOutputs(const std::vector<std::string>& portNames);
    void setOutputs(const std::vector<JackOutput>& ports);
    std::vector<std::string> getOutputs() const;
    std::vector<JackOutput>  getOutputsFull() const;  // names + backends

    // Default port type applied to newly added ports.
    void        setDefaultBackend(MidiBackend backend);
    MidiBackend getDefaultBackend() const { return defaultBackend_; }

    // Show/hide the red "JACK server not running" warning under the title.
    void setJackWarning(bool show);

    // MIDI input API. Loading carries an input on a backend this mode cannot
    // drive over to one it can, as the single input always did, so a project moved
    // between standalone and plugin never arrives with its inputs dead.
    void setMidiInputs(const std::vector<MidiInputPort>& ins);
    const std::vector<MidiInputPort>& getMidiInputs() const { return inputs_; }
    // Retypes every input — the startup dialog's MIDI input choice.
    void setAllInputBackends(MidiBackend b);

    // Instrument API
    struct InstrumentInfo {
        int         id;
        std::string name;
        std::string portName;
        int         midiChannel;
        std::map<int, std::string> drumMap;
        bool        isDrum            = false;
        bool        fallbackNoteNames = false;
        int         programNumber     = -1;
        int         bankMsb           = -1;
        int         bankLsb           = -1;
        int         gm1Instrument     = -1;
        std::string inputName;           // empty = the first input
        int         inputChannel      = 0;
        KeySplit    split             = KeySplit::None;
        int         splitNote         = 60;
        bool        passThrough       = true;
    };
    void setInstruments(const std::vector<InstrumentInfo>& instrs);
    std::vector<InstrumentInfo> getInstruments() const;
    // The input, channel and split instrument `instrId` is played from. False if
    // there is no such instrument. Cheap: for the MIDI input path, which runs per event.
    bool instrumentInput(int instrId, std::string& inputName, int& channel,
                         KeySplit& split, int& splitNote) const;
    // Whether instrument `instrId` sends what it is played straight to its output.
    // False if there is no such instrument.
    bool instrumentPassesThrough(int instrId) const;
    // Every instrument's id, in the list's order.
    std::vector<int> instrumentIds() const;

    // Split learn: an instrument's MIDI Learn button waits for a key on that
    // instrument's input (and channel), which becomes its split point. Feed every
    // incoming message from input `inputName` here before anything else sees it.
    // True when the message was the learned key — its press or its release — and
    // so should go nowhere else.
    bool handleSplitLearn(const std::string& inputName, const uint8_t* data, int len);
    void cancelSplitLearn();
    // A split learn began. The app cancels any other MIDI learn, so one key never
    // completes two.
    std::function<void()> onSplitLearnStarted;
    void updateInstrumentDrumMap(int instrId, int midiNote, const std::string& label);
    void setObservableInstrument(ObservableInstrument* instr);

    std::function<void(const std::string& name)>                                onPortAdded;
    std::function<void(const std::string& name)>                                onPortRemoved;
    std::function<void(const std::string& oldName, const std::string& newName)> onPortRenamed;
    // Fired when any port's backend (Jack/Native/Debug) changes; main re-syncs the port set.
    std::function<void()>                                                       onPortBackendChanged;
    // Fired when an input is added or removed or its type changes; the owner
    // reopens the underlying ports to match.
    std::function<void()>                                                       onMidiInputsChanged;
    // Renamed in place, so the owner can keep the port (and its connections).
    std::function<void(const std::string& oldName, const std::string& newName)> onMidiInputRenamed;

    // Fired whenever the instruments list or any instrument's fields change.
    std::function<void()> onInstrumentsChanged;
    // Fired when program number or bank fields change for an instrument.
    std::function<void(const std::string& instrName)> onProgramChanged;

    // Optional: return true if the instrument ID is currently used by a pattern.
    std::function<bool(int instrId)> isInstrumentInUse;
    void refreshInstrumentButtons();

};
