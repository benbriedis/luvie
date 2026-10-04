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
    // Define a new parameter on the instrument by hand, opening the editor at
    // (wx, wy); addLane then gets its name, as for learnNew.
    std::function<void(int instrumentId, int wx, int wy,
                       std::function<void(const std::string&)> addLane)> create;
};

// The "Add automation" submenu: "Standard" (a submenu of the core standard
// parameters), "Other standard" (the rest of the standard set), "Custom" (the
// instrument's own parameters; greyed when it has none), then "New parameter…" and
// "New (MIDI learn)". A standard parameter the instrument has redefined stays under
// its standard heading. Parameters the target already has a lane
// for are ticked and greyed. The rows are rebuilt each time it opens, since the
// instrument's parameters change.
class ParameterSubmenu : public ContextMenuPopup {
    struct ItemData { ParameterSubmenu* self; int idx; };

    std::vector<std::string>               names_;   // one per parameter row
    std::vector<std::unique_ptr<ItemData>> itemData_;
    std::function<bool(const char*)>       hasFn_;
    // The top menu, or one of its submenus.
    enum class Kind { Top, Core, Other, Custom };

    ModernButton*                          standardBtn_ = nullptr;
    ModernButton*                          moreBtn_     = nullptr;
    ModernButton*                          customBtn_   = nullptr;
    Kind                                   kind_        = Kind::Top;

    explicit ParameterSubmenu(Kind kind) : ContextMenuPopup(popW, 2), kind_(kind) {
        end();
        hide();
        if (kind_ == Kind::Top) {
            standard = new ParameterSubmenu(Kind::Core);
            more     = new ParameterSubmenu(Kind::Other);
            custom   = new ParameterSubmenu(Kind::Custom);
            for (ParameterSubmenu* sub : {standard, more, custom})
                sub->onSelect = [this](const char* type) {
                    hide();
                    if (onSelect) onSelect(type);
                };
        }
    }

    // Opens one of the submenus, closing the other.
    void showSub(ParameterSubmenu* sub, ModernButton* btn) {
        if (!sub || !btn) return;
        for (ParameterSubmenu* other : {standard, more, custom})
            if (other && other != sub) other->hide();
        sub->hasFn_ = hasFn_;
        sub->rebuild(ownNames_);
        sub->place(this, y() + btn->y());
        sub->show();
    }

    ModernButton* addSubRow(int row, const char* label, ParameterSubmenu* sub,
                            void (*open)(Fl_Widget*, void*)) {
        auto* b = addRow(row, label);
        b->setSubmenuArrow(true);
        b->onEnter = [this, sub]() {
            for (ParameterSubmenu* other : {standard, more, custom})
                if (other && other != sub) other->hideSoon();
            if (sub) sub->cancelHide();
        };
        b->callback(open, this);
        return b;
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
        if (kind_ == Kind::Top) b->onEnter = [this]() {
            for (ParameterSubmenu* sub : {standard, more, custom})
                if (sub) sub->hideSoon();
        };
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
        standardBtn_ = nullptr;
        moreBtn_     = nullptr;
        customBtn_   = nullptr;
        int row = 0;

        if (kind_ == Kind::Custom) {
            for (const auto& n : ownNames)
                if (!standardParam(n)) addParamRow(row++, n);
        } else if (kind_ != Kind::Top) {
            const bool core = kind_ == Kind::Core;
            for (const auto& s : kStandardParams)
                if (s.core == core) addParamRow(row++, s.name);
        } else {
            standardBtn_ = addSubRow(row++, "Standard", standard, [](Fl_Widget*, void* d) {
                auto* self = static_cast<ParameterSubmenu*>(d);
                self->showSub(self->standard, self->standardBtn_);
            });
            moreBtn_ = addSubRow(row++, "Other standard", more, [](Fl_Widget*, void* d) {
                auto* self = static_cast<ParameterSubmenu*>(d);
                self->showSub(self->more, self->moreBtn_);
            });
            customBtn_ = addSubRow(row++, "Custom", custom, [](Fl_Widget*, void* d) {
                auto* self = static_cast<ParameterSubmenu*>(d);
                self->showSub(self->custom, self->customBtn_);
            });
            if (std::none_of(ownNames.begin(), ownNames.end(),
                             [](const std::string& n) { return !standardParam(n); }))
                customBtn_->deactivate();
            if (onNew) {
                auto* add = addRow(row++, "New parameter…");
                add->callback([](Fl_Widget*, void* d) {
                    static_cast<ParameterSubmenu*>(d)->startNew();
                }, this);
            }
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
        for (ParameterSubmenu* sub : {standard, more, custom})
            if (sub) sub->hide();
        onLearnNew();
        learningNew_ = true;
        row->copy_label("Move a control…");
        row->labelcolor(0xF59E0B00);   // amber, as the "Learning…" badge
        row->deactivate();
        redraw();
    }

    // The editor opens where the menu was, which closes along with the one it
    // was opened from.
    void startNew() {
        auto fn = onNew;
        const int wx = x(), wy = y();
        hide();
        if (parent_) parent_->hide();
        if (fn) fn(wx, wy);
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
    // "New parameter…" chosen, with where to open the editor (window coordinates).
    // Unset, the row is not shown.
    std::function<void(int wx, int wy)> onNew;
    // Cancelled if the menu closes before the control arrives.
    MidiLearnMap*                    midiLearn = nullptr;
    // The "Standard", "Other standard" and "Custom" submenus. The owner adds them
    // to the window beside this.
    ParameterSubmenu*                standard = nullptr;
    ParameterSubmenu*                more     = nullptr;
    ParameterSubmenu*                custom   = nullptr;

    ParameterSubmenu() : ParameterSubmenu(Kind::Top) {}
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
        for (ParameterSubmenu* sub : {standard, more, custom})
            if (sub) sub->hide();
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
