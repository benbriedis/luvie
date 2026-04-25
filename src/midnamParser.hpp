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

inline std::map<int, std::string> gmPercussionMap() {
    return {
        {35, "Ac. Bass Drum"}, {36, "Bass Drum 1"},  {37, "Side Stick"},
        {38, "Acoustic Snare"},{39, "Hand Clap"},     {40, "Electric Snare"},
        {41, "Lo Floor Tom"},  {42, "Closed Hi-Hat"}, {43, "Hi Floor Tom"},
        {44, "Pedal Hi-Hat"},  {45, "Low Tom"},       {46, "Open Hi-Hat"},
        {47, "Low-Mid Tom"},   {48, "Hi-Mid Tom"},    {49, "Crash Cymbal 1"},
        {50, "High Tom"},      {51, "Ride Cymbal 1"}, {52, "Chinese Cymbal"},
        {53, "Ride Bell"},     {54, "Tambourine"},    {55, "Splash Cymbal"},
        {56, "Cowbell"},       {57, "Crash Cymbal 2"},{58, "Vibra-slap"},
        {59, "Ride Cymbal 2"}, {60, "Hi Bongo"},      {61, "Low Bongo"},
        {62, "Mute Hi Conga"}, {63, "Open Hi Conga"}, {64, "Low Conga"},
        {65, "High Timbale"},  {66, "Low Timbale"},   {67, "High Agogo"},
        {68, "Low Agogo"},     {69, "Cabasa"},         {70, "Maracas"},
        {71, "Short Whistle"}, {72, "Long Whistle"},  {73, "Short Guiro"},
        {74, "Long Guiro"},    {75, "Claves"},         {76, "Hi Wood Block"},
        {77, "Low Wood Block"},{78, "Mute Cuica"},    {79, "Open Cuica"},
        {80, "Mute Triangle"}, {81, "Open Triangle"},
    };
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
