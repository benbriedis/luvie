// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "paramDefPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <algorithm>

static constexpr int popupW = 230;
static constexpr int pad    = 8;
static constexpr int rowH   = 22;
static constexpr int rowGap = 6;
static constexpr int labelW = 60;
static constexpr int ccW    = 46;
static constexpr int rangeW = 60;   // fits 16383
static constexpr int fieldX = pad + labelW;
static constexpr int fieldW = popupW - fieldX - pad;

static constexpr int nameY = pad;
static constexpr int inY   = nameY + rowH + rowGap;
static constexpr int inCcY = inY   + rowH + rowGap;
static constexpr int outY  = inCcY + rowH + rowGap;
static constexpr int ccY   = outY  + rowH + rowGap;
static constexpr int minY  = ccY   + rowH + rowGap;
static constexpr int maxY  = minY  + rowH + rowGap;
static constexpr int restY = maxY  + rowH + rowGap;
static constexpr int valueY = restY + rowH + rowGap;
static constexpr int popupH = valueY + rowH + pad;

// Choice indices, in ParamOutKind / ParamRest / MidiSrcKind order.
static constexpr const char* kOutLabels[]  = {"CC", "Pitch bend", "Pressure"};
static constexpr const char* kRestLabels[] = {"Min", "Centre", "Max"};
static constexpr const char* kInLabels[]   = {"Nothing", "CC", "Pitch bend", "Pressure"};

ParamDefPopup::ParamDefPopup()
    : InputEditorPopup(popupW, popupH)
{
    auto label = [](int y, const char* text) {
        auto* b = new Fl_Box(pad, y, labelW, rowH, text);
        b->labelcolor(popupText);
        b->box(FL_NO_BOX);
        b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        return b;
    };
    auto ccField = [this](int y) {
        auto* in = new Fl_Value_Input(fieldX, y, ccW, rowH);
        in->range(0, 127);
        in->step(1);
        in->box(FL_FLAT_BOX);
        in->color(popupInputBg);
        in->textcolor(popupText);
        in->cursor_color(popupText);
        in->when(FL_WHEN_CHANGED);
        in->callback([](Fl_Widget*, void* d) {
            static_cast<ParamDefPopup*>(d)->syncCcRows();
        }, this);
        return in;
    };
    auto styleChoice = [this](ModernChoice* c) {
        c->color(popupInputBg);
        c->labelcolor(popupText);
        c->textcolor(popupText);
        c->setBorderColor(0x4B556300);
        c->setArrowColor(popupText);
        c->callback([](Fl_Widget*, void* d) {
            static_cast<ParamDefPopup*>(d)->syncCcRows();
        }, this);
    };

    label(nameY, "Name");
    nameInput = new Fl_Input(fieldX, nameY, fieldW, rowH);
    nameInput->box(FL_FLAT_BOX);
    nameInput->color(popupInputBg);
    nameInput->textcolor(popupText);
    nameInput->cursor_color(popupText);

    label(inY, "Receives");
    inChoice = new ModernChoice(fieldX, inY, fieldW, rowH);
    for (const char* l : kInLabels) inChoice->add(l);
    styleChoice(inChoice);

    inCcLabel = label(inCcY, "CC");
    inCcInput = ccField(inCcY);

    label(outY, "Sends");
    outChoice = new ModernChoice(fieldX, outY, fieldW, rowH);
    for (const char* l : kOutLabels) outChoice->add(l);
    styleChoice(outChoice);

    ccLabel = label(ccY, "CC");
    ccInput = ccField(ccY);
    ccName = new Fl_Box(fieldX + ccW + 6, ccY, fieldW - ccW - 6, rowH);
    ccName->labelcolor(popupText);
    ccName->box(FL_NO_BOX);
    ccName->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    label(minY, "Min");
    minInput = ccField(minY);
    minInput->size(rangeW, rowH);
    label(maxY, "Max");
    maxInput = ccField(maxY);
    maxInput->size(rangeW, rowH);

    label(restY, "Rests");
    restChoice = new ModernChoice(fieldX, restY, fieldW, rowH);
    for (const char* l : kRestLabels) restChoice->add(l);
    styleChoice(restChoice);

    label(valueY, "Value");
    valueText = new Fl_Box(fieldX, valueY, fieldW, rowH);
    valueText->labelcolor(popupText);
    valueText->box(FL_NO_BOX);
    valueText->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    end();
}

// A CC row only means something for a CC. The Sends one names the standard use of
// the number chosen, as a guide.
void ParamDefPopup::syncCcRows()
{
    auto enable = [](bool on, std::initializer_list<Fl_Widget*> ws) {
        for (Fl_Widget* w : ws) { if (on) w->activate(); else w->deactivate(); }
    };
    syncRange((ParamOutKind)std::clamp(outChoice->value(), 0, 2));
    const bool isCC = outChoice->value() == (int)ParamOutKind::CC;
    enable(isCC, {ccLabel, ccInput});
    enable(inChoice->value() == (int)MidiSrcKind::CC, {inCcLabel, inCcInput});
    const int cc = std::clamp((int)ccInput->value(), 0, 127);
    const StandardParam* s = isCC ? standardParamFor(ParamOutKind::CC, cc) : nullptr;
    ccName->copy_label(s ? s->name : "");
    refreshValue();
    redraw();
}

// The value as its lane has it and, where Min and Max narrow the output, what that
// sends — following the popup's settings, applied or not. "-" until one arrives.
void ParamDefPopup::refreshValue()
{
    const std::optional<int> v = valueCb ? valueCb() : std::nullopt;
    std::string text = "-";
    if (v) {
        const ParamDef d   = current();
        const int      out = paramOutValue(d, *v);
        text = std::to_string(*v);
        if (paramOutMin(d) != 0 || paramOutMax(d) != paramMaxValue(d))
            text += ", sends " + std::to_string(out);
    }
    valueText->copy_label(text.c_str());
    valueText->redraw();
}

// Min and Max are in the units of what is sent. Moving to an output of another size
// keeps a full range full, and otherwise keeps what fits.
void ParamDefPopup::syncRange(ParamOutKind kind)
{
    const int top = paramMaxValue(kind);
    if (kind != rangeKind) {
        const int oldTop = paramMaxValue(rangeKind);
        const bool full  = (int)minInput->value() == 0 && (int)maxInput->value() == oldTop;
        minInput->value(full ? 0   : std::clamp((int)minInput->value(), 0, top));
        maxInput->value(full ? top : std::clamp((int)maxInput->value(), 0, top));
        rangeKind = kind;
    }
    minInput->range(0, top);
    maxInput->range(0, top);
}

ParamDef ParamDefPopup::current() const
{
    ParamDef d;
    d.name = nameInput->value();
    // Surrounding spaces would make two names that look the same.
    const auto first = d.name.find_first_not_of(' ');
    const auto last  = d.name.find_last_not_of(' ');
    d.name = first == std::string::npos ? std::string{} : d.name.substr(first, last - first + 1);
    d.kind = (ParamOutKind)std::clamp(outChoice->value(), 0, 2);
    d.cc   = std::clamp((int)ccInput->value(), 0, 127);
    d.rest = (ParamRest)std::clamp(restChoice->value(), 0, 2);
    const int top = paramMaxValue(d);
    d.outMin = std::clamp((int)minInput->value(), 0, top);
    const int hi = std::clamp((int)maxInput->value(), 0, top);
    d.outMax = hi == top ? -1 : hi;
    return d;
}

MidiSrc ParamDefPopup::currentReceives() const
{
    MidiSrc src;
    src.kind = (MidiSrcKind)std::clamp(inChoice->value(), 0, 3);
    if (src.kind == MidiSrcKind::CC) src.num = std::clamp((int)inCcInput->value(), 0, 127);
    return src;
}

void ParamDefPopup::doOk()
{
    if (onOkCb && !onOkCb(current(), currentReceives())) {
        fl_beep();
        nameInput->take_focus();
        return;
    }
    commit();
}

void ParamDefPopup::open(int wx, int wy, const ParamDef& def, const MidiSrc& receives,
                         OkFn onOk, ValueFn valueFn)
{
    valueCb = std::move(valueFn);
    nameInput->value(def.name.c_str());
    outChoice->value((int)def.kind);
    ccInput->value(def.cc);
    restChoice->value((int)def.rest);
    rangeKind = def.kind;
    minInput->value(paramOutMin(def));
    maxInput->value(paramOutMax(def));
    inChoice->value((int)receives.kind);
    // Unbound or not a CC: offer the CC the parameter sends, the usual pairing.
    inCcInput->value(receives.kind == MidiSrcKind::CC ? receives.num : def.cc);
    syncCcRows();
    onOkCb = std::move(onOk);
    openEditor(wx, wy, nameInput);
    nameInput->insert_position(0, nameInput->size());
}
