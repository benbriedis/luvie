// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SONG_PANEL_HPP
#define SONG_PANEL_HPP

#include "controlBar.hpp"
#include "modernChoice.hpp"
#include <functional>

// The Song Editor's control bar: the dark strip between the grid and the
// transport, matching the pattern editors' panel. It carries the horizontal
// zoom for now.
class SongPanel : public ControlBar {
    static constexpr int zoomChoiceW = 62;

    ModernChoice zoomChoice;   // tooltip-only (no label), as in the pattern panel

    std::vector<PanelRow> buildLayout(int availW) override;

    void initZoomChoice();

public:
    SongPanel(int x, int y, int w, int h);

    // The column width multiplier the dropdown currently shows (1 = the width
    // the grid is built at).
    float zoomFactor() const;

    std::function<void(float)> onZoomChanged;
};

#endif
