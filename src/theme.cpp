// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "theme.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <vector>

namespace {

// The original look: every value here is the literal the UI used before themes.
constexpr Theme kLight {
    .name             = "Light",

    .editorBg         = 0xFFFFFF00,
    .rulerBg          = 0xFEFCE800,
    .rulerBorder      = 0xD1D5DB00,
    .rulerText        = 0xA0906000,
    .gridSubdiv       = 0xDDDDDD00,
    .gridBeat         = 0x00EE0000,
    .gridBar          = 0x00660000,
    .gridRowLine      = 0xEE888800,
    .gridGroupLine    = 0x33110000,
    .pianoBlackRow    = 0xE8E8E800,
    .unusedRow        = 0xCCCCCC00,
    .paramRowBg       = 0xF0F4FF00,
    .paramCentre      = 0xD0D0E000,
    .paramLine        = 0x8888CC00,
    .paramRowSep      = 0xCCCCEE00,
    .loopRulerBg      = 0xECFDF500,
    .tempoRulerBg     = 0xFEF3C700,
    .timeSigRulerBg   = 0xEDE9FE00,
    .playheadDim      = 0xD1D5DB00,

    .tabBarBg         = 0xFFFFFF00,
    .tabInactive      = 0xF3F4F600,
    .tabSeparator     = 0xD1D5DB00,
    .tabText          = FL_FOREGROUND_COLOR,
    .gearIcon         = 0x4B556300,
    .gearIconInactive = 0x9CA3AF00,
    .transportBorder  = 0xCBD5E100,
    .transportIcon    = 0x37415100,

    .popupBg          = 0xFFFFFF00,
    .popupText        = 0x37415100,
    .popupInputBg     = 0xE4F9E900,
    .popupHover       = 0xDDEEFF00,
    .popupDivider     = 0xCBD5E100,
    .popupHeading     = 0x6B728000,
    .popupCtrlBorder  = 0x4B556300,

    .overlayBg        = 0xFFFFFF00,
    .overlayBorder    = 0xCBD5E100,
    .overlayDivider   = 0xE5E7EB00,
    .overlayText      = 0x37415100,
    .overlaySubText   = 0x6B728000,
    .overlayInputBg   = 0xF9FAFB00,
    .overlayButtonBg  = 0xF3F4F600,
    .overlayStripe    = 0xF9FAFB00,
    .sliderTrack      = 0xE5E7EB00,

    .fltkForeground   = 0,
    .fltkBackground2  = 0,
};

// Loosely after Ardour's default "dark" theme: neutral greys, a near-black
// ruler, light grey text and a muted blue selection.
constexpr Theme kDark {
    .name             = "Dark",

    .editorBg         = 0x33333300,
    .rulerBg          = 0x1E1E1E00,
    .rulerBorder      = 0x10101000,
    .rulerText        = 0x9A9A9A00,
    .gridSubdiv       = 0x3D3D3D00,
    .gridBeat         = 0x55555500,
    .gridBar          = 0x8C8C8C00,
    .gridRowLine      = 0x40404000,
    .gridGroupLine    = 0x7F7F7F00,
    .pianoBlackRow    = 0x2A2A2A00,
    .unusedRow        = 0x1E1E1E00,
    .paramRowBg       = 0x2C303800,
    .paramCentre      = 0x454A5800,
    .paramLine        = 0x6670A800,
    .paramRowSep      = 0x454A5800,
    .loopRulerBg      = 0x1F3A3000,
    .tempoRulerBg     = 0x3E342000,
    .timeSigRulerBg   = 0x33294A00,
    .playheadDim      = 0x55555500,

    .tabBarBg         = 0x3D3D3D00,
    .tabInactive      = 0x26262600,
    .tabSeparator     = 0x10101000,
    .tabText          = 0xE5E5E500,
    .gearIcon         = 0xCCCCCC00,
    .gearIconInactive = 0x6A6A6A00,
    .transportBorder  = 0x5A5A5A00,
    .transportIcon    = 0xD0D0D000,

    .popupBg          = 0x3D3D3D00,
    .popupText        = 0xE5E5E500,
    .popupInputBg     = 0x2F4A3600,
    .popupHover       = 0x4A647800,
    .popupDivider     = 0x5A5A5A00,
    .popupHeading     = 0xA8A8A800,
    .popupCtrlBorder  = 0x6A6A6A00,

    .overlayBg        = 0x3D3D3D00,
    .overlayBorder    = 0x5A5A5A00,
    .overlayDivider   = 0x4F4F4F00,
    .overlayText      = 0xE5E5E500,
    .overlaySubText   = 0xA8A8A800,
    .overlayInputBg   = 0x2A2A2A00,
    .overlayButtonBg  = 0x4F4D4D00,
    .overlayStripe    = 0x43434300,
    .sliderTrack      = 0x5A5A5A00,

    .fltkForeground   = 0xE5E5E500,
    .fltkBackground2  = 0x2A2A2A00,
};

constexpr const Theme* kThemes[] = { &kLight, &kDark };

int current = 0;

std::vector<ThemeObserver*>& observers() {
    static std::vector<ThemeObserver*> list;
    return list;
}

// Set one of FLTK's indexed colours, remembering its original value so a theme
// that leaves it at 0 can put it back.
void applyFltkColor(Fl_Color index, Fl_Color themed, Fl_Color& original, bool& saved) {
    if (!saved) { original = Fl::get_color(index); saved = true; }
    Fl::set_color(index, themed ? themed : original);
}

// Subwindows (popups, overlays) keep their own damage, so mark every widget.
void redrawAll(Fl_Widget* w) {
    w->redraw();
    if (Fl_Group* g = w->as_group())
        for (int i = 0; i < g->children(); ++i) redrawAll(g->child(i));
}

}  // namespace

const Theme& theme() { return *kThemes[current]; }

std::span<const Theme* const> themes() { return kThemes; }

int themeIndex() { return current; }

void setTheme(int index) {
    if (index < 0 || index >= (int)std::size(kThemes) || index == current) return;
    current = index;

    static Fl_Color fg, bg2;
    static bool fgSaved = false, bg2Saved = false;
    applyFltkColor(FL_FOREGROUND_COLOR,  theme().fltkForeground,  fg,  fgSaved);
    applyFltkColor(FL_BACKGROUND2_COLOR, theme().fltkBackground2, bg2, bg2Saved);

    // Copied: an observer may create or destroy others while it restyles.
    const auto list = observers();
    for (ThemeObserver* o : list)
        if (std::ranges::find(observers(), o) != observers().end())
            o->onThemeChanged();

    for (Fl_Window* w = Fl::first_window(); w; w = Fl::next_window(w))
        redrawAll(w);
}

ThemeObserver::ThemeObserver() { observers().push_back(this); }

void ThemeObserver::onThemeChanged() {
    for (auto& style : stylers_) style();
}

void ThemeObserver::onTheme(std::function<void()> style) {
    style();
    stylers_.push_back(std::move(style));
}

ThemeObserver::~ThemeObserver() { std::erase(observers(), this); }
