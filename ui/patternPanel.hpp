#ifndef PATTERN_PANEL_HPP
#define PATTERN_PANEL_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Choice.H>
#include <string>

class PatternPanel : public Fl_Group {
    static constexpr Fl_Color bg     = 0x1E293B00;
    static constexpr Fl_Color border = 0x37415100;
    static constexpr Fl_Color text   = FL_WHITE;

    Fl_Box    patternName;
    Fl_Box    rootLabel;
    Fl_Choice rootChoice;
    Fl_Box    chordLabel;
    Fl_Choice chordChoice;

    void draw() override;

public:
    PatternPanel(int x, int y, int w, int h);

    void setPatternName(const std::string& name);
};

#endif
