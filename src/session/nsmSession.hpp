#pragma once
#include <string>
#include "sessionManager.hpp"

class NsmClient;

// NSM mode: the session manager owns save timing. We never write to disk on our
// own — markDirty() just tells the manager the document changed (is_dirty), and
// the manager calls back via /nsm/client/save when it wants a save, which lands
// in save(). The session base path arrives later via /nsm/client/open, so it is
// set after construction with setSessionPath().
class NsmSession : public SessionManager {
public:
    NsmSession(std::function<AppState()> collect, NsmClient* nsm);

    void setSessionPath(std::string base) { basePath = std::move(base); }

    void markDirty() override;
    bool save() override;

private:
    NsmClient*  nsm;
    std::string basePath;   // session path from onOpen; ".luv" appended on save
};
