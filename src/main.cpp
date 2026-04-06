#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <string>
#include "appWindow.hpp"
#include "songEditor.hpp"
#include "patternEditor.hpp"
#include "popup.hpp"
#include "modernTabs.hpp"
#include "transport.hpp"
#include "simpleTransport.hpp"
#include "jackTransport.hpp"
#include "markerPopup.hpp"
#include "markerRuler.hpp"
#include "observableTimeline.hpp"
#include "patternPanel.hpp"
#include "trackContextPopup.hpp"
#include "noteLabels.hpp"
#include "drumPatternEditor.hpp"
#include "loopEditor.hpp"

int main(int argc, char **argv) {
    bool verbose = false;
    int  fltk_argc = 1;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--verbose" || arg == "-v")
            verbose = true;
        else
            argv[fltk_argc++] = argv[i];
    }
    argc = fltk_argc;
    const int tabBarH      = 35;
    const int bottomH      = 50;
    const int markerRulerH = 18;
    const int winW         = 920;
    const int winH         = tabBarH + 2 * markerRulerH + Editor::rulerH + 10 * 45 + 20 + bottomH;

    AppWindow window(winW, winH);
    window.color(bgColor);
    window.end();

    Popup popup1{};
    Popup popup2{};

    MarkerPopup tempoPopup(MarkerPopup::TEMPO);
    MarkerPopup timeSigPopup(MarkerPopup::TIME_SIG);

    const int tabsH = winH - bottomH;

    ModernTabs tabs(0, 0, winW, tabsH);
    window.add(tabs);

    Fl_Group tab1(0, tabBarH, winW, tabsH - tabBarH, "Song Editor");
    tab1.color(bgColor);
    tabs.add(tab1);

    const int numPatternBeats = 8;

    ObservableTimeline songTimeline(120.0f, 4, 4);
    for (int i = 1; i <= 8; i++) {
        int patId = songTimeline.createPattern(numPatternBeats);
        songTimeline.addTrack("Pattern " + std::to_string(i), patId);
    }

    MarkerRuler timeSigRuler(0, tabBarH, winW, markerRulerH,
                              80, 60, MarkerRuler::TIME_SIG, &songTimeline, &timeSigPopup);
    tab1.add(timeSigRuler);

    MarkerRuler tempoRuler(0, tabBarH + markerRulerH, winW, markerRulerH,
                            80, 60, MarkerRuler::TEMPO, &songTimeline, &tempoPopup);
    tab1.add(tempoRuler);

    TrackContextPopup trackContextPopup;

    std::vector<Note> patterns(0);
    SongEditor og2(0, tabBarH + 2 * markerRulerH, winW, patterns, 10, 80, 45, 60, 0.25, popup2);
    tab1.add(og2);

    const int panelH = 50;

    Fl_Group tabLoop(0, tabBarH, winW, tabsH - tabBarH, "Loop Editor");
    tabLoop.color(bgColor);
    tabs.add(tabLoop);

    LoopEditor loopEd(0, tabBarH, winW, tabsH - tabBarH);
    tabLoop.add(loopEd);

    Fl_Group tab2(0, tabBarH, winW, tabsH - tabBarH, "Pattern Editor");
    tab2.color(bgColor);
    tabs.add(tab2);

    const int rowHeight     = 30;
    const int numRows       = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / rowHeight;
    const int drumRowHeight = 20;
    const int drumNumRows   = (tabsH - tabBarH - Editor::rulerH - panelH - Editor::hScrollH) / drumRowHeight;

    std::vector<Note> notes(0);
    PatternEditor og1(0, tabBarH, winW, notes, numRows, numPatternBeats, rowHeight, 40, 0.25, popup1);
    tab2.add(og1);

    DrumPatternEditor drumEd(0, tabBarH, winW, drumNumRows, numPatternBeats, drumRowHeight, 40, 0.25f, popup1);
    tab2.add(drumEd);
    drumEd.hide();

    PatternPanel patternPanel(0, tabsH - panelH, winW, panelH);
    patternPanel.setTimeline(&songTimeline);
    tab2.add(patternPanel);

    JackTransport  jackTransport;
    SimpleTransport simpleTransport;
    bool useJack = jackTransport.open();
    ITransport* transport = useJack
        ? static_cast<ITransport*>(&jackTransport)
        : static_cast<ITransport*>(&simpleTransport);
    if (useJack) {
        jackTransport.setTimeline(&songTimeline);
    } else {
        simpleTransport.setTimeline(&songTimeline);
    }
    Transport bottomPane(0, tabsH, winW, bottomH, transport, &songTimeline);
    window.add(bottomPane);

    if (useJack) {
        jackTransport.onTransportEvent = [&]() {
            Fl::awake([](void* data) {
                static_cast<Transport*>(data)->syncPlayState();
            }, &bottomPane);
        };
    }

    og2.setTransport(transport, &songTimeline);
    if (verbose) {
        og2.setVerbose(true);
        og2.setPitchName([&patternPanel](int pitch) {
            return noteName(pitch, patternPanel.rootPitch(),
                                   patternPanel.chordType(),
                                   patternPanel.isSharp());
        });
    }
    og2.onRulerOffsetChanged = [&](int off, int clipLeft) {
        timeSigRuler.setOffsetX(off);
        timeSigRuler.setClipLeft(clipLeft);
        tempoRuler.setOffsetX(off);
        tempoRuler.setClipLeft(clipLeft);
    };
    og2.setContextPopup(&trackContextPopup);
    loopEd.setTimeline(&songTimeline);
    loopEd.setContextPopup(&trackContextPopup);
    og2.onEndReached = [&bottomPane]() { bottomPane.notifyEndReached(); };
    og2.onSeek       = [&bottomPane]() { bottomPane.notifySeek(); };
    og2.onPatternDoubleClick = [&](int trackIndex) {
        songTimeline.selectTrack(trackIndex);
        tabs.value(&tab2);
        tabs.redraw();
    };

    og1.setPatternPlayhead(transport, &songTimeline, 0);
    drumEd.setPatternPlayhead(transport, &songTimeline, 0);

    // Switch between standard and drum editors based on selected track's pattern type
    struct EditorSwitcher : ITimelineObserver {
        PatternEditor*      stdEd;
        DrumPatternEditor*  drumEd;
        ObservableTimeline* tl;
        void onTimelineChanged() override {
            const auto& data = tl->get();
            int sel = data.selectedTrackIndex;
            bool isDrum = false;
            if (sel >= 0 && sel < (int)data.tracks.size()) {
                int patId = data.tracks[sel].patternId;
                for (const auto& p : data.patterns)
                    if (p.id == patId) { isDrum = (p.type == PatternType::DRUM); break; }
            }
            if (isDrum) { stdEd->hide(); drumEd->show(); }
            else        { stdEd->show(); drumEd->hide(); }
        }
    } editorSwitcher;
    editorSwitcher.stdEd  = &og1;
    editorSwitcher.drumEd = &drumEd;
    editorSwitcher.tl     = &songTimeline;
    songTimeline.addObserver(&editorSwitcher);

    songTimeline.selectTrack(0);  // triggers PatternEditor to load track 0's pattern

    auto syncNoteLabels = [&]() {
        og1.setNoteParams(patternPanel.rootPitch(), patternPanel.chordType(), patternPanel.isSharp());
        if (useJack)
            jackTransport.setNoteParams(patternPanel.rootPitch(), patternPanel.chordType());
    };
    patternPanel.onParamsChanged = syncNoteLabels;
    syncNoteLabels();

    window.add(popup1);              window.registerPopup(&popup1);
    window.add(popup2);              window.registerPopup(&popup2);
    window.add(tempoPopup);          window.registerPopup(&tempoPopup);
    window.add(timeSigPopup);        window.registerPopup(&timeSigPopup);
    window.add(trackContextPopup);   window.registerPopup(&trackContextPopup);

    // Resizable chain: window → tabs → each tab → editor
    tab1.resizable(og2);
    tabLoop.resizable(loopEd);
    tab2.resizable(og1);
    window.resizable(tabs);

    // Minimum size: at least 5 pattern columns wide and 5 pattern rows tall
    const int minW = 14 + 36 + 5 * 40;  // scrollbarW + labelsW + 5 cols
    const int minH = tabBarH + Editor::rulerH + 5 * rowHeight + Editor::hScrollH + panelH + bottomH;
    window.size_range(minW, minH);

    window.show(argc, argv);
    return Fl::run();
}
