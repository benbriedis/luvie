// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "controlBar.hpp"
#include "panelStyle.hpp"
#include <FL/fl_draw.H>
#include <algorithm>

static int gapFor(const PanelItem& it, int standard)
{
    return it.gapBefore >= 0 ? it.gapBefore : standard;
}

int ControlBar::rowWidth(const PanelRow& row)
{
    int total = 2 * pad + row.indent;
    bool first = true;
    for (const auto* run : { &row.left, &row.right })
        for (const auto& it : *run) {
            if (!it.widget->visible()) continue;
            if (!first) total += gapFor(it, itemGap);
            total += it.width;
            first  = false;
        }
    return total;
}

void ControlBar::applyLayout(const std::vector<PanelRow>& rows)
{
    int ry = y() + vMargin;
    for (const auto& row : rows) {
        int lx    = x() + pad + row.indent;
        bool first = true;
        for (const auto& it : row.left) {
            if (!it.widget->visible()) continue;
            if (!first) lx += gapFor(it, itemGap);
            it.widget->resize(lx, ry, it.width, rowH);
            lx   += it.width;
            first = false;
        }
        int rx = x() + w() - pad;
        for (auto it = row.right.rbegin(); it != row.right.rend(); ++it) {
            if (!it->widget->visible()) continue;
            rx -= it->width;
            it->widget->resize(std::max(rx, lx + (first ? 0 : gapFor(*it, itemGap))),
                               ry, it->width, rowH);
            rx -= gapFor(*it, itemGap);
        }
        ry += rowH + rowGap;
    }
}

void ControlBar::relayout()
{
    auto rows = buildLayout(w());
    applyLayout(rows);
    afterLayout();

    int wanted = rowsHeight((int)rows.size());
    if (wanted != h() && onHeightChanged) onHeightChanged(wanted);
    redraw();
}

void ControlBar::resize(int x, int y, int w, int h)
{
    Fl_Widget::resize(x, y, w, h);
    relayout();
}

void ControlBar::draw()
{
    fl_color(panelBorder);
    fl_rectf(x(), y(), w(), 1);
    fl_color(panelBg);
    fl_rectf(x(), y() + 1, w(), h() - 1);
    draw_children();
}
