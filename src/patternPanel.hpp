#ifndef PATTERN_PANEL_HPP
#define PATTERN_PANEL_HPP

#include "observablePattern.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include "panelStyle.hpp"
#include <string>
#include <vector>

class RecenterButton : public Fl_Widget {
    bool hovered = false;

    void draw() override {
        Fl_Color bg = hovered ? panelBgHover : panelBg;
        fl_color(bg);
        fl_rectf(x(), y(), w(), h());
        fl_color(panelCtrlBorder);
        fl_rect(x(), y(), w(), h());
        int cx = x() + w() / 2;
        int cy = y() + h() / 2;
        int r  = std::min(w(), h()) / 4;
        int tk = 3;
        fl_color(0x94A3B800);
        fl_line_style(FL_SOLID, 1);
        fl_arc(cx - r, cy - r, 2 * r, 2 * r, 0, 360);
        fl_line(cx,         cy - r - 1,     cx,         cy - r - 1 - tk);
        fl_line(cx,         cy + r + 1,     cx,         cy + r + 1 + tk);
        fl_line(cx - r - 1, cy,             cx - r - 1 - tk, cy);
        fl_line(cx + r + 1, cy,             cx + r + 1 + tk, cy);
        fl_line_style(0);
    }

    int handle(int event) override {
        switch (event) {
        case FL_ENTER: hovered = true;  redraw(); return 1;
        case FL_LEAVE: hovered = false; redraw(); return 1;
        case FL_PUSH:    return 1;
        case FL_RELEASE:
            if (Fl::event_inside(this)) do_callback();
            return 1;
        default: return Fl_Widget::handle(event);
        }
    }

public:
    RecenterButton(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}
};

class PatternPanel : public Fl_Group, public ITimelineObserver {

    ObservablePattern* pattern      = nullptr;
    int                 editingTrackId = -1;
    std::string         originalLabel;
    bool                useSharp      = true;
    std::vector<std::string> stdInstrumentNames_;
    std::vector<std::string> drumInstrumentNames_;

    RecenterButton recentreBtn;
    Fl_Box         patternName;
    Fl_Box         baseLabel;
    ModernButton   sharpFlatBtn;
    ModernChoice   rootChoice;
    Fl_Box         chordLabel;
    ModernChoice   chordChoice;
    Fl_Box         timeSigLabel;
    Fl_Value_Input timeSigNum;
    Fl_Box         timeSigSlash;
    ModernChoice   timeSigDen;
    Fl_Box         barsLabel;
    Fl_Value_Input barsInput;
    ModernChoice   outChoice;
    InlineInput    input;

    void startEdit();
    void cancelEdit();
    void checkDuplicate();
    void updateRootChoiceLabels(int preserveIndex);
    void refreshOutChoice();
    void refreshTimeSig();
    void refreshBars();

    void draw() override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

public:
    PatternPanel(int x, int y, int w, int h);
    ~PatternPanel();

    std::function<void()> onParamsChanged;
    std::function<void()> onFocus;

    void commitEdit();

    int  rootPitch() const { return rootChoice.value(); }
    int  chordType() const { return chordChoice.value(); }
    bool isSharp()   const { return useSharp; }

    // Set chord/root/sharp without triggering onParamsChanged.
    void setParams(int root, int chord, bool sharp);

    // Update the available instrument lists shown in the Out dropdown.
    void setInstruments(const std::vector<std::string>& stdNames,
                        const std::vector<std::string>& drumNames);

    void setPattern(ObservablePattern* tl);
    void setDrumMode(bool drum);
    void onTimelineChanged() override;
};

#endif
