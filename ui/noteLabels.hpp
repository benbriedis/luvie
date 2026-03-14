#ifndef NOTE_LABELS_HPP
#define NOTE_LABELS_HPP

#include <FL/Fl_Widget.H>
#include <FL/Fl.H>
#include <functional>
#include <string>

class NoteLabels : public Fl_Widget {
    int  numRows;
    int  rowHeight;
    int  rootPitch  = 0;
    int  chordType  = 0;
    bool useSharp   = true;
    int  rowOffset  = 0;
    int  totalTones = 30;

    int         computeTotalTones() const;
    std::string noteForRow(int n) const;
    void        draw() override;
    int         handle(int event) override;

public:
    std::function<void(int delta)> onPageChange;

    NoteLabels(int x, int y, int w, int numRows, int rowHeight);

    void setParams(int rootPitch, int chordType, bool useSharp);
    void setRowOffset(int offset);
    int  getTotalTones() const { return totalTones; }
    int  getRowOffset()  const { return rowOffset; }
};

#endif
