#ifndef LOOP_EDITOR_HPP
#define LOOP_EDITOR_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <vector>
#include "observableTimeline.hpp"
#include "trackContextPopup.hpp"
#include "itransport.hpp"
#include "inlineInput.hpp"

// Dark control bar at the bottom of the Loop Editor
class LoopPanel : public Fl_Group, public ITimelineObserver {
    ObservableTimeline* timeline = nullptr;

    Fl_Box      bpmLabel;
    InlineInput bpmInput;
    Fl_Box      timeSigLabel;
    InlineInput timeSigTopInput;
    Fl_Box      timeSigSlash;
    InlineInput timeSigBotInput;

    void draw() override;

public:
    void resize(int x, int y, int w, int h) override;

    LoopPanel(int x, int y, int w, int h);
    ~LoopPanel();

    void setTimeline(ObservableTimeline* tl);
    void onTimelineChanged() override;
};

// Grid of large pattern toggle buttons
class LoopEditor : public Fl_Group, public ITimelineObserver {
public:
    static constexpr int panelH = 50;

private:
    static constexpr int btnH    = 80;
    static constexpr int btnGap  = 10;
    static constexpr int padX    = 12;
    static constexpr int padY    = 12;
    static constexpr int cols    = 4;

    ObservableTimeline* timeline     = nullptr;
    TrackContextPopup*  contextPopup = nullptr;
    ITransport*         transport    = nullptr;
    LoopPanel*          panel        = nullptr;

    int hoveredIdx = -1;
    std::vector<bool> toggled;

    static void timerCb(void* data);

    int   gridAreaH()  const { return h() - panelH; }
    float beatProgress(int trackIdx) const;  // 0.0–1.0 loop position for track's pattern
    void  btnRect(int idx, int& bx, int& by, int& bw, int& bh) const;
    int   trackAt(int mx, int my) const;

    void draw()   override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

public:
    LoopEditor(int x, int y, int w, int h);
    ~LoopEditor();

    void setTimeline(ObservableTimeline* tl);
    void setTransport(ITransport* t);
    void setContextPopup(TrackContextPopup* popup);
    bool isEnabled(int trackIdx) const;
    void onTimelineChanged() override;
};

#endif
