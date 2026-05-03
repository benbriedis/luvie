#ifndef NOTE_LABELS_HPP
#define NOTE_LABELS_HPP

#include <FL/Fl_Widget.H>
#include <FL/Fl.H>
#include <functional>
#include <string>
#include <vector>
#include <set>

std::string noteName(int n, int rootPitch, int chordType, bool useSharp);

class NoteLabels : public Fl_Widget {
    int              numRows;
    int              rowHeight;
    int              rootPitch      = 0;
    int              chordType      = 0;
    bool             useSharp       = true;
    int              rowOffset      = 0;
    int              totalTones     = 30;   // in virtual-row units
    std::vector<int> disabledDegrees;           // sorted ascending
    int              groupSize          = 3;    // chordSize + disabledDegrees.size()
    int              chordSize          = 3;
    std::set<int>    occupiedDisabledVPos;      // virtual positions with actual disabled notes

    int         computeTotalTones() const;
    std::string noteForRow(int virtualPos) const;
    void        draw() override;
    int         handle(int event) override;

public:
    std::function<void()> onRightClick;

    NoteLabels(int x, int y, int w, int numRows, int rowHeight);

    void setParams(int rootPitch, int chordType, bool useSharp);
    void setRowOffset(int offset);
    void setNumRows(int n) { numRows = n; }
    void setDisabledDegrees(const std::vector<int>& dd, int gs, const std::set<int>& occupied);
    int  getTotalTones() const { return totalTones; }
    int  getRowOffset()  const { return rowOffset; }
};

#endif
