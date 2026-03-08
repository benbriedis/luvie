#include "grid.hpp"
#include "playhead.hpp"
#include "outerGrid.hpp"
#include <FL/Fl.H>
#include "cell.hpp"
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

    fl_color(bgColor);
    fl_rectf(x(), y(), w(), h());

    fl_color(0xEE888800);  // orange
    for (int i = 0; i < numRows + 1; i++) {
        fl_line(x(), y() + i * rowHeight, x() + numCols * colWidth, y() + i * rowHeight);
    }

    for (int i = 0; i < numCols + 1; i++) {
        int x0 = x() + i * colWidth;
        int y0 = y();
        int x1 = x0;
        int y1 = y() + numRows * rowHeight;
        fl_color(columnColor(i));
        fl_line(x0, y0, x1, y1);
    }

    for (const Note note : notes) {
        int x0    = x() + note.col * colWidth;
        int y0    = y() + note.row * rowHeight;
        int width = note.length * colWidth;
        fl_rectf(x0, y0 + 1, width, rowHeight - 1, 0x5555EE00);
        const int barWidth = 5;
        fl_color(0x1111EE00);
        fl_line_style(FL_SOLID, barWidth);
        fl_line(x0 + barWidth / 2, y0 + 1, x0 + barWidth / 2, y0 + rowHeight - 1);
        fl_line_style(0);
    }

    if (playhead)
        playhead->drawLine(x(), y(), numRows * rowHeight);
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
                int row   = (Fl::event_y() - y()) / rowHeight;
                float col = (float)((Fl::event_x() - x()) / colWidth);
                creationForbidden = false;
                bool wouldRemove = std::any_of(notes.begin(), notes.end(),
                    [=](const Note& n) { return n.row == row && n.col == col; });
                if (!wouldRemove) {
                    creationForbidden = std::any_of(notes.begin(), notes.end(),
                        [=](const Note& n) { return n.row == row && col < n.col + n.length && col + 1.0f > n.col; });
                    if (creationForbidden)
                        window()->cursor(forbiddenCursorImage(), 11, 11);
                }
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
                notes[selectedNote].row = lastValidPosition.row;
                notes[selectedNote].col = lastValidPosition.col;
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
    selected->col  = (ex - movingGrabXOffset) / (float)colWidth;
    if (selected->col < 0.0) selected->col = 0.0;
    if (selected->col + selected->length > numCols) selected->col = numCols - selected->length;
    if (snap > 0.0) selected->col = std::round(selected->col / snap) * snap;
    float ey      = Fl::event_y() - this->y();
    selected->row = (ey - movingGrabYOffset + rowHeight / 2.0) / (float)rowHeight;
    if (selected->row < 0)        selected->row = 0;
    if (selected->row >= numRows) selected->row = numRows - 1;
    amOverlapping = overlappingNote() >= 0;
    if (!amOverlapping) lastValidPosition = {selected->row, selected->col};
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
        float endCol   = selected->col + selected->length;
        selected->col  = ex / (float)colWidth;
        if (snap) selected->col = std::round(selected->col / snap) * snap;
        int   neighbour = overlappingNote();
        float min       = neighbour < 0 ? 0.0 : notes[neighbour].col + notes[neighbour].length;
        if (selected->col < min) selected->col = min;
        selected->length = endCol - selected->col;
        if (selected->length < minLength) { selected->length = minLength; selected->col = endCol - minLength; }
        redraw();
    } else if (side == RIGHT) {
        selected->length = ex / (float)colWidth - selected->col;
        float endCol     = selected->col + selected->length;
        if (snap) { endCol = std::round(endCol / snap) * snap; selected->length = endCol - selected->col; }
        int   neighbour = overlappingNote();
        float max       = neighbour < 0 ? numCols : notes[neighbour].col;
        if (selected->col + selected->length > max) selected->length = max - selected->col;
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
        if (n.row != row) continue;
        float leftEdge  = n.col * colWidth;
        float rightEdge = (n.col + n.length) * colWidth;
        if (leftEdge - ex <= resizeZone && ex - leftEdge <= resizeZone) {
            hoverState = RESIZING; side = LEFT; selectedIfResize = i;
        } else if (rightEdge - ex <= resizeZone && ex - rightEdge <= resizeZone) {
            hoverState = RESIZING; side = RIGHT; selectedIfResize = i;
        } else if (ex >= leftEdge && ex <= rightEdge) {
            hoverState         = MOVING;
            selectedNote       = i;
            movingGrabXOffset  = ex - n.col * colWidth;
            movingGrabYOffset  = ey - n.row * rowHeight;
            originalPosition   = {n.row, n.col};
            lastValidPosition  = {n.row, n.col};
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
    float col = (float)(ex / colWidth);

    int size = notes.size();
    notes.erase(std::remove_if(notes.begin(), notes.end(),
        [=](const Note& n) { return n.row == row && n.col == col; }), notes.end());
    if ((int)notes.size() == size) {
        bool clear = std::none_of(notes.begin(), notes.end(),
            [=](const Note& n) { return n.row == row && col < n.col + n.length && col + 1.0f > n.col; });
        if (clear)
            notes.push_back({0, row, col, 1.0});
    }
    redraw();
}

int Grid::overlappingNote()
{
    Note  a      = notes[selectedNote];
    float aStart = a.col, aEnd = a.col + a.length;
    for (const auto [i, b] : std::views::enumerate(notes)) {
        if (i == selectedNote || b.row != a.row) continue;
        float bStart      = b.col, bEnd = b.col + b.length;
        float firstEnd    = aStart <= bStart ? aEnd : bEnd;
        float secondStart = aStart <= bStart ? bStart : aStart;
        if (firstEnd > secondStart) return i;
    }
    return -1;
}
