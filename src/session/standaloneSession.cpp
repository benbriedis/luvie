#include "standaloneSession.hpp"
#include "luvieApp.hpp"   // LuvieApp::lastFileDir
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <filesystem>

StandaloneSession::StandaloneSession(std::function<AppState()> collect,
                                     std::string projectPath)
    : SessionManager(std::move(collect)), projectPath(std::move(projectPath))
{}

StandaloneSession::~StandaloneSession()
{
    Fl::remove_timeout(autoSaveCb, this);   // cancel any pending auto-save
}

// Shows a Save As dialog and returns the chosen path (with a .luv extension),
// or an empty string if the user cancelled.
std::string StandaloneSession::pickSavePath()
{
    Fl_Native_File_Chooser fc;
    fc.title("Save Project As");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.filter("Luvie Projects\t*.luv\nAll Files\t*");
    fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    if (!LuvieApp::lastFileDir.empty()) fc.directory(LuvieApp::lastFileDir.c_str());
    if (fc.show() != 0) return {};
    std::string p = fc.filename();
    if (p.size() < 4 || p.substr(p.size() - 4) != ".luv") p += ".luv";
    LuvieApp::lastFileDir = std::filesystem::path(p).parent_path().string();
    return p;
}

bool StandaloneSession::writeTo(const std::string& path)
{
    return saveAppState(collect(), path);
}

void StandaloneSession::markDirty()
{
    clean = false;
    scheduleAutoSave();
}

// Reset the debounce timer so the auto-save fires autoSaveDelay seconds after the
// most recent edit. No-op until a project file exists (nowhere to auto-save into).
void StandaloneSession::scheduleAutoSave()
{
    if (projectPath.empty()) return;
    Fl::remove_timeout(autoSaveCb, this);
    Fl::add_timeout(autoSaveDelay, autoSaveCb, this);
}

void StandaloneSession::autoSaveCb(void* self)
{
    static_cast<StandaloneSession*>(self)->autoSave();
}

// Timer fired: persist pending changes. A failed write leaves the document dirty;
// the next edit re-arms the timer for another attempt.
void StandaloneSession::autoSave()
{
    if (clean || projectPath.empty()) return;
    if (writeTo(projectPath)) clean = true;
}

bool StandaloneSession::save()
{
    if (projectPath.empty()) return saveAs();   // no file yet — behave as Save As
    Fl::remove_timeout(autoSaveCb, this);        // explicit save supersedes the timer
    bool ok = writeTo(projectPath);
    if (ok) clean = true;
    return ok;
}

bool StandaloneSession::saveAs()
{
    std::string p = pickSavePath();
    if (p.empty()) return false;
    Fl::remove_timeout(autoSaveCb, this);
    projectPath = p;
    bool ok = writeTo(projectPath);
    if (ok) clean = true;
    return ok;
}
