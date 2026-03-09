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
#include "markerPopup.hpp"
#include "markerRuler.hpp"
#include "observableTimeline.hpp"

int main(int argc, char **argv) {
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

    Fl_Group tab1(0, tabBarH, winW, tabsH - tabBarH, "Pattern Editor");
    tab1.color(bgColor);
    tabs.add(tab1);

    std::vector<Note> notes(0);
    PatternEditor og1(0, tabBarH, notes, 10, 8, 30, 40, 0.25, popup1);
    tab1.add(og1);

    Fl_Group tab2(0, tabBarH, winW, tabsH - tabBarH, "Song Editor");
    tab2.color(bgColor);
    tabs.add(tab2);

    ObservableTimeline songTimeline(120.0f, 4, 4);
    for (int i = 0; i < 10; i++) {
        int patId = songTimeline.createPattern(og1.numPatternBeats());
        songTimeline.addTrack("Pattern " + std::to_string(i + 1), patId);
    }

    MarkerRuler timeSigRuler(0, tabBarH, winW, markerRulerH,
                              15, 60, MarkerRuler::TIME_SIG, &songTimeline, &timeSigPopup);
    tab2.add(timeSigRuler);

    MarkerRuler tempoRuler(0, tabBarH + markerRulerH, winW, markerRulerH,
                            15, 60, MarkerRuler::TEMPO, &songTimeline, &tempoPopup);
    tab2.add(tempoRuler);

    std::vector<Note> patterns(0);
    SongEditor og2(0, tabBarH + 2 * markerRulerH, patterns, 10, 15, 45, 60, 0.25, popup2);
    tab2.add(og2);

    SimpleTransport simpleTransport;
    Transport bottomPane(0, tabsH, winW, bottomH, &simpleTransport, &songTimeline);
    window.add(bottomPane);

    og2.setTransport(&simpleTransport, &songTimeline);
    og2.onEndReached = [&bottomPane]() { bottomPane.notifyEndReached(); };
    og2.onSeek       = [&bottomPane]() { bottomPane.notifySeek(); };

    og1.setPatternPlayhead(&simpleTransport, &songTimeline, 0);
    songTimeline.selectTrack(0);  // triggers PatternEditor to load track 0's pattern

    window.add(popup1);       window.registerPopup(&popup1);
    window.add(popup2);       window.registerPopup(&popup2);
    window.add(tempoPopup);   window.registerPopup(&tempoPopup);
    window.add(timeSigPopup); window.registerPopup(&timeSigPopup);

    window.show(argc, argv);
    return Fl::run();
}
