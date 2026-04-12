#pragma once
#include <functional>
#include <string>

// New Session Manager (NSM) client.
//
// Reads NSM_URL from the environment to find the session manager's OSC address.
// Call init() at startup. Incoming OSC messages are dispatched via FLTK's
// Fl::add_fd() — the event loop wakes only when NSM actually sends something,
// so no timer or polling is needed.
class NsmClient {
public:
    NsmClient();
    ~NsmClient();

    // Announce to NSM. Returns true if NSM_URL is set and the announce was
    // sent. The session manager will reply asynchronously via the FLTK fd watch.
    // appName  : human-readable name shown in the session manager UI
    // exe      : basename of argv[0] (used to re-launch the app)
    bool init(const char* appName, const char* exe);

    bool isActive() const { return active_; }

    // --- Callbacks (set before calling init) ---

    // Called when the session manager sends /nsm/client/open.
    // path is the base path for session data (append an extension, e.g. ".json").
    // Return true on success; false to report an error to the session manager.
    std::function<bool(const std::string& path, const std::string& displayName)> onOpen;

    // Called when the session manager requests a save.
    // Use the path from the most recent onOpen call.
    // Return true on success.
    std::function<bool()> onSave;

private:
    void* server_  = nullptr;   // lo_server (opaque)
    void* nsmAddr_ = nullptr;   // lo_address (opaque)
    int   sockFd_  = -1;        // fd registered with Fl::add_fd
    bool  active_  = false;

    void replyOk(const char* addr);
    void replyError(const char* addr, int code, const char* msg);

    // Called by FLTK when the OSC socket becomes readable.
    static void fdReadable(int fd, void* ud);

    // liblo method handlers (registered with this as user_data)
    static int cbOpen(const char* path, const char* types,
                      void* argv[], int argc, void* msg, void* ud);
    static int cbSave(const char* path, const char* types,
                      void* argv[], int argc, void* msg, void* ud);
    static int cbReply(const char* path, const char* types,
                       void* argv[], int argc, void* msg, void* ud);
};
