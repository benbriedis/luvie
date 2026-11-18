// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "jackObserver.hpp"
#include "jackTransport.hpp"
#include <FL/Fl.H>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <system_error>
#include <thread>

namespace {
// How long attempt() blocks the UI thread waiting for the connect. A running server
// answers in a few milliseconds, and so does an absent one (the socket is refused),
// so this is only ever reached when the server is slow or wedged.
constexpr auto   kFastWait          = std::chrono::milliseconds(250);
// After that, how often the UI thread checks back...
constexpr double kCheckInterval     = 0.1;
// ...until the connect has been stuck this long, when the server is declared
// unresponsive and the checks slow down.
constexpr auto   kUnresponsiveAfter = std::chrono::seconds(3);
constexpr double kSlowCheckInterval = 0.5;

std::atomic<int> gStranded{0};   // see JackObserver::strandedConnects()
}

int JackObserver::strandedConnects() { return gStranded.load(); }

std::vector<std::string> JackObserver::unresponsiveAdvice() {
    if (JackTransport::isPipeWire())
        return {
            "The JACK server (PipeWire) seems to be hung. Restarting it usually fixes it:",
            "    systemctl --user restart pipewire pipewire-pulse wireplumber",
            "Luvie will connect by itself once the server answers.",
        };
    return {
        "The JACK server seems to be hung. Try restarting it;",
        "Luvie will connect by itself once the server answers.",
    };
}

// Shared by the UI thread and the worker running jack_client_open(). The worker
// holds its own reference, so this outlives the observer if the connect is still
// stuck when the observer is destroyed — the worker then closes any client itself.
struct JackObserver::Pending {
    std::mutex              m;
    std::condition_variable cv;
    bool                    done      = false;
    bool                    abandoned = false;   // observer gone: worker cleans up
    jack_client_t*          client    = nullptr;
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
};

JackObserver::JackObserver(JackTransport* jack) : jack_(jack) {}

JackObserver::~JackObserver() {
    if (timerScheduled_) Fl::remove_timeout(timerCb, this);
    if (checkScheduled_) Fl::remove_timeout(checkCb, this);
    if (pending_) {
        jack_client_t* c = nullptr;
        {
            std::lock_guard<std::mutex> lk(pending_->m);
            if (pending_->done) c = pending_->client;   // finished, never collected
            else {
                pending_->abandoned = true;
                gStranded.fetch_add(1);
            }
        }
        JackTransport::discard(c);
    }
}

void JackObserver::start() {
    if (up_) {                 // already have JACK — re-notify so clients re-attach
        setState(State::Up);
        return;
    }
    attempt();
}

void JackObserver::stop() {
    if (timerScheduled_) {
        Fl::remove_timeout(timerCb, this);
        timerScheduled_ = false;
    }
    // An in-flight connect is left to finish: its check timer stays armed, and the
    // client it brings back is discarded unless start() has asked again by then.
    if (state_ == State::Polling) setState(State::Down);
}

void JackObserver::release() {
    stop();
    if (!up_) return;
    up_ = false;
    jack_->close();
    setState(State::Down);
}

void JackObserver::serverLost() {
    if (!up_) return;          // already down / polling — nothing to tear down
    up_ = false;
    jack_->close();            // free the zombie client and its stale ports
    attempt();                 // retry now; falls through to Polling if still gone
}

void JackObserver::attempt() {
    if (pending_) {
        // A connect is already in flight (the server is slow or wedged). Don't start
        // another; Polling marks JACK as wanted, so finish() adopts what it returns.
        if (state_ != State::Polling) setState(State::Polling);
        return;
    }

    JackTransport::loadLibrary();   // on this thread: the shim's load isn't thread-safe
    auto p = std::make_shared<Pending>();
    try {
        std::thread([p]() {
            jack_client_t* c = JackTransport::connect();
            bool orphaned;
            {
                std::lock_guard<std::mutex> lk(p->m);
                p->client = c;
                p->done   = true;
                orphaned  = p->abandoned;
            }
            p->cv.notify_all();
            if (orphaned) {
                JackTransport::discard(c);
                gStranded.fetch_sub(1);
            }
        }).detach();
    } catch (const std::system_error&) {
        setState(State::Polling);   // no thread to be had: treat as not available
        schedulePoll();
        return;
    }

    bool done;
    {
        std::unique_lock<std::mutex> lk(p->m);
        done = p->cv.wait_for(lk, kFastWait, [&] { return p->done; });
    }
    if (done) {                     // the usual case: answered (or refused) promptly
        finish(p->client, true);
        return;
    }

    // Slow server. Run without JACK meanwhile and collect the result on a timer.
    pending_ = std::move(p);
    setState(State::Polling);
    scheduleCheck(kCheckInterval);
}

void JackObserver::finish(jack_client_t* client, bool wanted) {
    if (unresponsive_) {
        unresponsive_ = false;
        fprintf(stderr, "[luvie] JACK server answered after all\n");
    }
    if (!wanted) {                  // stopped while the connect was in flight
        JackTransport::discard(client);
        setState(state_);           // re-notify: serverUnresponsive() may have changed
        return;
    }
    if (jack_->adopt(client)) {
        up_ = true;
        setState(State::Up);
    } else {
        setState(State::Polling);
        schedulePoll();
    }
}

void JackObserver::schedulePoll() {
    if (timerScheduled_) return;
    Fl::add_timeout(1.0, timerCb, this);
    timerScheduled_ = true;
}

void JackObserver::scheduleCheck(double seconds) {
    if (checkScheduled_) return;
    Fl::add_timeout(seconds, checkCb, this);
    checkScheduled_ = true;
}

void JackObserver::timerCb(void* self) {
    auto* obs = static_cast<JackObserver*>(self);
    obs->timerScheduled_ = false;
    if (obs->state_ != State::Polling) return;  // stopped while waiting
    obs->attempt();
}

void JackObserver::checkCb(void* self) {
    auto* obs = static_cast<JackObserver*>(self);
    obs->checkScheduled_ = false;
    std::shared_ptr<Pending> p = obs->pending_;
    if (!p) return;

    bool           done;
    jack_client_t* client;
    {
        std::lock_guard<std::mutex> lk(p->m);
        done   = p->done;
        client = p->client;
    }
    if (done) {
        obs->pending_.reset();
        obs->finish(client, obs->state_ == State::Polling);
        return;
    }

    if (!obs->unresponsive_ &&
        std::chrono::steady_clock::now() - p->started >= kUnresponsiveAfter) {
        obs->unresponsive_ = true;
        fprintf(stderr, "[luvie] JACK server is not responding: a connection attempt "
                        "has been stuck for %lld s. Carrying on without JACK.\n",
                static_cast<long long>(kUnresponsiveAfter.count()));
        for (const auto& line : unresponsiveAdvice())
            fprintf(stderr, "[luvie] %s\n", line.c_str());
        obs->setState(obs->state_);   // re-notify so the UI can say so
    }
    obs->scheduleCheck(obs->unresponsive_ ? kSlowCheckInterval : kCheckInterval);
}

void JackObserver::setState(State s) {
    state_ = s;
    for (auto& l : listeners_) l(s);
}
