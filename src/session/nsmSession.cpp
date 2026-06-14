#include "nsmSession.hpp"
#include "nsm.hpp"

NsmSession::NsmSession(std::function<AppState()> collect, NsmClient* nsm)
    : SessionManager(std::move(collect)), nsm(nsm)
{}

void NsmSession::markDirty()
{
    if (!clean) return;          // already dirty — only report the transition
    clean = false;
    if (nsm) nsm->sendDirty();
}

bool NsmSession::save()
{
    if (basePath.empty()) return false;
    bool ok = saveAppState(collect(), basePath + ".luv");
    if (ok) {
        clean = true;
        if (nsm) nsm->sendClean();
    }
    return ok;
}
