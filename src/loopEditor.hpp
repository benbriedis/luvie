#ifndef LOOP_EDITOR_HPP
#define LOOP_EDITOR_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <vector>
#include "observableSong.hpp"
#include "activePatternSet.hpp"
#include "loopContextPopup.hpp"
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
    LoopContextPopup*  contextPopup = nullptr;
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

    // Drag-to-reorder of the instrument axis (loopOrder). dragAxisFrom is the
    // instrument-axis slot being dragged; dropGap is the insertion slot (0..N).
    bool draggingInstr = false;
    int  dragAxisFrom  = -1;
    int  dragTrackId   = -1;
    int  dragStartX    = 0;
    int  dragStartY    = 0;
    int  dropGap       = -1;

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
    bool  cellAt(int mx, int my, int& trackIdx, int& laneIdx, int& col, int& row) const;

    // Map an instrument-axis slot to the real index into tracks via loopOrder.
    int   trackForAxis(int axisIdx) const;
    // Hit-test the instrument-name strip; returns the axis slot under the cursor.
    bool  instrLabelAt(int mx, int my, int& axisIdx) const;
    // Insertion slot (0..numTracks) for the current drag position.
    int   computeDropGap(int mx, int my) const;

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
    void setContextPopup(LoopContextPopup* popup);
    bool isEnabled(int trackIdx, int laneIdx) const;
    void onTimelineChanged()       override;
    void onActivePatternsChanged() override;
};

#endif
