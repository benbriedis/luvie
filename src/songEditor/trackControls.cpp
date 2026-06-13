#include "trackControls.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

static constexpr Fl_Color colNormal      = 0x1F293700;
static constexpr Fl_Color colParam       = 0x1A2B3A00;
static constexpr Fl_Color colEmpty       = 0x17202A00;
static constexpr Fl_Color colBorder      = 0x37415100;
static constexpr Fl_Color colTrackDiv    = 0x64748B00;
static constexpr Fl_Color colInstrHeader = 0x64748B00;
static constexpr Fl_Color colBtnOff     = 0x29354800;
static constexpr Fl_Color colSoloOn     = 0x22C55E00;
static constexpr Fl_Color colMuteOn     = 0xEF444400;
static constexpr Fl_Color colTextOn     = FL_WHITE;
static constexpr Fl_Color colTextOff    = 0x64748B00;
static constexpr int      instrNameRowH = 24;

TrackControls::TrackControls(int x, int y, int w, int numVisibleRows, int rowHeight)
    : Fl_Widget(x, y, w, numVisibleRows * rowHeight),
      numVisibleRows(numVisibleRows),
      rowHeight(rowHeight)
{}

TrackControls::~TrackControls()
{
    swapObserver(timeline, nullptr, this);
}

void TrackControls::setTimeline(ObservableSong* tl)
{
    swapObserver(timeline, tl, this);
    redraw();
}

void TrackControls::setRowOffset(int offset)
{
    rowOffset = offset;
    redraw();
}

static int rowHFor(const Timeline* tl, int absRow, int rowHeight) {
    if (!tl) return rowHeight;
    const auto& ro = tl->rowOrder;
    if (absRow >= 0 && absRow < (int)ro.size() && ro[absRow].kind == RowKind::Header)
        return instrNameRowH;
    return rowHeight;
}

static int rowYInPanel(const Timeline* tl, int rowOffset, int absRow, int rowHeight) {
    int py = 0;
    for (int i = rowOffset; i < absRow; i++)
        py += rowHFor(tl, i, rowHeight);
    return py;
}

static int absRowAtPanelY(const Timeline* tl, int rowOffset, int numVisibleRows, int py, int rowHeight) {
    int cumY = 0;
    for (int i = rowOffset; i < rowOffset + numVisibleRows; i++) {
        int rh = rowHFor(tl, i, rowHeight);
        if (py < cumY + rh) return i;
        cumY += rh;
    }
    return rowOffset + numVisibleRows;
}

void TrackControls::draw()
{
    int btnH = rowHeight / 2;
    int pad  = 1;
    const Timeline* tl_ = timeline ? &timeline->get() : nullptr;

    for (int i = rowOffset; i < rowOffset + numVisibleRows; i++) {
        int rh = rowHFor(tl_, i, rowHeight);
        int ry = y() + rowYInPanel(tl_, rowOffset, i, rowHeight);

        bool isInstrHeader = false;
        bool isTrack       = false;
        bool isFirstLane   = false;
        bool solo          = false;
        bool mute          = false;
        Fl_Color rowBg     = colEmpty;

        if (timeline) {
            const auto& tl = timeline->get();
            const auto& ro = tl.rowOrder;
            if (i >= 0 && i < (int)ro.size()) {
                const auto& ref = ro[i];
                if (ref.kind == RowKind::Header) {
                    rowBg        = colInstrHeader;
                    isInstrHeader = true;
                } else if (ref.kind == RowKind::Lane) {
                    rowBg = colNormal;
                    int tid = timeline->trackIdForLaneId(ref.id);
                    for (const auto& t : tl.tracks) {
                        if (t.id != tid) continue;
                        isFirstLane = (!t.lanes.empty() && t.lanes[0].id == ref.id);
                        solo        = t.solo;
                        mute        = t.mute;
                        isTrack     = true;
                        break;
                    }
                } else {
                    rowBg = colParam;
                }
            }
        }

        fl_color(rowBg);
        fl_rectf(x(), ry, w(), rh);
        fl_color(colBorder);
        fl_line(x(), ry + rh - 1, x() + w() - 1, ry + rh - 1);

        // Draw track divider at the top of instrument header rows or first-lane of single/stacked tracks
        if (ry > y()) {
            bool drawDiv = isInstrHeader;
            if (!drawDiv && isFirstLane && isTrack) {
                // Only for single-lane/stacked — multi-lane unstacked has the header row
                if (timeline) {
                    const auto& tl = timeline->get();
                    const auto& ro = tl.rowOrder;
                    if (i >= 0 && i < (int)ro.size() && ro[i].kind == RowKind::Lane) {
                        int tIdx = timeline->trackIndexForLaneId(ro[i].id);
                        if (tIdx >= 0) {
                            const auto& t = tl.tracks[tIdx];
                            drawDiv = t.stackedLanes;
                        }
                    }
                }
            }
            if (drawDiv) {
                fl_color(colTrackDiv);
                fl_rectf(x(), ry - 1, w(), 2);
            }
        }

        if (!isTrack) continue;

        int bx = x() + pad;
        int bw = w() - 2 * pad;

        // S (solo) button — top half
        fl_color(solo ? colSoloOn : colBtnOff);
        fl_rectf(bx, ry + pad, bw, btnH - 2 * pad);
        fl_font(FL_HELVETICA_BOLD, 9);
        fl_color(solo ? colTextOn : colTextOff);
        fl_draw("S", bx, ry + pad, bw, btnH - 2 * pad, FL_ALIGN_CENTER);

        // M (mute) button — bottom half
        fl_color(mute ? colMuteOn : colBtnOff);
        fl_rectf(bx, ry + btnH + pad, bw, btnH - 2 * pad);
        fl_color(mute ? colTextOn : colTextOff);
        fl_draw("M", bx, ry + btnH + pad, bw, btnH - 2 * pad, FL_ALIGN_CENTER);
    }
}

int TrackControls::handle(int event)
{
    if (event == FL_ENTER) {
        window()->cursor(FL_CURSOR_DEFAULT);
        return 1;
    }
    if (event == FL_LEAVE)
        return 1;

    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE && timeline) {
        int localY = Fl::event_y() - y();
        const Timeline* tl_ = &timeline->get();
        int row    = absRowAtPanelY(tl_, rowOffset, numVisibleRows, localY, rowHeight);
        const auto& tl = timeline->get();
        const auto& ro = tl.rowOrder;
        if (row < 0 || row >= (int)ro.size() || ro[row].kind != RowKind::Lane)
            return 1;

        int rowPY   = rowYInPanel(tl_, rowOffset, row, rowHeight);
        int rowH_   = rowHFor(tl_, row, rowHeight);
        int trackId = timeline->trackIdForLaneId(ro[row].id);
        bool isSolo = (localY - rowPY) < rowH_ / 2;

        for (const auto& t : tl.tracks) {
            if (t.id != trackId) continue;
            if (isSolo)
                timeline->setTrackSolo(trackId, !t.solo);
            else
                timeline->setTrackMute(trackId, !t.mute);
            break;
        }
        return 1;
    }

    return 0;
}
