#include "observableTimeline.hpp"
#include <algorithm>

ObservableTimeline::ObservableTimeline(float initBpm, int initTop, int initBottom)
{
	data.bpms.push_back({0, initBpm});
	data.timeSigs.push_back({0, initTop, initBottom});
}

void ObservableTimeline::addObserver(ITimelineObserver* o)
{
	observers.push_back(o);
}

void ObservableTimeline::removeObserver(ITimelineObserver* o)
{
	observers.erase(std::remove(observers.begin(), observers.end(), o), observers.end());
}

void ObservableTimeline::notify()
{
	for (auto* o : observers) o->onTimelineChanged();
}

void ObservableTimeline::sortBpms()
{
	std::sort(data.bpms.begin(), data.bpms.end(),
		[](const BpmMarker& a, const BpmMarker& b) { return a.bar < b.bar; });
}

void ObservableTimeline::sortTimeSigs()
{
	std::sort(data.timeSigs.begin(), data.timeSigs.end(),
		[](const TimeSigMarker& a, const TimeSigMarker& b) { return a.bar < b.bar; });
}

void ObservableTimeline::setBpm(int bar, float bpm)
{
	for (auto& m : data.bpms) {
		if (m.bar == bar) { m.bpm = bpm; notify(); return; }
	}
	data.bpms.push_back({bar, bpm});
	sortBpms();
	notify();
}

void ObservableTimeline::removeBpm(int bar)
{
	if (bar == 0) return;
	data.bpms.erase(std::remove_if(data.bpms.begin(), data.bpms.end(),
		[bar](const BpmMarker& m) { return m.bar == bar; }), data.bpms.end());
	notify();
}

void ObservableTimeline::moveBpmMarker(int fromBar, int toBar)
{
	if (fromBar == 0) return;
	auto it = std::find_if(data.bpms.begin(), data.bpms.end(),
		[fromBar](const BpmMarker& m) { return m.bar == fromBar; });
	if (it == data.bpms.end()) return;
	float val = it->bpm;
	data.bpms.erase(it);
	data.bpms.erase(std::remove_if(data.bpms.begin(), data.bpms.end(),
		[toBar](const BpmMarker& m) { return m.bar == toBar; }), data.bpms.end());
	data.bpms.push_back({toBar, val});
	sortBpms();
	notify();
}

float ObservableTimeline::bpmAt(int bar) const
{
	float result = 120.0f;
	for (auto& m : data.bpms) {
		if (m.bar <= bar) result = m.bpm;
		else break;
	}
	return result;
}

void ObservableTimeline::setTimeSig(int bar, int top, int bottom)
{
	for (auto& m : data.timeSigs) {
		if (m.bar == bar) { m.top = top; m.bottom = bottom; notify(); return; }
	}
	data.timeSigs.push_back({bar, top, bottom});
	sortTimeSigs();
	notify();
}

void ObservableTimeline::removeTimeSig(int bar)
{
	if (bar == 0) return;
	data.timeSigs.erase(std::remove_if(data.timeSigs.begin(), data.timeSigs.end(),
		[bar](const TimeSigMarker& m) { return m.bar == bar; }), data.timeSigs.end());
	notify();
}

void ObservableTimeline::moveTimeSigMarker(int fromBar, int toBar)
{
	if (fromBar == 0) return;
	auto it = std::find_if(data.timeSigs.begin(), data.timeSigs.end(),
		[fromBar](const TimeSigMarker& m) { return m.bar == fromBar; });
	if (it == data.timeSigs.end()) return;
	int top = it->top, bottom = it->bottom;
	data.timeSigs.erase(it);
	data.timeSigs.erase(std::remove_if(data.timeSigs.begin(), data.timeSigs.end(),
		[toBar](const TimeSigMarker& m) { return m.bar == toBar; }), data.timeSigs.end());
	data.timeSigs.push_back({toBar, top, bottom});
	sortTimeSigs();
	notify();
}

void ObservableTimeline::timeSigAt(int bar, int& top, int& bottom) const
{
	top = 4; bottom = 4;
	for (auto& m : data.timeSigs) {
		if (m.bar <= bar) { top = m.top; bottom = m.bottom; }
		else break;
	}
}
