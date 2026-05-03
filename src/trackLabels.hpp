#ifndef TRACK_LABELS_HPP
#define TRACK_LABELS_HPP

#include "observableSong.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>

class TrackContextPopup;
class ParamLaneContextPopup;

class TrackLabels : public Fl_Group, public ITimelineObserver {
    ObservableSong*     timeline          = nullptr;
    TrackContextPopup*      contextPopup      = nullptr;
    ParamLaneContextPopup*  paramLanePopup    = nullptr;
    int                  numVisibleRows;
    int                  rowHeight;
    int                  rowOffset         = 0;
    InlineInput          input;
    int                  editingAbsRow = -1;
    std::string          originalLabel;

    int  dragStartRow = -1;
    int  dragStartY   = 0;
    bool dragging     = false;
    int  dragRow      = -1;
    int  dropRow      = -1;
    static constexpr int dragThreshold = 4;

    void startEdit(int absRow);
    void cancelEdit();
    void checkDuplicate();

public:
    void commitEdit();

public:
    TrackLabels(int x, int y, int w, int numVisibleRows, int rowHeight);
    ~TrackLabels();

    void setTimeline(ObservableSong* tl);
    void setContextPopup(TrackContextPopup* p) { contextPopup = p; }
    void setParamLaneContextPopup(ParamLaneContextPopup* p) { paramLanePopup = p; }
    void setRowOffset(int offset);
    void setNumVisibleRows(int n) { numVisibleRows = n; }
    void onTimelineChanged() override { redraw(); }

protected:
    void draw() override;
    int  handle(int event) override;
};

#endif
