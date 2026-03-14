#ifndef NOTE_LABELS_HPP
#define NOTE_LABELS_HPP

#include <FL/Fl_Widget.H>
#include <string>

class NoteLabels : public Fl_Widget {
    int  numRows;
    int  rowHeight;
    int  octave    = 4;
    int  rootPitch = 0;  // index into root choice (A=0)
    int  chordType = 0;  // 0=Major, 1=Minor, 2=Major7, 3=Minor7
    bool useSharp  = true;

    std::string noteForRow(int stepsFromBottom) const;
    void draw() override;

public:
    NoteLabels(int x, int y, int w, int numRows, int rowHeight);
    void setParams(int oct, int root, int chord, bool sharp);
};

#endif
