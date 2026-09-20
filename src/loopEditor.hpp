// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef LOOP_EDITOR_HPP
#define LOOP_EDITOR_HPP

#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <array>
#include <functional>
#include <vector>
#include "controlBar.hpp"
#include "sceneBank.hpp"
#include "timeSigSection.hpp"
#include "observableSong.hpp"
#include "loopManager.hpp"
#include "loopContextPopup.hpp"
#include "itransport.hpp"
#include "inlineInput.hpp"
#include "gridScrollPane.hpp"
#include "modern/modernButton.hpp"
#include "modern/modernSpinner.hpp"

// Dark control bar at the bottom of the Loop Editor: Flip, BPM, Loop Mode's time
// signature, then the scene buttons.
//
// The time signature here is Loop Mode's own, not the song's. A loop's *content* is
// still timed by its pattern's signature and beat definition (that lives in the
// pattern editor's control bar), and the song's markers stay on the song editor's
// rulers. What this one sets is the length of a Loop-Mode bar — which is what a
// scene switch lands on, so it wants to be the user's choice rather than whichever
// marker the song happened to freeze under. See ObservableSong::setLoopTimeSig.
//
// Built on ControlBar like the song and pattern panels, so the row folds when the
// window is too narrow for everything on it rather than running off the edge.
class LoopPanel : public ControlBar, public ITimelineObserver,
                  public IGlobalTempoObserver {
    ObservableSong* timeline  = nullptr;
    ITransport*     transport = nullptr;

    // Widths of the items in the row; see buildLayout().
    static constexpr int flipBtnW  = 55;
    static constexpr int labelW    = 40;
    static constexpr int bpmW      = 70;
    static constexpr int sceneBtnW = 34;
    // A wider gap before the first scene button, setting the run apart from the
    // timing controls it follows.
    static constexpr int sceneGap  = 14;

    ModernButton   flipBtn;
    Fl_Box         bpmLabel;
    ModernSpinner  bpmInput;
    TimeSigSection timeSigSec;
    std::array<ModernButton*, SceneBank::kScenes> sceneBtns{};

    void  commitBpm();
    void  commitTimeSig();
    float bpmBar() const;   // the bar whose tempo this panel shows and edits

    std::vector<PanelRow> buildLayout(int availW) override;
    void draw() override;

public:
    LoopPanel(int x, int y, int w, int h);
    ~LoopPanel();

    // The Flip button was clicked; the editor swaps its axes. The button lives here
    // rather than in the editor so it takes part in the row's folding.
    std::function<void()> onFlip;
    // A scene button was clicked. 0 is Scene S; 1..4 the user's own.
    std::function<void(int)> onSceneChosen;

    // Repaint the scene buttons for this shown/playing pair. They differ only while a
    // switch is armed, and the shown one draws amber for that window.
    void setSceneVisual(int shown, int playing);

    void setTimeline(ObservableSong* tl);
    void setTransport(ITransport* t) { transport = t; syncBpm(); }
    // Re-read the BPM box from the song. Called on every timeline change and on the
    // editor's redraw tick, so the box follows the tempo in force wherever it was
    // set — a song-editor marker, a mode switch, or playback crossing a marker.
    void syncBpm();
    // Re-read the time-signature boxes from the loop register. Public for the same
    // reason syncBpm() is: a mode switch changes which bar they speak for and
    // notifies nobody.
    void syncTimeSig();
    void onTimelineChanged() override;
    // The tempo channel: the register changed under us (the song editor, a mode
    // switch, or this box's own commit). Cheaper and far quieter than riding on
    // onTimelineChanged, which fires for every note edit in the project.
    void onGlobalTempoChanged() override { syncBpm(); syncTimeSig(); }
};

// 2D grid of pattern toggle buttons.
// One axis = tracks/instruments, other axis = pattern slot index (lane index).
// The "Flip" button swaps which axis is columns vs. rows.
class LoopEditor : public Fl_Group, public ITimelineObserver, public ILoopObserver {
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

    // Height the control bar asks for. Not a constant: the bar folds to a second
    // row when the window is too narrow for everything on it, and the grid above
    // has to give up the space. Kept in step by layoutPanel().
    int panelH = 32;

    ObservableSong*    timeline     = nullptr;
    ObservablePattern* patternObs   = nullptr;
    SceneBank*         scenes       = nullptr;
    LoopManager*  loopMgr          = nullptr;
    LoopContextPopup*  contextPopup = nullptr;
    ITransport*        transport    = nullptr;
    LoopPanel*         panel        = nullptr;
    GridScrollPane*    hScroll      = nullptr;
    GridScrollPane*    vScroll      = nullptr;

    // Guards layoutPanel() against the height callback it fires re-entering it.
    bool layingOutPanel = false;

    bool tracksAsColumns = true;  // true: tracks=cols, lanes=rows; false: tracks=rows, lanes=cols
    int  hoveredCol      = -1;
    int  hoveredRow      = -1;
    int  scrollX         = 0;     // horizontal scroll offset in px
    int  scrollY         = 0;     // vertical scroll offset in px

    // Inline rename of an instrument name (double-click the name strip), mirroring
    // the Song Editor's TrackLabels. editingInstrId>=0 while an edit is active.
    InlineInput nameInput;
    int         editingInstrId = -1;
    int         editingAxisIdx = -1;
    std::string originalName;

    // Drag-to-reorder of the instrument axis (loopOrder). dragAxisFrom is the
    // instrument-axis slot being dragged; dropGap is the insertion slot (0..N).
    bool draggingInstr = false;
    int  dragAxisFrom  = -1;
    int  dragTrackId   = -1;
    int  dragStartX    = 0;
    int  dragStartY    = 0;
    int  dropGap       = -1;

    // Drag-to-reorder of a pattern cell (a lane). dragLaneId>=0 marks a pending
    // cell press; patternDragging flips once the move passes the threshold (a
    // plain press+release with no drag toggles the pattern instead of moving it).
    bool patternDragging = false;
    int  dragLaneId      = -1;
    int  dragSrcTrackIdx = -1;
    int  dragSrcLaneIdx  = -1;
    int  dragCellStartX  = 0;
    int  dragCellStartY  = 0;
    int  dropTrackIdx    = -1;   // tracks-vector index of the drop instrument
    int  dropSlot        = -1;   // insertion slot along the lane axis
    bool dropForbidden   = false;

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

    // Whether the grid draws this pattern's block as on, for the scene being shown.
    bool  patternEnabled(int patId) const;
    float beatProgress(int trackIdx, int laneIdx) const;
    void  btnRect(int col, int row, int& bx, int& by, int& bw, int& bh) const;
    bool  cellAt(int mx, int my, int& trackIdx, int& laneIdx, int& col, int& row) const;

    // Map an instrument-axis slot to the real index into tracks via loopOrder.
    int   trackForAxis(int axisIdx) const;
    // Map a pattern-axis slot to the real index into tracks[trackVecIdx].lanes
    // via that track's loopLanes order. Returns -1 if the slot is empty.
    int   laneForSlot(int trackVecIdx, int slot) const;
    // Hit-test the instrument-name strip; returns the axis slot under the cursor.
    bool  instrLabelAt(int mx, int my, int& axisIdx) const;
    // Screen rect of the instrument name for an axis slot (where the input sits).
    bool  instrLabelRect(int axisIdx, int& lx, int& ly, int& lw, int& lh) const;
    // Inline-rename lifecycle for the instrument under the given axis slot.
    void  startInstrumentEdit(int axisIdx);
    void  commitInstrumentEdit();
    void  cancelInstrumentEdit();
    void  checkDuplicateName();
    // Insertion slot (0..numTracks) for the current drag position.
    int   computeDropGap(int mx, int my) const;
    // Toggle a pattern cell's active state (deferred from press to release).
    void  togglePattern(int trackIdx, int laneIdx);
    // Anchor that puts a pattern's beat 0 on the next bar line when switched on now.
    float switchOnAnchor(int patId) const;
    // Select a pattern cell. The selection is the app-wide selected lane,
    // shared with the Song Editor rather than kept privately here.
    void  selectCell(int trackIdx, int laneIdx);
    // Resolve the drop instrument + lane slot for the current pattern drag.
    void  computePatternDrop(int mx, int my);

    void draw()   override;
    int  handle(int event) override;
    void resize(int x, int y, int w, int h) override;

    // Re-fit the control bar to the current width and give the grid what is left.
    void layoutPanel();
    // Swap which axis holds the tracks; wired to the panel's Flip button.
    void flipAxes();

public:
    LoopEditor(int x, int y, int w, int h);
    ~LoopEditor();

    std::function<void()> onToggleChanged;
    // The user picked a scene. The editor does not land the switch itself: whether it
    // takes effect now or on the next bar line depends on the mode and the transport,
    // which the app owns. It has already been made the *shown* scene by the time this
    // fires, so the grid is showing where we are going.
    std::function<void(int)> onSceneChosen;
    // A block was toggled on a user scene. The scene's stored set has already
    // changed; the app decides whether that scene is the one sounding and so whether
    // LoopManager needs to follow.
    std::function<void(int patId)> onSceneEdited;

    void setTimeline(ObservableSong* tl);
    void setPattern(ObservablePattern* p) { patternObs = p; }
    void setSceneBank(SceneBank* s);
    void setLoopManager(LoopManager* a);
    // Repaint the scene buttons from the bank. Called by the app when the playing
    // scene changes under us — an armed switch landing, or a mode change.
    void refreshSceneVisual();
    void setTransport(ITransport* t);
    void setContextPopup(LoopContextPopup* popup);
    // Re-read the panel's BPM box. The Loop-Mode tempo freeze deliberately notifies
    // nobody (see ObservableSong::holdTempo), but it does change which bar's
    // tempo the panel speaks for, so the mode switch calls this.
    void refreshPanel();
    bool isEnabled(int trackIdx, int laneIdx) const;
    void onTimelineChanged()       override;
    void onLoopsChanged() override;
};

#endif
