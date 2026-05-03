#ifndef PATTERN_PARAM_GRID_HPP
#define PATTERN_PARAM_GRID_HPP

#include "paramLaneTypes.hpp"
#include "observablePattern.hpp"
#include "paramDotPopup.hpp"
#include <FL/Fl_Widget.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <functional>
#include <vector>
#include <string>

inline constexpr int kParamRowH    = 45;
inline constexpr int kMaxVisParams = 2;
inline constexpr int kParamAreaH   = kMaxVisParams * kParamRowH;

// ── Left column: type labels for each visible param lane ─────────────────────

class PatternParamLabels : public Fl_Widget {
    ObservablePattern* timeline   = nullptr;
    int                 patternId  = -1;
    int                 laneOffset = 0;

    void draw()           override;
    int  handle(int event) override;

public:
    std::function<void(int laneId)> onRightClick;

    PatternParamLabels(int x, int y, int w)
        : Fl_Widget(x, y, w, kParamAreaH) {}

    void setTimeline(ObservablePattern* tl, int patId) {
        timeline  = tl;
        patternId = patId;
        redraw();
    }
    void setLaneOffset(int off) { laneOffset = off; redraw(); }
};

// ── Rubberband editing grid ───────────────────────────────────────────────────

class PatternParamGrid : public Fl_Widget {
    ObservablePattern* timeline   = nullptr;
    int                 patternId  = -1;
    int                 colOffset_ = 0;
    int                 numCols_   = 8;
    int                 colWidth_;
    float               snap_;
    int                 laneOffset = 0;
    ParamDotPopup*      dotPopup   = nullptr;

    std::vector<ParamLaneLocal> localLanes;
    ParamState                  paramState;

    void rebuildLanes();
    void drawParamRow(int laneIdx, int rowY, int gridRight);
    int  findParamPointAtCursor(int laneIdx, int rowY) const;
    int  findPrecedingDotIdx(int laneIdx) const;
    bool canPlaceDot(int laneIdx, float beat, int excludeId = -1) const;

    void draw()   override;
    int  handle(int event) override;

public:
    PatternParamGrid(int x, int y, int w, int colWidth, float snap)
        : Fl_Widget(x, y, w, kParamAreaH), colWidth_(colWidth), snap_(snap) {}

    void setTimeline(ObservablePattern* tl, int patId);
    void update(ObservablePattern* tl, int patId);   // respects active drag
    void setColOffset(int off) { colOffset_ = off; redraw(); }
    void setNumCols(int n)     { numCols_   = n;   redraw(); }
    void setLaneOffset(int off){ laneOffset = off; rebuildLanes(); redraw(); }
    void setParamDotPopup(ParamDotPopup* p) { dotPopup = p; }
    int  numLanes() const { return (int)localLanes.size(); }
};

#endif
