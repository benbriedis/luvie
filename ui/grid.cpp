#include "grid.hpp"
#include "playhead.hpp"
#include "editor.hpp"
#include <FL/Fl.H>
#include "FL/Enumerations.H"
#include "popup.hpp"
#include <ranges>
#include <algorithm>
#include <cmath>
#include <FL/fl_draw.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_RGB_Image.H>
#include "cursors.hpp"
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>

using std::vector;

Grid::Grid(vector<Note> notes, int numRows, int numCols, int rowHeight, int colWidth, float snap, Popup& popup) :
    notes(notes), numRows(numRows), numCols(numCols), rowHeight(rowHeight), colWidth(colWidth), snap(snap), popup(popup),
    Fl_Box(0, 0, numCols * colWidth, numRows * rowHeight, nullptr)
{}

void Grid::draw()
{
    Fl_Box::draw();

    fl_push_clip(x(), y(), w() + 1, h() + 1);

    fl_color(bgColor);
    fl_rectf(x(), y(), w(), h());

    int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
    for (int i = 0; i < numRows + 1; i++) {
        fl_color(rowLineColor(i));
        fl_line(x(), y() + i * rowHeight, x() + gridRight, y() + i * rowHeight);
    }

    int endCol = colOffset + w() / colWidth + 2;
    for (int i = colOffset; i <= std::min(endCol, numCols); i++) {
        int x0 = x() + (i - colOffset) * colWidth;
        fl_color(columnColor(i));
        fl_line(x0, y(), x0, y() + numRows * rowHeight);
    }

    for (const Note note : notes) {
        int x0    = x() + (int)((note.beat - colOffset) * colWidth);
        int y0    = y() + (int)(note.pitch * rowHeight);
        int width = (int)(note.length * colWidth);
        if (x0 + width < x() || x0 > x() + w()) continue;
        fl_rectf(x0, y0 + 1, width, rowHeight - 1, 0x5555EE00);
        const int barWidth = 5;
        fl_color(0x1111EE00);
        fl_line_style(FL_SOLID, barWidth);
        fl_line(x0 + barWidth / 2, y0 + 1, x0 + barWidth / 2, y0 + rowHeight - 1);
        fl_line_style(0);
    }

    if (playhead)
        playhead->drawLine(x() - colOffset * colWidth, y(), numRows * rowHeight);

    fl_pop_clip();
}

int Grid::handle(int event)
{
    if (popup.visible())
        return 0;

    switch (event) {
        case FL_PUSH:
            if (Fl::event_button() == FL_RIGHT_MOUSE) {
                if (hoverState != NONE) {
                    auto onDelete = makeDeleteCallback();
                    popup.open(selectedNote, &notes, this, std::move(onDelete));
                }
            } else if (hoverState == NONE) {
                int ex    = Fl::event_x() - x();
                int row   = (Fl::event_y() - y()) / rowHeight;
                float col = (float)(ex / colWidth) + colOffset;
                int gridRight = std::min(w(), (numCols - colOffset) * colWidth);
                creationForbidden = ex >= gridRight;
                if (!creationForbidden) {
                    bool wouldRemove = std::any_of(notes.begin(), notes.end(),
                        [=](const Note& n) { return n.pitch == row && n.beat == col; });
                    if (!wouldRemove) {
                        creationForbidden = std::any_of(notes.begin(), notes.end(),
                            [=](const Note& n) { return n.pitch == row && col < n.beat + n.length && col + 1.0f > n.beat; });
                        if (creationForbidden)
                            window()->cursor(forbiddenCursorImage(), 11, 11);
                    }
                }
            } else if (Fl::event_clicks() == 1) {
                onNoteDoubleClick();
            } else {
                onBeginDrag();
            }
            return 1;

        case FL_DRAG:
            if (hoverState == MOVING)   moving();
            if (hoverState == RESIZING) resizing();
            return 1;

        case FL_RELEASE:
            if (hoverState == MOVING && amOverlapping) {
                notes[selectedNote].pitch = lastValidPosition.row;
                notes[selectedNote].beat = lastValidPosition.col;
                redraw();
            }
            onCommitDrag();
            if (hoverState == NONE && Fl::event_button() == FL_LEFT_MOUSE) {
                if (!creationForbidden)
                    toggleNote();
                creationForbidden = false;
                window()->cursor(FL_CURSOR_DEFAULT);
            }
            return 1;

        case FL_ENTER:
            return 1;

        case FL_MOVE:
            findNoteForCursor();
            return 0;

        default:
            return 0;
    }
}

void Grid::moving()
{
    Note* selected = &notes[selectedNote];
    float ex       = Fl::event_x() - this->x();
    selected->beat  = (ex - movingGrabXOffset) / (float)colWidth + colOffset;
    if (selected->beat < 0.0) selected->beat = 0.0;
    if (selected->beat + selected->length > numCols) selected->beat = numCols - selected->length;
    if (snap > 0.0) selected->beat = std::round(selected->beat / snap) * snap;
    float ey      = Fl::event_y() - this->y();
    selected->pitch = (ey - movingGrabYOffset + rowHeight / 2.0) / (float)rowHeight;
    if (selected->pitch < 0)        selected->pitch = 0;
    if (selected->pitch >= numRows) selected->pitch = numRows - 1;
    amOverlapping = overlappingNote() >= 0;
    if (!amOverlapping) lastValidPosition = {selected->pitch, selected->beat};
    if (amOverlapping) window()->cursor(forbiddenCursorImage(), 11, 11);
    else               window()->cursor(FL_CURSOR_HAND);
    redraw();
}

void Grid::resizing()
{
    float minLength = 10.0 / colWidth;
    Note* selected  = &notes[selectedNote];
    float ex        = Fl::event_x() - this->x();
    if (side == LEFT) {
        float endCol   = selected->beat + selected->length;
        selected->beat  = ex / (float)colWidth + colOffset;
        if (snap) selected->beat = std::round(selected->beat / snap) * snap;
        int   neighbour = overlappingNote();
        float min       = neighbour < 0 ? 0.0 : notes[neighbour].beat + notes[neighbour].length;
        if (selected->beat < min) selected->beat = min;
        selected->length = endCol - selected->beat;
        if (selected->length < minLength) { selected->length = minLength; selected->beat = endCol - minLength; }
        redraw();
    } else if (side == RIGHT) {
        selected->length = ex / (float)colWidth + colOffset - selected->beat;
        float endCol     = selected->beat + selected->length;
        if (snap) { endCol = std::round(endCol / snap) * snap; selected->length = endCol - selected->beat; }
        int   neighbour = overlappingNote();
        float max       = neighbour < 0 ? numCols : notes[neighbour].beat;
        if (selected->beat + selected->length > max) selected->length = max - selected->beat;
        if (selected->length < minLength) selected->length = minLength;
        redraw();
    }
}

void Grid::findNoteForCursor()
{
    const int resizeZone = 5;
    float ex = Fl::event_x() - this->x();
    int   ey = Fl::event_y() - this->y();
    int   row = ey / rowHeight;
    int selectedIfResize = 0;
    hoverState = NONE;
    for (const auto [i, n] : std::views::enumerate(notes)) {
        if (n.pitch != row) continue;
        float leftEdge  = (n.beat - colOffset) * colWidth;
        float rightEdge = (n.beat + n.length - colOffset) * colWidth;
        if (leftEdge - ex <= resizeZone && ex - leftEdge <= resizeZone) {
            hoverState = RESIZING; side = LEFT; selectedIfResize = i;
        } else if (rightEdge - ex <= resizeZone && ex - rightEdge <= resizeZone) {
            hoverState = RESIZING; side = RIGHT; selectedIfResize = i;
        } else if (ex >= leftEdge && ex <= rightEdge) {
            hoverState         = MOVING;
            selectedNote       = i;
            movingGrabXOffset  = ex - leftEdge;
            movingGrabYOffset  = ey - n.pitch * rowHeight;
            originalPosition   = {n.pitch, n.beat};
            lastValidPosition  = {n.pitch, n.beat};
            window()->cursor(FL_CURSOR_HAND);
            redraw();
            return;
        }
    }
    if (hoverState == RESIZING) { selectedNote = selectedIfResize; window()->cursor(FL_CURSOR_WE); }
    else window()->cursor(FL_CURSOR_DEFAULT);
    redraw();
}

void Grid::toggleNote()
{
    int   ex  = Fl::event_x() - x();
    int   ey  = Fl::event_y() - y();
    int   row = ey / rowHeight;
    float col = (float)(ex / colWidth) + colOffset;

    int size = notes.size();
    notes.erase(std::remove_if(notes.begin(), notes.end(),
        [=](const Note& n) { return n.pitch == row && n.beat == col; }), notes.end());
    if ((int)notes.size() == size) {
        bool clear = std::none_of(notes.begin(), notes.end(),
            [=](const Note& n) { return n.pitch == row && col < n.beat + n.length && col + 1.0f > n.beat; });
        if (clear)
            notes.push_back({0, row, col, 1.0});
    }
    redraw();
}

int Grid::overlappingNote()
{
    Note  a      = notes[selectedNote];
    float aStart = a.beat, aEnd = a.beat + a.length;
    for (const auto [i, b] : std::views::enumerate(notes)) {
        if (i == selectedNote || b.pitch != a.pitch) continue;
        float bStart      = b.beat, bEnd = b.beat + b.length;
        float firstEnd    = aStart <= bStart ? aEnd : bEnd;
        float secondStart = aStart <= bStart ? bStart : aStart;
        if (firstEnd > secondStart) return i;
    }
    return -1;
}
