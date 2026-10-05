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
#include "timelineIO.hpp"   // MidiSrc
#include <functional>
#include <optional>

// Edits one of an instrument's parameters: its name, the control it receives from
// (its MIDI-learn binding, set by hand), what it sends (a CC, pitch bend or channel
// pressure), the range of that output its lane sweeps, and the value it rests at.
// It also shows the parameter's current value, live. Opened from a param lane's
// "Edit parameter" menu item. Like the other editor popups, Enter or a click away
// applies it and Escape leaves it as it was.
class ParamDefPopup : public InputEditorPopup {
public:
    ParamDefPopup();

    using OkFn    = std::function<bool(const ParamDef&, const MidiSrc& receives)>;
    // The parameter's current value in lane units, if it has one.
    using ValueFn = std::function<std::optional<int>()>;

    // `receives` is the control bound to the parameter, kind None for none. onOk
    // gets the edited parameter and control; returning false (the name is taken)
    // keeps the popup open so it can be changed.
    // valueFn gives the value shown; refreshValue() reads it again.
    void open(int wx, int wy, const ParamDef& def, const MidiSrc& receives, OkFn onOk,
              ValueFn valueFn = {});
    void refreshValue();

private:
    void doOk() override;
    void syncCcRows();
    void syncRange(ParamOutKind kind);
    ParamDef current() const;
    MidiSrc  currentReceives() const;

    Fl_Input*       nameInput  = nullptr;
    ModernChoice*   outChoice  = nullptr;
    Fl_Box*         ccLabel    = nullptr;
    Fl_Value_Input* ccInput    = nullptr;
    Fl_Box*         ccName     = nullptr;   // the standard name of that CC, if any
    Fl_Value_Input* minInput   = nullptr;
    Fl_Value_Input* maxInput   = nullptr;
    ModernChoice*   restChoice = nullptr;
    ModernChoice*   inChoice   = nullptr;
    Fl_Box*         inCcLabel  = nullptr;
    Fl_Value_Input* inCcInput  = nullptr;
    Fl_Box*         valueText  = nullptr;

    ParamOutKind rangeKind = ParamOutKind::CC;   // the output Min and Max are in

    OkFn    onOkCb;
    ValueFn valueCb;
};

#endif
