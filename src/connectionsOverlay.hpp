#pragma once
#include "basePopup.hpp"
#include <functional>
#include <string>
#include <vector>

class ModernButton;
class Fl_Input;

class ConnectionsOverlay : public BasePopup {
    ModernButton* closeBtn = nullptr;
    ModernButton* addBtn   = nullptr;

    struct Connection {
        std::string portName;  // current name (what JACK has registered)
    };
    struct RowWidgets {
        Fl_Input*     input       = nullptr;
        ModernButton* deleteBtn   = nullptr;
        std::string   committedName; // last name confirmed to JACK
    };

    std::vector<Connection>  connections_;
    std::vector<RowWidgets>  rows_;

    void rebuildRows();
    void syncFromInputs();

    static void inputCb (Fl_Widget*, void*);
    static void deleteCb(Fl_Widget*, void*);

    void draw() override;

public:
    ConnectionsOverlay(int x, int y, int w, int h);
    void hide() override;

    // Populate without firing callbacks (e.g. on session load).
    void setConnections(const std::vector<std::string>& portNames);
    std::vector<std::string> getConnections() const;

    // Fired when JACK API should be called.
    std::function<void(const std::string& name)>                       onPortAdded;
    std::function<void(const std::string& name)>                       onPortRemoved;
    std::function<void(const std::string& oldName, const std::string& newName)> onPortRenamed;

    std::function<void()> onClose;
};
