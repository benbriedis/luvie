#ifndef GRID_HPP
#define GRID_HPP

#include "popup.hpp"
#include "timeline.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Button.H>
#include <functional>

class Playhead;

enum SelectionState { NONE, MOVING, RESIZING };
enum Side { LEFT, RIGHT };

typedef struct { int row; float col; } Point;

class Grid : public Fl_Box {
public:
    int numRows, numCols;
    int rowHeight, colWidth;

protected:
    float snap;
    Popup& popup;
    std::vector<Note> notes;

    int selectedNote = 0;
    SelectionState hoverState = NONE;
    Side side = LEFT;
    float movingGrabXOffset = 0, movingGrabYOffset = 0;
    bool amOverlapping = false, creationForbidden = false;
    Point originalPosition = {0, 0.0f}, lastValidPosition = {0, 0.0f};

    Playhead* playhead = nullptr;

    void draw() override;
    int  handle(int event) override;
    void findNoteForCursor();
    int  overlappingNote();
    void moving();
    virtual void resizing();
    void clampSelection() { if (selectedNote >= (int)notes.size()) { selectedNote = 0; hoverState = NONE; } }

    // Virtual extension hooks
    virtual Fl_Color columnColor(int col) const { (void)col; return 0x00EE0000; }
    virtual std::function<void()> makeDeleteCallback() { return nullptr; }
    virtual void onBeginDrag() {}
    virtual void onCommitDrag() {}
    virtual void toggleNote();

public:
    Grid(std::vector<Note> notes, int numRows, int numCols,
           int rowHeight, int colWidth, float snap, Popup& popup);

    void setPlayhead(Playhead* p) { playhead = p; }
};

#endif
