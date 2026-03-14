#include "patternPanel.hpp"
#include <FL/fl_draw.H>

static constexpr int pad    = 10;
static constexpr int ctrlH  = 24;
static constexpr int labelW = 50;
static constexpr int nameW  = 150;
static constexpr int choiceW = 130;

PatternPanel::PatternPanel(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h),
      patternName(x + pad,                                    y + (h - ctrlH) / 2, nameW,   ctrlH),
      rootLabel  (x + pad + nameW + pad,                      y + (h - ctrlH) / 2, labelW,  ctrlH, "Root note"),
      rootChoice (x + pad + nameW + pad + labelW,             y + (h - ctrlH) / 2, choiceW, ctrlH),
      chordLabel (x + pad + nameW + pad + labelW + choiceW + pad, y + (h - ctrlH) / 2, labelW,  ctrlH, "Chord"),
      chordChoice(x + pad + nameW + pad + labelW + choiceW + pad + labelW, y + (h - ctrlH) / 2, choiceW, ctrlH)
{
    box(FL_NO_BOX);

    patternName.box(FL_FLAT_BOX);
    patternName.color(0x37415100);
    patternName.labelcolor(text);
    patternName.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    rootLabel.box(FL_NO_BOX);
    rootLabel.labelcolor(text);
    rootLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const char* note : {"A", "A#/Bb", "B", "C", "C#/Db",
                              "D", "D#/Eb", "E", "F", "F#/Gb",
                              "G", "G#/Ab"})
        rootChoice.add(note);
    rootChoice.value(0);

    chordLabel.box(FL_NO_BOX);
    chordLabel.labelcolor(text);
    chordLabel.align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

    for (const char* chord : {"Major", "Minor", "Major 7", "Minor 7"})
        chordChoice.add(chord);
    chordChoice.value(0);

    end();
}

void PatternPanel::setPatternName(const std::string& name)
{
    patternName.copy_label(name.c_str());
    redraw();
}

void PatternPanel::draw()
{
    // Border along the top edge
    fl_color(border);
    fl_rectf(x(), y(), w(), 1);

    // Background
    fl_color(bg);
    fl_rectf(x(), y() + 1, w(), h() - 1);

    draw_children();
}
