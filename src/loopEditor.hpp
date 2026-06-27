#ifndef LOOP_EDITOR_HPP
#define LOOP_EDITOR_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <vector>
#include "observableSong.hpp"
#include "activePatternSet.hpp"
#include "trackContextPopup.hpp"
#include "itransport.hpp"
#include "inlineInput.hpp"
#include "gridScrollPane.hpp"
#include "modern/modernButton.hpp"

// Dark control bar at the bottom of the Loop Editor
class LoopPanel : public Fl_Group, public ITimelineObserver {
    ObservableSong* timeline = nullptr;

    Fl_Box      bpmLabel;
    InlineInput bpmInput;
    Fl_Box      timeSigLabel;
    InlineInput timeSigTopInput;
    Fl_Box      timeSigSlash;
    InlineInput timeSigBotInput;

    void commitBpm();

    void draw() override;

public:
    void resize(int x, int y, int w, int h) override;

    LoopPanel(int x, int y, int w, int h);
    ~LoopPanel();

    void setTimeline(ObservableSong* tl);
    void onTimelineChanged() override;
};

// 2D grid of pattern toggle buttons.
// One axis = tracks/instruments, other axis = pattern slot index (lane index).
// The "Flip" button swaps which axis is columns vs. rows.
class LoopEditor : public Fl_Group, public ITimelineObserver, public IActivePatternObserver {
public:
    static constexpr int panelH = 50;

private:
    static constexpr int cellW        = 150; // fixed pattern-block width
    static constexpr int cellH        = 64;  // fixed pattern-block height (80% of former 80)
    static constexpr int scrollW      = 14;  // scrollbar thickness
    static constexpr int btnGap       = 8;
    static constexpr int padX         = 12;
    static constexpr int padY         = 10;
    static constexpr int trackHeaderH = 24;  // column header height for track names
    static constexpr int trackHeaderW = 80;  // row label width for track names
    static constexpr int patLabelH    = 22;  // column header height for "P1", "P2"
    static constexpr int patLabelW    = 26;  // row label width for "P1", "P2"
    static constexpr int toggleBtnW   = 55;
    static constexpr int toggleBtnH   = 22;

    ObservableSong*    timeline     = nullptr;
    ObservablePattern* patternObs   = nullptr;
    ActivePatternSet*  aps          = nullptr;
    TrackContextPopup* contextPopup = nullptr;
    ITransport*        transport    = nullptr;
    LoopPanel*         panel        = nullptr;
    ModernButton*      axisToggleBtn = nullptr;
    GridScrollPane*    hScroll      = nullptr;
    GridScrollPane*    vScroll      = nullptr;

    bool tracksAsColumns = true;  // true: tracks=cols, lanes=rows; false: tracks=rows, lanes=cols
    int  hoveredCol      = -1;
    int  hoveredRow      = -1;
    int  scrollX         = 0;     // horizontal scroll offset in px
    int  scrollY         = 0;     // vertical scroll offset in px

    static void timerCb(void* data);

    // Computed geometry of the scrollable cell area.
    struct Layout {
        int  vpX, vpY, vpW, vpH;   // cell viewport (excludes header strips + scrollbars)
        int  contentW, contentH;   // total size of all cells
        bool needH, needV;         // whether each scrollbar is required
        int  maxScrollX, maxScrollY;
    };
    Layout computeLayout() const;
    void   updateScrollbars();

    int  gridAreaH()  const { return h() - panelH; }
    int  maxLanes()   const;
    int  numCols()    const;
    int  numRows()    const;
    int  leftStripW() const;
    int  topStripH()  const;

    float beatProgress(int trackIdx, int laneIdx) const;
    void  btnRect(int col, int row, int& bx, int& by, int& bw, int& bh) const;
    bool  cellAt(int mx, int my, int& trackIdx, int& laneIdx) const;

    void draw()   override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

    void positionToggleBtn();

public:
    LoopEditor(int x, int y, int w, int h);
    ~LoopEditor();

    std::function<void()> onToggleChanged;

    void setTimeline(ObservableSong* tl);
    void setPattern(ObservablePattern* p) { patternObs = p; }
    void setActivePatterns(ActivePatternSet* a);
    void setTransport(ITransport* t);
    void setContextPopup(TrackContextPopup* popup);
    bool isEnabled(int trackIdx, int laneIdx) const;
    void onTimelineChanged()       override;
    void onActivePatternsChanged() override;
};

#endif
