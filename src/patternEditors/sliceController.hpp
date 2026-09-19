// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SLICE_CONTROLLER_HPP
#define SLICE_CONTROLLER_HPP

#include "clipboard.hpp"
#include "sliceClip.hpp"
#include <functional>
#include <vector>

class Fl_Widget;
class ObservablePattern;

// A time slice: the second kind of selection in the pattern editors, alongside
// the item selection. It is a stretch of beats [start, end) taken top to bottom
// — the notes or drum hits and every automation lane together — swept out with
// Alt-drag, dragged sideways to move it, and copied, cut, deleted and pasted
// through the same commands as the item selection.
//
// One per pattern editor, shared by the widgets that make up its body (the note
// or drum grid and the automation lanes), because the slice runs through all of
// them. Those widgets own the pixels and pass beats in; everything the gesture
// means is decided here, so the three do not each grow their own copy of it.
class SliceController {
public:
    // What a left press did. Dismissed means a slice was dropped by a press
    // that has other business (a Shift-band, a ctrl-toggle); the caller carries
    // on with it. Consumed means the press was the slice's, and the caller must
    // not also create, remove or drag anything.
    enum class Press { Ignored, Consumed, Dismissed };

    void setPattern(ObservablePattern* p, int patId);
    void setSnap(float s) { snap = s; }
    // Every widget the slice is drawn across, so a change redraws them all.
    void addView(Fl_Widget* w) { views.push_back(w); }
    // Called when a sweep begins, from whichever widget it began in, so the
    // grid can drop its item selection: only one kind is live at a time.
    std::function<void()> onSweepStart;

    bool active() const { return has; }
    // A sweep or a move-drag is in progress: the widget that took the press
    // routes its drags and release here.
    bool busy()   const { return sweeping || dragging; }
    bool contains(float beat) const { return has && beat >= start && beat < end; }

    // `beat` is the unsnapped beat under the cursor.
    Press press(float beat, bool alt, bool otherModifiers);
    void  drag(float beat);
    void  release();
    void  clear();

    // Where the band is to be drawn this frame, in beats: the slice itself, the
    // band being swept, or where a move-drag would put the slice. False when
    // there is nothing to draw.
    bool  band(float& from, float& to) const;
    // How far a move-drag is currently carrying the slice's contents. Zero
    // outside a drag; items overlapping the slice are previewed shifted by this.
    float dragOffset() const { return dragging ? dBeat : 0.0f; }
    float sliceStart() const { return start; }
    float sliceEnd()   const { return end; }

    // Commands. Each is one edit, so one undo.
    void copy() const;
    void deleteRange();
    // True if the clipboard holds a slice this pattern could take.
    bool canPaste() const;
    // Paste with the slice's start on `beat` (snapped down to the grid, as the
    // item paste anchors). False, and nothing changed, if it does not fit there.
    bool pasteAt(float beat);

    // Draw the band across a widget whose beat 0 sits at x = `beat0X`.
    void draw(int beat0X, int y, int h, int colWidth) const;

private:
    ObservablePattern* pattern   = nullptr;
    int                patternId = -1;
    float              snap      = 0.0f;
    std::vector<Fl_Widget*> views;

    bool  has = false;
    float start = 0.0f, end = 0.0f;

    bool  sweeping = false;
    float anchor = 0.0f, cur = 0.0f;

    // Move-drag. The contents are captured when it begins, so the travel limits
    // can allow for notes carried whole past either edge.
    bool  dragging = false;
    float grabBeat = 0.0f, dBeat = 0.0f, minD = 0.0f, maxD = 0.0f;

    float patternBeats() const;
    ClipKind kind() const;
    float snapped(float beat) const;
    void  changed() const;
};

#endif
