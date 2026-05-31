#include "observableInstrument.hpp"
#include <algorithm>

ObservableInstrument::ObservableInstrument(ObservableSong* song)
    : song_(song)
{
    if (song_) song_->addObserver(this);
}

ObservableInstrument::~ObservableInstrument()
{
    if (song_) song_->removeObserver(this);
}

void ObservableInstrument::addObserver(ITimelineObserver* o)
{
    observers_.push_back(o);
}

void ObservableInstrument::removeObserver(ITimelineObserver* o)
{
    observers_.erase(std::remove(observers_.begin(), observers_.end(), o), observers_.end());
}

void ObservableInstrument::notify()
{
    auto copy = observers_;
    for (auto* o : copy) o->onTimelineChanged();
}

void ObservableInstrument::onTimelineChanged()
{
    notify();
}

int ObservableInstrument::add(std::string name, bool isDrum)
{
    return song_->addInstrument(std::move(name), isDrum);
}

void ObservableInstrument::rename(int id, std::string name)
{
    song_->renameInstrument(id, std::move(name));
}

void ObservableInstrument::remove(int id)
{
    song_->removeInstrument(id);
}

void ObservableInstrument::setPatternInstrument(int patternId, int instrumentId)
{
    for (auto& p : song_->data.patterns) {
        if (p.id == patternId) {
            p.instrumentId = instrumentId;
            song_->notify();
            return;
        }
    }
}
