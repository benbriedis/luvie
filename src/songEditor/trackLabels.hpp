#ifndef TRACK_LABELS_HPP
#define TRACK_LABELS_HPP

#include "observableSong.hpp"
#include "inlineInput.hpp"
#include <FL/Fl_Group.H>

class TrackContextPopup;
class ParamLaneContextPopup;
class ObservablePattern;

class TrackLabels : public Fl_Group, public ITimelineObserver {
    ObservableSong*         timeline          = nullptr;
    ObservablePattern*      patternObs        = nullptr;
    TrackContextPopup*      contextPopup      = nullptr;
    ParamLaneContextPopup*  paramLanePopup    = nullptr;
    int                  numVisibleRows;
    int                  rowHeight;
    int                  rowOffset         = 0;
    InlineInput          input;
    int                  editingAbsRow = -1;
    int                  editingPatId  = -1;  // >= 0 when editing a pattern name (not track label)
    std::string          originalLabel;

    int  dragStartRow = -1;
    int  dragStartY   = 0;
    bool dragging     = false;
    int  dragRow      = -1;
    int  dropRow      = -1;
    bool isTrackDrag  = false;  // true when dragging an instrument header (whole track)
    int  dragTrackId  = -1;
    int  dropTrackId  = -1;     // track to insert before; -1 = append at end
    bool dropForbidden = false; // current track-drag drop would mix drum/non-drum
    static constexpr int dragThreshold = 4;

    bool trackIsDrum(int trackId) const;
    bool trackDropAllowed() const;  // is the current dragTrackId→dropTrackId move compatible?

    void startInstrumentEdit(int absRow);
    void startPatternEdit(int absRow);
    void cancelEdit();
    void checkDuplicate();

    int rowHFor(int absRow) const;
    int rowYInPanel(int absRow) const;  // cumulative pixel Y from widget top to row absRow
    int absRowAtPanelY(int py) const;   // absRow from pixel Y within widget

public:
    void commitEdit();

public:
    TrackLabels(int x, int y, int w, int numVisibleRows, int rowHeight);
    ~TrackLabels();

    void setTimeline(ObservableSong* tl);
    void setPattern(ObservablePattern* p) { patternObs = p; }
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
