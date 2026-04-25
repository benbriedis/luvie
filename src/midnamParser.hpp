#pragma once
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

inline std::map<int, std::string> parseMidnam(const std::string& path) {
    std::map<int, std::string> result;
    std::ifstream f(path);
    if (!f) return result;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    std::regex re(R"x(<Note\s+Number\s*=\s*"(\d+)"\s+Name\s*=\s*"([^"]*)")x",
                  std::regex::icase);
    for (auto it = std::sregex_iterator(content.begin(), content.end(), re);
         it != std::sregex_iterator(); ++it) {
        int note = std::stoi((*it)[1]);
        if (note >= 0 && note <= 127)
            result[note] = (*it)[2];
    }
    return result;
}

inline bool exportMidnam(const std::string& path,
                         const std::map<int, std::string>& map,
                         const std::string& channelName) {
    std::ofstream f(path);
    if (!f) return false;
    f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n";
    f << R"(<!DOCTYPE MIDINameDocument PUBLIC "-//MIDI Manufacturers Association//DTD MIDINameDocument 1.0//EN" "http://midi.org">)" "\n";
    f << "<MIDINameDocument>\n";
    f << "    <MasterDeviceNames>\n";
    f << "        <Manufacturer>Custom</Manufacturer>\n";
    f << "        <Model>" << channelName << "</Model>\n";
    f << "        <NoteNames>\n";
    for (const auto& [note, name] : map)
        f << "            <Note Number=\"" << note << "\" Name=\"" << name << "\"/>\n";
    f << "        </NoteNames>\n";
    f << "    </MasterDeviceNames>\n";
    f << "</MIDINameDocument>\n";
    return f.good();
}
