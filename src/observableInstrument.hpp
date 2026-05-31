#pragma once
#include "itimelineobserver.hpp"
#include "observableSong.hpp"
#include "timeline.hpp"
#include <vector>

// Wraps ObservableSong and exposes instrument-level editing (add, rename, remove,
// assign to pattern). Re-notifies its own observers on every song change, so widgets
// that only care about instruments can register here rather than with ObservableSong.
class ObservableInstrument : public ITimelineObserver {
public:
    explicit ObservableInstrument(ObservableSong* song);
    ~ObservableInstrument();

    void addObserver(ITimelineObserver* o);
    void removeObserver(ITimelineObserver* o);

    ObservableSong*  song() const { return song_; }
    const Timeline&  get()  const { return song_->get(); }

    int  add(std::string name, bool isDrum = false);  // returns new instrument ID
    void rename(int id, std::string name);
    void remove(int id);
    void setPatternInstrument(int patternId, int instrumentId);

    void onTimelineChanged() override;

private:
    ObservableSong* song_;
    std::vector<ITimelineObserver*> observers_;
    void notify();
};
