#ifndef SETTINGS_MENU_POPUP_HPP
#define SETTINGS_MENU_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include <FL/Fl_Box.H>
#include <functional>

// Settings gear drop-down, styled to match the app's other context menus
// (modern/contextMenuPopup.hpp) rather than FLTK's default menu.
class SettingsMenuPopup : public ContextMenuPopup {
    ModernButton* saveAsBtn = nullptr;

    // Hide the menu, then fire the slot the button was wired to.
    void pick(std::function<void()>& slot) {
        hide();
        if (slot) slot();
    }

public:
    static constexpr int popW = 190;

    std::function<void()> onSaveAs;
    std::function<void()> onImport;
    std::function<void()> onExport;
    std::function<void()> onTransport;
    std::function<void()> onOutputs;

    static constexpr int dividerH = 9;

    SettingsMenuPopup() : ContextMenuPopup(popW, 5 * btnH + dividerH + 2) {
        saveAsBtn         = addItem(0, "Save As");
        auto* importBtn   = addItem(1, "Import");
        auto* exportBtn   = addItem(2, "Export");
        auto* transportBtn= addItem(3, "Transport");
        auto* outputsBtn  = addItem(4, "Instruments & Outputs");

        // Separate the file items (Save As/Import/Export) from the view items
        // (Transport/Outputs), as the old menu's divider did: nudge the view
        // items down by the gap and draw a thin rule through the middle of it.
        transportBtn->position(transportBtn->x(), transportBtn->y() + dividerH);
        outputsBtn  ->position(outputsBtn->x(),   outputsBtn->y()   + dividerH);
        auto* divider = new Fl_Box(1, 1 + 3 * btnH + dividerH / 2, popW - 2, 1);
        divider->box(FL_FLAT_BOX);
        divider->color(0xCBD5E100);

        saveAsBtn->callback([](Fl_Widget*, void* d) {
            auto* s = static_cast<SettingsMenuPopup*>(d); s->pick(s->onSaveAs);
        }, this);
        importBtn->callback([](Fl_Widget*, void* d) {
            auto* s = static_cast<SettingsMenuPopup*>(d); s->pick(s->onImport);
        }, this);
        exportBtn->callback([](Fl_Widget*, void* d) {
            auto* s = static_cast<SettingsMenuPopup*>(d); s->pick(s->onExport);
        }, this);
        transportBtn->callback([](Fl_Widget*, void* d) {
            auto* s = static_cast<SettingsMenuPopup*>(d); s->pick(s->onTransport);
        }, this);
        outputsBtn->callback([](Fl_Widget*, void* d) {
            auto* s = static_cast<SettingsMenuPopup*>(d); s->pick(s->onOutputs);
        }, this);

        end();
        hide();
    }

    void open(int wx, int wy) { openAt(wx, wy); }

    void setSaveAsEnabled(bool en) {
        if (!saveAsBtn) return;
        en ? saveAsBtn->activate() : saveAsBtn->deactivate();
        saveAsBtn->redraw();
    }
};

#endif
