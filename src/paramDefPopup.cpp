// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "paramDefPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <algorithm>

static constexpr int popupW = 220;
static constexpr int pad    = 8;
static constexpr int rowH   = 22;
static constexpr int rowGap = 6;
static constexpr int labelW = 50;
static constexpr int fieldX = pad + labelW;
static constexpr int fieldW = popupW - fieldX - pad;

static constexpr int nameY = pad;
static constexpr int outY  = nameY + rowH + rowGap;
static constexpr int ccY   = outY  + rowH + rowGap;
static constexpr int restY = ccY   + rowH + rowGap;
static constexpr int popupH = restY + rowH + pad;

// Choice indices, in ParamOutKind / ParamRest order.
static constexpr const char* kOutLabels[]  = {"CC", "Pitch bend", "Pressure"};
static constexpr const char* kRestLabels[] = {"Min", "Centre", "Max"};

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
    auto styleChoice = [](ModernChoice* c) {
        c->color(popupInputBg);
        c->labelcolor(popupText);
        c->textcolor(popupText);
        c->setBorderColor(0x4B556300);
        c->setArrowColor(popupText);
    };

    label(nameY, "Name");
    nameInput = new Fl_Input(fieldX, nameY, fieldW, rowH);
    nameInput->box(FL_FLAT_BOX);
    nameInput->color(popupInputBg);
    nameInput->textcolor(popupText);
    nameInput->cursor_color(popupText);

    label(outY, "Sends");
    outChoice = new ModernChoice(fieldX, outY, fieldW, rowH);
    for (const char* l : kOutLabels) outChoice->add(l);
    styleChoice(outChoice);
    outChoice->callback([](Fl_Widget*, void* d) {
        static_cast<ParamDefPopup*>(d)->syncCcRow();
    }, this);

    ccLabel = label(ccY, "CC");
    constexpr int ccW = 46;
    ccInput = new Fl_Value_Input(fieldX, ccY, ccW, rowH);
    ccInput->range(0, 127);
    ccInput->step(1);
    ccInput->box(FL_FLAT_BOX);
    ccInput->color(popupInputBg);
    ccInput->textcolor(popupText);
    ccInput->cursor_color(popupText);
    ccInput->when(FL_WHEN_CHANGED);
    ccInput->callback([](Fl_Widget*, void* d) {
        static_cast<ParamDefPopup*>(d)->syncCcRow();
    }, this);
    ccName = new Fl_Box(fieldX + ccW + 6, ccY, fieldW - ccW - 6, rowH);
    ccName->labelcolor(popupText);
    ccName->box(FL_NO_BOX);
    ccName->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    label(restY, "Rests");
    restChoice = new ModernChoice(fieldX, restY, fieldW, rowH);
    for (const char* l : kRestLabels) restChoice->add(l);
    styleChoice(restChoice);

    end();
}

// The CC row only means something for a CC, and names the standard use of the
// number chosen, as a guide.
void ParamDefPopup::syncCcRow()
{
    const bool isCC = outChoice->value() == (int)ParamOutKind::CC;
    if (isCC) { ccLabel->activate(); ccInput->activate(); }
    else      { ccLabel->deactivate(); ccInput->deactivate(); }
    const int cc = std::clamp((int)ccInput->value(), 0, 127);
    const StandardParam* s = isCC ? standardParamFor(ParamOutKind::CC, cc) : nullptr;
    ccName->copy_label(s ? s->name : "");
    redraw();
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
    return d;
}

void ParamDefPopup::doOk()
{
    if (onOkCb && !onOkCb(current())) {
        fl_beep();
        nameInput->take_focus();
        return;
    }
    commit();
}

void ParamDefPopup::open(int wx, int wy, const ParamDef& def,
                         std::function<bool(const ParamDef&)> onOk)
{
    nameInput->value(def.name.c_str());
    outChoice->value((int)def.kind);
    ccInput->value(def.cc);
    restChoice->value((int)def.rest);
    syncCcRow();
    onOkCb = std::move(onOk);
    openEditor(wx, wy, nameInput);
    nameInput->insert_position(0, nameInput->size());
}
