// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SELECTION_HPP
#define SELECTION_HPP

#include <algorithm>
#include <unordered_set>
#include <vector>

class SelectionContextPopup;
class PasteContextPopup;

// A set of selected item ids, plus the geometry of the rubber band being dragged
// to build one. Held by each editing grid; the ids are model ids (Note::id,
// DrumNote::id, PatternInstance::id, ParamPoint::id) rather than indices into a
// grid's local note cache, because that cache is rebuilt — and re-ordered — on
// every timeline change.
class Selection {
public:
    bool contains(int id) const { return ids_.count(id) != 0; }
    bool empty()          const { return ids_.empty(); }
    int  size()           const { return (int)ids_.size(); }

    void add(int id)    { ids_.insert(id); }
    void clear()        { ids_.clear(); }
    void toggle(int id) { if (!ids_.erase(id)) ids_.insert(id); }

    void set(const std::vector<int>& ids) { ids_.clear(); ids_.insert(ids.begin(), ids.end()); }

    const std::unordered_set<int>& ids() const { return ids_; }

    // Drop ids the model no longer has, so a deleted item cannot linger in the
    // selection and resurrect as a stale id later.
    void retain(const std::unordered_set<int>& live)
    {
        for (auto it = ids_.begin(); it != ids_.end(); )
            it = live.count(*it) ? std::next(it) : ids_.erase(it);
    }

    // ── Rubber band ──────────────────────────────────────────────────────────
    // Live while a band drag is in progress. Coordinates are widget-relative
    // pixels; the anchor is where the drag started, so either corner may lead.
    bool active = false;
    int  anchorX = 0, anchorY = 0, curX = 0, curY = 0;

    void beginBand(int px, int py) { active = true; anchorX = curX = px; anchorY = curY = py; }
    void updateBand(int px, int py) { curX = px; curY = py; }
    void endBand() { active = false; }

    int bandLeft()   const { return std::min(anchorX, curX); }
    int bandRight()  const { return std::max(anchorX, curX); }
    int bandTop()    const { return std::min(anchorY, curY); }
    int bandBottom() const { return std::max(anchorY, curY); }

    // The band is asymmetric by design. Horizontally any overlap counts, because
    // a long note or a multi-bar pattern instance is often wider than a
    // comfortable drag and would otherwise be unselectable without zooming out.
    // Vertically the row must be covered through its centre: rows are only tens
    // of pixels tall, so a band that grazes the row above would otherwise pick up
    // notes the user never meant to touch.
    bool bandCoversRow(int rowTopPx, int rowHeightPx) const
    {
        const int centre = rowTopPx + rowHeightPx / 2;
        return centre >= bandTop() && centre <= bandBottom();
    }

    // Point items (drum hits, automation dots) have no extent, so both axes
    // reduce to a plain containment test of the point itself.
    bool bandContainsPoint(int px, int py) const
    {
        return px >= bandLeft() && px <= bandRight() &&
               py >= bandTop()  && py <= bandBottom();
    }

private:
    std::unordered_set<int> ids_;
};

// Force a drag-limit interval to contain zero. Every grid builds its limits by
// intersecting one range per selected item, and an item that is already out of
// bounds — an instance extending past the last column, say — contributes a range
// that excludes zero, which can leave the intersection empty. Clamping a delta
// to an empty interval is undefined and in practice shoves the whole selection
// the other way, off the front of the song. An out-of-bounds item simply cannot
// move further out; it must never force the selection to move.
template <typename T>
inline void includeZero(T& lo, T& hi) { lo = std::min(lo, T{}); hi = std::max(hi, T{}); }

// Implemented by every grid that owns a Selection, so window-level commands
// (Escape to clear, click-away to clear) can reach whichever grid is showing
// without the caller knowing which kind of grid it is. DrumGrid is not a Grid —
// it is a separate widget over a different item type — so a shared base class
// will not do.
class ISelectionHost {
public:
    virtual ~ISelectionHost() = default;
    virtual void clearSelection()      = 0;
    virtual void selectAllItems()      = 0;
    // Remove every selected item, in one batched edit.
    virtual void deleteSelectedItems() = 0;
    virtual bool hasSelection() const  = 0;
    // Put the selection on the shared clipboard (ctrl-C, or the selection menu).
    // Nothing selected copies nothing, leaving an earlier copy where it was.
    virtual void copySelection()       = 0;
    // Paste the clipboard at (wx, wy), in window coordinates. The position is
    // passed rather than read from the current event because the paste menu's
    // item runs long after the right-click that chose the spot, by which time
    // the event position is the click on the menu itself. Only called for a
    // point inside this host, so a copy that will not fit there is the host's
    // own to report — see flashForbiddenCursor.
    virtual void pasteClipboard(int wx, int wy) = 0;
    // Ctrl-X, and the selection menu's Cut. Copying and removing are both here
    // already, so cut is the two in order — copy first, while the items still
    // exist. Everything selected goes, including anything copySelection() chose
    // not to take (the song editor's automation dots), exactly as Delete does.
    void cutSelection() { copySelection(); deleteSelectedItems(); }
    // The menu shown when a right-click lands on a member of the selection,
    // instead of the menu for the single item under the cursor. One instance is
    // shared by every host: only one editor is on screen at a time.
    virtual void setSelectionPopup(SelectionContextPopup* p) = 0;
    // The menu offered by a right-click on empty grid when there is something on
    // the clipboard to paste there. Shared for the same reason.
    virtual void setPastePopup(PasteContextPopup* p) = 0;
    // True when this host's widget is on screen. Only one editor shows at a time,
    // so it is what picks the grid a window-level command was meant for.
    virtual bool showing() const = 0;
    // True when this point, in window coordinates, lies inside the widget that
    // owns the selection. A click outside it dismisses the selection; a click
    // inside is the grid's own business (band drags, ctrl-toggles, drags).
    virtual bool ownsWindowPoint(int wx, int wy) const = 0;
};

#endif
