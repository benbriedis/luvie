#pragma once
#include "overlayWindow.hpp"
#include "midiBackend.hpp"
#include "timelineIO.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

class ObservableInstrument;

class ModernButton;
class Fl_Box;
class Fl_Input;
class Fl_Choice;

class OutputsOverlay : public OverlayWindow {
    ModernButton* addBtn          = nullptr;
    ModernButton* addInstrBtn     = nullptr;
    ModernButton* addDrumInstrBtn = nullptr;
    Fl_Choice*    defaultTypeChoice = nullptr;  // "Default port type" for new ports

    int nextPortId_ = 1;
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
        std::string   committedName;
    };

    std::vector<Output>  outputs_;
    std::vector<RowWidgets>  rows_;
    std::vector<Instrument>    instruments_;
    std::vector<InstrumentRow> instrRows_;

    bool jackWarning_ = false;  // draw the red "JACK server not running" line

    // Layout state (recomputed by rebuildRows / rebuildInstrumentRows)
    int instrSectionTopY_ = 0;
    int instrRowsTopY_    = 0;
    int instrNameW_       = 0;
    int instrPortW_       = 0;

    void rebuildRows();
    void rebuildInstrumentRows();
    void rebuildPortChoices();
    void syncFromInputs();

    // Vertical layout of the ports section (depends on whether the JACK warning shows).
    int defaultTypeRowY() const;  // top Y of the "Default port type" row
    int portsColY()       const;  // top Y of the "PORT NAME" column header
    int portsRowsTopY()   const;  // top Y of the first port row / divider

    std::vector<Fl_Widget*> getFocusOrder() const;
    void advanceFocusBy(int dir);

    std::string uniquePortName(const std::string& base, int excludeIdx = -1) const;
    std::string uniqueInstrName(const std::string& base, int excludeIdx = -1) const;
    std::string nextNumberedInstrName(bool isDrum) const;

    static void inputCb            (Fl_Widget*, void*);
    static void backendChoiceCb    (Fl_Widget*, void*);
    static void defaultTypeChoiceCb(Fl_Widget*, void*);
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
    void drawStaticContent(int scrollY, int sbW) override;
    int  handle(int event) override;

public:
    OutputsOverlay(int x, int y, int w, int h);
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
    };
    void setInstruments(const std::vector<InstrumentInfo>& instrs);
    std::vector<InstrumentInfo> getInstruments() const;
    void updateInstrumentDrumMap(int instrId, int midiNote, const std::string& label);
    void setObservableInstrument(ObservableInstrument* instr);

    std::function<void(const std::string& name)>                                onPortAdded;
    std::function<void(const std::string& name)>                                onPortRemoved;
    std::function<void(const std::string& oldName, const std::string& newName)> onPortRenamed;
    // Fired when any port's backend (Jack/Native/Debug) changes; main re-syncs the port set.
    std::function<void()>                                                       onPortBackendChanged;

    // Fired whenever the instruments list or any instrument's fields change.
    std::function<void()> onInstrumentsChanged;
    // Fired when program number or bank fields change for an instrument.
    std::function<void(const std::string& instrName)> onProgramChanged;

    // Optional: return true if the instrument ID is currently used by a pattern.
    std::function<bool(int instrId)> isInstrumentInUse;
    void refreshInstrumentButtons();
};
