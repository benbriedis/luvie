// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PARAMETER_SUBMENU_HPP
#define PARAMETER_SUBMENU_HPP

#include "modern/contextMenuPopup.hpp"
#include "observableSong.hpp"
#include "paramDefs.hpp"
#include "midiLearn.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// What the param-lane menus ask the app to do with an instrument's parameters.
// LuvieApp fills these in; the menus leave the items out while they are unset.
struct ParamMenuActions {
    // Open the editor for the instrument's parameter `name` at (wx, wy).
    std::function<void(int instrumentId, const std::string& name, int wx, int wy)> edit;
    // MIDI learn a new parameter on the instrument; addLane then gets its name and
    // adds the lane where the menu was opened.
    std::function<void(int instrumentId, std::function<void(const std::string&)> addLane)> learnNew;
};

// The "Add automation" submenu: the core standard parameters, then the
// instrument's own, then "Other standard" (a submenu of the rest of the standard
// set) and "New (MIDI learn)". Parameters the target already has a lane
// for are ticked and greyed. The rows are rebuilt each time it opens, since the
// instrument's parameters change.
class ParameterSubmenu : public ContextMenuPopup {
    struct ItemData { ParameterSubmenu* self; int idx; };

    std::vector<std::string>               names_;   // one per parameter row
    std::vector<std::unique_ptr<ItemData>> itemData_;
    std::function<bool(const char*)>       hasFn_;
    ModernButton*                          moreBtn_ = nullptr;
    bool                                   nested_  = false;

    explicit ParameterSubmenu(bool nested) : ContextMenuPopup(popW, 2), nested_(nested) {
        end();
        hide();
        if (!nested_) {
            more = new ParameterSubmenu(true);
            more->onSelect = [this](const char* type) {
                hide();
                if (onSelect) onSelect(type);
            };
        }
    }

    void select(int idx) {
        // A copy: onSelect may open this menu again, which rebuilds names_.
        const std::string type = names_[idx];
        hide();
        if (onSelect) onSelect(type.c_str());
    }

    ModernButton* addRow(int row, const char* label) {
        begin();
        auto* b = addItem(row, label);
        end();
        b->reserveTick(true);
        if (!nested_) b->onEnter = [this]() { if (more) more->hideSoon(); };
        return b;
    }

    void addParamRow(int row, const std::string& name) {
        auto* b = addRow(row, name.c_str());
        b->copy_label(name.c_str());
        const int idx = (int)names_.size();
        names_.push_back(name);
        itemData_.push_back(std::make_unique<ItemData>(ItemData{this, idx}));
        b->callback([](Fl_Widget*, void* d) {
            auto* data = static_cast<ItemData*>(d);
            data->self->select(data->idx);
        }, itemData_.back().get());
        const bool has = hasFn_ && hasFn_(name.c_str());
        b->setTicked(has);
        has ? b->deactivate() : b->activate();
    }

    void rebuild(const std::vector<std::string>& ownNames) {
        clear();   // deletes the old rows
        // The window is then resized to fit the new rows. As its own resizable()
        // — which clear() has just made it again — it would stretch them in
        // proportion to the old size (2px tall, the first time), scattering them.
        resizable(nullptr);
        names_.clear();
        itemData_.clear();
        moreBtn_ = nullptr;
        int row = 0;

        if (nested_) {
            for (const auto& s : kStandardParams)
                if (!s.core && std::find(ownNames.begin(), ownNames.end(), s.name) == ownNames.end())
                    addParamRow(row++, s.name);
        } else {
            for (const auto& s : kStandardParams)
                if (s.core) addParamRow(row++, s.name);
            for (const auto& n : ownNames) {
                const StandardParam* s = standardParam(n);
                if (!s || !s->core) addParamRow(row++, n);
            }
            moreBtn_ = addRow(row++, "Other standard");
            moreBtn_->setSubmenuArrow(true);
            moreBtn_->onEnter = [this]() { if (more) more->cancelHide(); };
            moreBtn_->callback([](Fl_Widget*, void* d) {
                static_cast<ParameterSubmenu*>(d)->showMore();
            }, this);
            if (onLearnNew) {
                auto* learn = addRow(row++, "New (MIDI learn)");
                learn->callback([](Fl_Widget* w, void* d) {
                    static_cast<ParameterSubmenu*>(d)->startLearnNew(static_cast<ModernButton*>(w));
                }, this);
            }
        }
        popH = row * itemH + 2;
        size(popW, popH);
        redraw();
    }

    // The menu stays open, saying what it is waiting for, until the control
    // arrives (learnStateChanged) or it is closed, which cancels the learn.
    void startLearnNew(ModernButton* row) {
        if (!onLearnNew) return;
        if (more) more->hide();
        onLearnNew();
        learningNew_ = true;
        row->copy_label("Move a control…");
        row->labelcolor(0xF59E0B00);   // amber, as the "Learning…" badge
        row->deactivate();
        redraw();
    }

    void showMore() {
        if (!more || !moreBtn_) return;
        more->hasFn_ = hasFn_;
        more->rebuild(ownNames_);
        more->place(this, y() + moreBtn_->y());
        more->show();
    }

    void place(BasePopup* parent, int btnY) {
        // Popups are subwindows of the app window, so all of this is in its
        // coordinates (0,0 at its top left), not the screen's. A submenu that would
        // run off the right opens to the left instead; one that would run off the
        // bottom moves up, though never above the top.
        constexpr int margin = 4;
        int subX = parent->x() + parent->w();
        int subY = btnY;
        if (auto* win = parent->window()) {
            if (subX > win->w() - popW) subX = parent->x() - popW;
            subY = std::min(subY, win->h() - h() - margin);
        }
        position(std::max(0, subX), std::max(margin, subY));
    }

    std::vector<std::string> ownNames_;
    static constexpr double  kHideDelay = 0.4;   // seconds
    static void hideTimeout(void* d) { static_cast<ParameterSubmenu*>(d)->hide(); }
    BasePopup*               parent_      = nullptr;
    bool                     learningNew_ = false;

public:
    static constexpr int itemH = btnH;
    static constexpr int popW  = 170;

    std::function<void(const char*)> onSelect;
    // "New (MIDI learn)" chosen: start the learn. Unset, the row is not shown.
    std::function<void()>            onLearnNew;
    // Cancelled if the menu closes before the control arrives.
    MidiLearnMap*                    midiLearn = nullptr;
    // The "Other standard" submenu. The owner adds it to the window beside this.
    ParameterSubmenu*                more = nullptr;

    ParameterSubmenu() : ParameterSubmenu(false) {}
    ~ParameterSubmenu() override { Fl::remove_timeout(hideTimeout, this); }

    // For the opening menu's other rows to call on hover. The submenu closes after
    // a moment rather than at once, so a pointer heading for it on a slant, across
    // a neighbouring row, gets there first: reaching it, or going back to the row
    // that opened it (cancelHide), keeps it open. While it waits for a MIDI-learn
    // control it stays open regardless, until clicked away or Escape.
    void hideSoon() {
        if (!visible() || learningNew_) return;
        Fl::remove_timeout(hideTimeout, this);
        Fl::add_timeout(kHideDelay, hideTimeout, this);
    }
    void cancelHide() { Fl::remove_timeout(hideTimeout, this); }

    int handle(int event) override {
        if (event == FL_ENTER || event == FL_MOVE) cancelHide();
        return ContextMenuPopup::handle(event);
    }

    void hide() override {
        cancelHide();
        if (more) more->hide();
        if (learningNew_) {
            learningNew_ = false;
            if (midiLearn) midiLearn->cancelLearn();
        }
        ContextMenuPopup::hide();
    }

    // The MIDI-learn state changed. Once "New (MIDI learn)" has its control, the
    // menu it was chosen from closes.
    void learnStateChanged() {
        if (!learningNew_ || !midiLearn || midiLearn->isLearningNew()) return;
        learningNew_ = false;
        hide();
        if (parent_) parent_->hide();
    }

    // ownNames: the instrument's own parameters (Instrument::paramDefs), listed
    // after the core standard ones. hasFn: whether the target already has a lane
    // of that name.
    void showFor(BasePopup* parent, int btnY, const std::vector<std::string>& ownNames,
                 const std::function<bool(const char*)>& hasFn) {
        hasFn_    = hasFn;
        ownNames_ = ownNames;
        parent_   = parent;
        rebuild(ownNames_);
        place(parent, btnY);
        show();
    }

    void showFor(BasePopup* parent, int btnY, ObservableSong* tl, int instrumentId) {
        showFor(parent, btnY, ownParamNames(tl ? &tl->get() : nullptr, instrumentId),
                [tl, instrumentId](const char* type) {
                    return tl && tl->hasParamLane(type, instrumentId);
                });
    }

    static std::vector<std::string> ownParamNames(const Timeline* tl, int instrumentId) {
        std::vector<std::string> names;
        if (const Instrument* in = tl ? tl->instrument(instrumentId) : nullptr)
            for (const auto& d : in->paramDefs) names.push_back(d.name);
        return names;
    }
};

#endif
