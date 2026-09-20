// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef TIME_SIG_SECTION_HPP
#define TIME_SIG_SECTION_HPP

#include <FL/Fl_Box.H>
#include <FL/Fl_Flex.H>
#include "modern/denomBeatChoice.hpp"
#include "modern/modernValueInput.hpp"

// Time signature: "Sig [n] / [d]". The denominator dropdown also carries the beat
// definition (a plain denominator counts in crotchets; the dotted variants show a
// note glyph), which is why it is a DenomBeatChoice rather than a plain number.
//
// Lives here rather than in patternPanel.hpp because two control bars want it: the
// pattern editor's, where it is the pattern's own signature, and the Loop Editor's,
// where it is Loop Mode's. Wiring is the owner's — this is layout only.
struct TimeSigSection : Fl_Flex {
    static constexpr int kGap    = 3;
    static constexpr int kLabelW = 28;
    static constexpr int kNumW   = 26;
    static constexpr int kSlashW = 12;
    static constexpr int kDenW   = 54;   // hugs DenomBeatChoice::naturalWidth()
    static constexpr int kWidth  = kLabelW + kGap + kNumW + kGap + kSlashW + kGap + kDenW;
    Fl_Box           timeSigLabel;
    ModernValueInput timeSigNum;
    Fl_Box           timeSigSlash;
    DenomBeatChoice  timeSigDen;
    TimeSigSection(int x, int y, int h);
};

#endif
