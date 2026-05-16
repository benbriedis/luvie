#include "trackControls.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

static constexpr Fl_Color colNormal  = 0x1F293700;
static constexpr Fl_Color colParam   = 0x1A2B3A00;
static constexpr Fl_Color colEmpty   = 0x17202A00;
static constexpr Fl_Color colBorder  = 0x37415100;
static constexpr Fl_Color colBtnOff  = 0x29354800;
static constexpr Fl_Color colSoloOn  = 0x22C55E00;
static constexpr Fl_Color colMuteOn  = 0xEF444400;
static constexpr Fl_Color colTextOn  = FL_WHITE;
static constexpr Fl_Color colTextOff = 0x64748B00;

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

void TrackControls::draw()
{
    int btnH = rowHeight / 2;
    int pad  = 1;

    for (int i = rowOffset; i < rowOffset + numVisibleRows; i++) {
        int ry = y() + (i - rowOffset) * rowHeight;

        bool isTrack = false;
        bool solo    = false;
        bool mute    = false;
        Fl_Color rowBg = colEmpty;

        if (timeline) {
            const auto& tl = timeline->get();
            const auto& ro = tl.rowOrder;
            if (i >= 0 && i < (int)ro.size()) {
                const auto& ref = ro[i];
                if (ref.isTrack) {
                    rowBg = colNormal;
                    for (const auto& t : tl.tracks) {
                        if (t.id != ref.id) continue;
                        solo    = t.solo;
                        mute    = t.mute;
                        isTrack = true;
                        break;
                    }
                } else {
                    rowBg = colParam;
                }
            }
        }

        fl_color(rowBg);
        fl_rectf(x(), ry, w(), rowHeight);
        fl_color(colBorder);
        fl_line(x(), ry + rowHeight - 1, x() + w() - 1, ry + rowHeight - 1);

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
        int row    = localY / rowHeight + rowOffset;
        const auto& tl = timeline->get();
        const auto& ro = tl.rowOrder;
        if (row < 0 || row >= (int)ro.size() || !ro[row].isTrack)
            return 1;

        int trackId = ro[row].id;
        bool isSolo = (localY % rowHeight) < rowHeight / 2;

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
