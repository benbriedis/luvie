// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PARAM_DEF_POPUP_HPP
#define PARAM_DEF_POPUP_HPP

#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Value_Input.H>
#include "modernChoice.hpp"
#include "modern/inputEditorPopup.hpp"
#include "paramDefs.hpp"
#include <functional>

// Edits one of an instrument's parameters: its name, what it sends (a CC, pitch
// bend or channel pressure) and the value it rests at. Opened from a param lane's
// "Edit parameter" menu item. Like the other editor popups, Enter or a click away
// applies it and Escape leaves it as it was.
class ParamDefPopup : public InputEditorPopup {
public:
    ParamDefPopup();

    // onOk gets the edited parameter; returning false (the name is taken) keeps
    // the popup open so it can be changed.
    void open(int wx, int wy, const ParamDef& def, std::function<bool(const ParamDef&)> onOk);

private:
    void doOk() override;
    void syncCcRow();
    ParamDef current() const;

    Fl_Input*       nameInput  = nullptr;
    ModernChoice*   outChoice  = nullptr;
    Fl_Box*         ccLabel    = nullptr;
    Fl_Value_Input* ccInput    = nullptr;
    Fl_Box*         ccName     = nullptr;   // the standard name of that CC, if any
    ModernChoice*   restChoice = nullptr;

    std::function<bool(const ParamDef&)> onOkCb;
};

#endif
