#pragma once
#include "basePopup.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

class ModernButton;
class Fl_Box;
class Fl_Input;
class Fl_Choice;

class ConnectionsOverlay : public BasePopup {
    ModernButton* closeBtn        = nullptr;
    ModernButton* addBtn          = nullptr;
    ModernButton* addInstrBtn     = nullptr;
    ModernButton* addDrumInstrBtn = nullptr;

    int nextPortId_ = 1;
    int nextInstrId_ = 1;

    // ── Port data ──────────────────────────────────────────────────────────────
    struct Connection {
        int id;
        std::string portName;
    };
    struct RowWidgets {
        Fl_Input*     input        = nullptr;
        ModernButton* deleteBtn    = nullptr;
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
        std::string   committedName;
    };

    std::vector<Connection>  connections_;
    std::vector<RowWidgets>  rows_;
    std::vector<Instrument>    instruments_;
    std::vector<InstrumentRow> instrRows_;

    // Layout state (recomputed by rebuildRows / rebuildInstrumentRows)
    int instrSectionTopY_ = 0;
    int instrRowsTopY_    = 0;
    int instrNameW_       = 0;
    int instrPortW_       = 0;

    void rebuildRows();
    void rebuildInstrumentRows();
    void rebuildPortChoices();
    void syncFromInputs();

    std::vector<Fl_Widget*> getFocusOrder() const;
    void advanceFocusBy(int dir);

    std::string uniquePortName(const std::string& base, int excludeIdx = -1) const;
    std::string uniqueInstrName(const std::string& base, int excludeIdx = -1) const;
    std::string nextNumberedInstrName(bool isDrum) const;

    static void inputCb         (Fl_Widget*, void*);
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

    void draw() override;
    int  handle(int event) override;

public:
    ConnectionsOverlay(int x, int y, int w, int h);
    void hide() override;

    // Port API (unchanged)
    void setConnections(const std::vector<std::string>& portNames);
    std::vector<std::string> getConnections() const;

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
    void updateInstrumentDrumMap(const std::string& instrName, int midiNote, const std::string& label);

    std::function<void(const std::string& name)>                                onPortAdded;
    std::function<void(const std::string& name)>                                onPortRemoved;
    std::function<void(const std::string& oldName, const std::string& newName)> onPortRenamed;

    // Fired whenever the instruments list or any instrument's fields change.
    std::function<void()> onInstrumentsChanged;
    std::function<void(const std::string& oldName, const std::string& newName)> onInstrumentRenamed;
    // Fired when program number or bank fields change for an instrument.
    std::function<void(const std::string& instrName)> onProgramChanged;

    // Optional: return true if the named instrument is currently used by a pattern.
    std::function<bool(const std::string& name)> isInstrumentInUse;
    void refreshInstrumentButtons();
    std::function<void()> onClose;
};
