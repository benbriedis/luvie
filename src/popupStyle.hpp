// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef POPUP_STYLE_HPP
#define POPUP_STYLE_HPP

#include "theme.hpp"
#include <FL/Enumerations.H>

inline Fl_Color popupBg()      { return theme().popupBg; }
inline Fl_Color popupInputBg() { return theme().popupInputBg; }
inline Fl_Color popupText()    { return theme().popupText; }
static constexpr Fl_Color popupAccent = 0xF59E0B00;


#endif
