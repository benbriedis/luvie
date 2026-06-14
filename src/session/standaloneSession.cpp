#include "standaloneSession.hpp"
#include "luvieApp.hpp"   // LuvieApp::lastFileDir
#include <FL/Fl_Native_File_Chooser.H>
#include <filesystem>

StandaloneSession::StandaloneSession(std::function<AppState()> collect,
                                     std::string projectPath)
    : SessionManager(std::move(collect)), projectPath(std::move(projectPath))
{}

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

bool StandaloneSession::save()
{
    if (projectPath.empty()) return saveAs();   // no file yet — behave as Save As
    bool ok = writeTo(projectPath);
    if (ok) clean = true;
    return ok;
}

bool StandaloneSession::saveAs()
{
    std::string p = pickSavePath();
    if (p.empty()) return false;
    projectPath = p;
    bool ok = writeTo(projectPath);
    if (ok) clean = true;
    return ok;
}
