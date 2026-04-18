#pragma once
#include "basePopup.hpp"
#include <functional>
#include <string>
#include <vector>

class ModernButton;
class Fl_Input;
class Fl_Choice;

class ConnectionsOverlay : public BasePopup {
    ModernButton* closeBtn   = nullptr;
    ModernButton* addBtn     = nullptr;
    ModernButton* addChanBtn = nullptr;

    int nextPortId_ = 1;
    int nextChanId_ = 1;

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

    // ── Channel data ───────────────────────────────────────────────────────────
    struct Channel {
        int id;
        std::string name;
        std::string portName;
        int midiChannel = 1;
    };
    struct ChannelRow {
        Fl_Input*     nameInput     = nullptr;
        Fl_Choice*    portChoice    = nullptr;
        Fl_Choice*    midiChanChoice = nullptr;
        ModernButton* deleteBtn     = nullptr;
        std::string   committedName;
    };

    std::vector<Connection>  connections_;
    std::vector<RowWidgets>  rows_;
    std::vector<Channel>     channels_;
    std::vector<ChannelRow>  chanRows_;

    // Layout state (recomputed by rebuildRows / rebuildChannelRows)
    int chanSectionTopY_ = 0;
    int chanRowsTopY_    = 0;
    int chanNameW_       = 0;
    int chanPortW_       = 0;

    void rebuildRows();
    void rebuildChannelRows();
    void rebuildPortChoices();
    void syncFromInputs();

    std::string uniquePortName(const std::string& base, int excludeIdx = -1) const;
    std::string uniqueChanName(const std::string& base, int excludeIdx = -1) const;

    static void inputCb         (Fl_Widget*, void*);
    static void deleteCb        (Fl_Widget*, void*);
    static void chanNameCb      (Fl_Widget*, void*);
    static void chanDeleteCb    (Fl_Widget*, void*);
    static void portChoiceCb    (Fl_Widget*, void*);
    static void midiChanChoiceCb(Fl_Widget*, void*);

    void draw() override;

public:
    ConnectionsOverlay(int x, int y, int w, int h);
    void hide() override;

    // Port API (unchanged)
    void setConnections(const std::vector<std::string>& portNames);
    std::vector<std::string> getConnections() const;

    // Channel API
    struct ChannelInfo {
        int         id;
        std::string name;
        std::string portName;
        int         midiChannel;
    };
    void setChannels(const std::vector<ChannelInfo>& chans);
    std::vector<ChannelInfo> getChannels() const;

    std::function<void(const std::string& name)>                                onPortAdded;
    std::function<void(const std::string& name)>                                onPortRemoved;
    std::function<void(const std::string& oldName, const std::string& newName)> onPortRenamed;

    // Fired whenever the channels list or any channel's fields change.
    std::function<void()> onChannelsChanged;
    std::function<void(const std::string& oldName, const std::string& newName)> onChannelRenamed;

    // Optional: return true if the named channel is currently used by a pattern.
    std::function<bool(const std::string& name)> isChannelInUse;
    void refreshChannelButtons();
    std::function<void()> onClose;
};
