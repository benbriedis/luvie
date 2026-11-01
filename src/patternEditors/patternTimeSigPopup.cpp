// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "patternTimeSigPopup.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <cstdio>

static constexpr int popupW  = 170;
static constexpr int pad     = 8;
static constexpr int rowH    = 22;
static constexpr int rowGap  = 6;
static constexpr int labelW  = 34;    // "Sig" / "Bars" column
static constexpr int numW    = 32;
static constexpr int btnH    = 24;
static constexpr int btnGap  = 6;

static constexpr int sigY    = pad;
static constexpr int barsY   = sigY + rowH + rowGap;
static constexpr int btnY    = barsY + rowH + rowGap;
static constexpr int popupH  = btnY + btnH + pad;

// The pattern panel's Bars spinner tops out here, as does flexible-bars growth.
static constexpr int kMaxBars = 64;

PatternTimeSigPopup::PatternTimeSigPopup()
	: InputEditorPopup(popupW, popupH)
{
	auto styleInput = [this](Fl_Value_Input* inp) {
		inp->box(FL_FLAT_BOX);
		onTheme([inp] {
			inp->color(popupInputBg());
			inp->textcolor(popupText());
			inp->cursor_color(popupText());
			inp->labelcolor(popupText());
		});
	};
	auto label = [this](int y, const char* text) {
		auto* lbl = new Fl_Box(pad, y, labelW, rowH, text);
		onTheme([lbl] { lbl->labelcolor(popupText()); });
		lbl->box(FL_NO_BOX);
		lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
	};

	const int fieldX = pad + labelW;

	label(sigY, "Sig");
	numInput = new Fl_Value_Input(fieldX, sigY, numW, rowH);
	numInput->range(timeSettings::numeratorMin, timeSettings::numeratorMax);
	numInput->step(1);
	styleInput(numInput);
	auto* slash = new Fl_Box(fieldX + numW + 2, sigY, 10, rowH, "/");
	onTheme([slash] { slash->labelcolor(popupText()); });
	slash->box(FL_NO_BOX);
	denomChoice = new DenomBeatChoice(fieldX + numW + 16, sigY, 0, rowH);
	denomChoice->size(denomChoice->naturalWidth(), rowH);
	onTheme([c = denomChoice] {
		c->color(popupInputBg());
		c->labelcolor(popupText());
		c->textcolor(popupText());
		c->setBorderColor(theme().popupCtrlBorder);
		c->setArrowColor(popupText());
	});

	label(barsY, "Bars");
	barsValue = new Fl_Box(fieldX, barsY, numW, rowH);
	onTheme([b = barsValue] { b->labelcolor(popupText()); });
	barsValue->box(FL_NO_BOX);
	barsValue->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
	const int btnW = (int)std::max(fl_width("OK"), fl_width("Cancel")) + 24;
	okBtn     = new ModernButton(pad, btnY, btnW, btnH, "OK");
	cancelBtn = new ModernButton(pad + btnW + btnGap, btnY, btnW, btnH, "Cancel");
	onTheme([this] {
		for (auto* b : {okBtn, cancelBtn}) {
			b->color(popupBg());
			b->labelcolor(popupText());
		}
	});

	end();

	// The numerator snaps the beat to the one the signature implies, as everywhere
	// else a signature is edited; the dropdown can then pick another.
	numInput->callback([](Fl_Widget*, void* d) {
		auto* self = static_cast<PatternTimeSigPopup*>(d);
		int den = self->denomChoice->denominator();
		self->denomChoice->set(den, timeSettings::impliedBeatUnit(self->numerator(), den));
		self->denomChoice->redraw();
		self->sigChanged();
	}, this);
	denomChoice->callback([](Fl_Widget*, void* d) {
		static_cast<PatternTimeSigPopup*>(d)->sigChanged();
	}, this);
	okBtn->callback([](Fl_Widget*, void* d) {
		static_cast<PatternTimeSigPopup*>(d)->doOk();
	}, this);
	cancelBtn->callback([](Fl_Widget*, void* d) {
		static_cast<PatternTimeSigPopup*>(d)->dismiss();
	}, this);
}

// Fl_Value_Input honours its range() only while dragging; a typed value is
// accepted verbatim, so clamp it here at every read.
int PatternTimeSigPopup::numerator() const
{
	return std::clamp((int)numInput->value(),
	                  timeSettings::numeratorMin, timeSettings::numeratorMax);
}

int PatternTimeSigPopup::barsToFit() const
{
	// The pattern's length in the new denominator's units, then whole bars of the
	// new numerator over it. The slack keeps float noise from buying an extra bar.
	const float scale = origBottom > 0 ? (float)denomChoice->denominator() / origBottom : 1.0f;
	const float beats = lengthBeats * scale;
	const int   need  = (int)std::ceil(beats / numerator() - 1.0e-3f);
	return std::clamp(need, 1, kMaxBars);
}

void PatternTimeSigPopup::sigChanged()
{
	char buf[8];
	std::snprintf(buf, sizeof buf, "%d", barsToFit());
	barsValue->copy_label(buf);
	barsValue->redraw();
}

void PatternTimeSigPopup::open(int wx, int wy, int top, int bottom, timeSettings::BeatUnit beat,
                               float length,
                               std::function<void(int, int, timeSettings::BeatUnit, int)> onOk)
{
	origTop     = top;
	origBottom  = bottom;
	origBeat    = beat;
	lengthBeats = length;
	origBars    = std::max(1, (int)std::round(length / (float)std::max(1, top)));
	onOkCb      = std::move(onOk);

	numInput->value(top);
	denomChoice->set(bottom, beat);
	sigChanged();
	openEditor(wx, wy, numInput);
}

void PatternTimeSigPopup::doOk()
{
	const int top = numerator(), bottom = denomChoice->denominator(), n = barsToFit();
	const timeSettings::BeatUnit beat = denomChoice->beatUnit();
	const bool changed = top != origTop || bottom != origBottom || beat != origBeat
	                  || n != origBars;
	if (changed && onOkCb) onOkCb(top, bottom, beat, n);
	commit();
}

int PatternTimeSigPopup::handle(int event)
{
	if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
		if (Fl::focus() == cancelBtn) { dismiss(); return 1; }
	}
	return InputEditorPopup::handle(event);
}
