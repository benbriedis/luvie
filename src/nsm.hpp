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
    // path is the base path for session data (append an extension, e.g. ".luv").
    // Return true on success; false to report an error to the session manager.
    std::function<bool(const std::string& path, const std::string& displayName)> onOpen;

    // Called when the session manager requests a save.
    // Use the path from the most recent onOpen call.
    // Return true on success.
    std::function<bool()> onSave;

    // Called when the session manager asks to show / hide the optional GUI
    // (the "eye" toggle). The handler should show/hide the window and then call
    // setGuiVisible() to report the new state back.
    std::function<void()> onShowGui;
    std::function<void()> onHideGui;

    // Report the GUI's current visibility to the session manager so its "eye"
    // toggle stays in sync. Sent immediately; also re-sent when the announce
    // reply arrives, in case this raced ahead of the server registering us.
    void setGuiVisible(bool visible);

    // Report whether the document has unsaved changes (the ":dirty:" capability).
    // The session manager uses this to show modified state and to prompt on quit;
    // it still drives the actual save via /nsm/client/save.
    void sendDirty();
    void sendClean();

    // Service incoming OSC directly, blocking up to timeoutMs for the first
    // message then draining the rest. Needed when no FLTK window is shown, since
    // FLTK's event loop (and thus our Fl::add_fd watch) goes idle then.
    void poll(int timeoutMs = 0);

private:
    void* server_     = nullptr;   // lo_server (opaque)
    void* nsmAddr_    = nullptr;   // lo_address (opaque)
    int   sockFd_     = -1;        // fd registered with Fl::add_fd
    bool  active_     = false;
    bool  guiVisible_ = true;      // last-reported GUI visibility

    void replyOk(const char* addr);
    void replyError(const char* addr, int code, const char* msg);
    void sendGuiState();           // emit gui_is_shown/hidden for guiVisible_

    // Called by FLTK when the OSC socket becomes readable.
    static void fdReadable(int fd, void* ud);

    // liblo method handlers (registered with this as user_data)
    static int cbOpen(const char* path, const char* types,
                      void* argv[], int argc, void* msg, void* ud);
    static int cbSave(const char* path, const char* types,
                      void* argv[], int argc, void* msg, void* ud);
    static int cbReply(const char* path, const char* types,
                       void* argv[], int argc, void* msg, void* ud);
    static int cbShowGui(const char* path, const char* types,
                         void* argv[], int argc, void* msg, void* ud);
    static int cbHideGui(const char* path, const char* types,
                         void* argv[], int argc, void* msg, void* ud);
};
