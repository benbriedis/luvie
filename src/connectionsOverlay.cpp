#include "connectionsOverlay.hpp"
#include "midnamParser.hpp"
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl.H>

// ── Layout ────────────────────────────────────────────────────────────────────

static constexpr int headerH  = 52;
static constexpr int closeSz  = 36;
static constexpr int closePad = 10;
static constexpr int titlePad = 16;
static constexpr int sectionH = 44;
static constexpr int colH     = 22;
static constexpr int rowsTopY = headerH + sectionH + colH;
static constexpr int rowH     = 40;
static constexpr int inputH   = 26;
static constexpr int addBtnH  = 32;
static constexpr int addBtnW  = 150;
static constexpr int addBtnPad= 10;
static constexpr int pad      = 16;
static constexpr int delBtnSz = 26;

static constexpr int chanSecH    = 44;
static constexpr int chanColH2   = 22;
static constexpr int chanGap     = 6;
static constexpr int chanMidiW   = 70;
static constexpr int typeLabelW  = 62;

static constexpr int progRowH          = 28;
static constexpr int progBtnH          = 20;

static constexpr int drumRowH          = 32;
static constexpr int drumBtnH          = 22;
static constexpr int drumImportW       = 115;
static constexpr int drumGmW           = 105;
static constexpr int drumGsW           = 100;
static constexpr int drumExportW       = 115;
static constexpr int drumClearW        = 65;
static constexpr int drumFallbackLabelW = 58;
static constexpr int drumFallbackChoiceW = 90;
static constexpr int drumBtnGap        = 8;

// ── Colors ────────────────────────────────────────────────────────────────────

static constexpr Fl_Color bgCol      = 0xFFFFFF00;
static constexpr Fl_Color borderCol  = 0xCBD5E100;
static constexpr Fl_Color dividerCol = 0xE5E7EB00;
static constexpr Fl_Color textCol    = 0x37415100;
static constexpr Fl_Color subTextCol = 0x6B728000;
static constexpr Fl_Color inputBgCol = 0xF9FAFB00;
static constexpr Fl_Color delRedCol  = 0xEF444400;
static constexpr Fl_Color addBtnBg   = 0xF3F4F600;

// ── GM1 instrument list ───────────────────────────────────────────────────────

static constexpr const char* kGm1Names[128] = {
    "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano",
    "Honky-Tonk Piano",     "Electric Piano 1",      "Electric Piano 2",
    "Harpsichord",          "Clavi",
    "Celesta",              "Glockenspiel",           "Music Box",
    "Vibraphone",           "Marimba",                "Xylophone",
    "Tubular Bells",        "Dulcimer",
    "Drawbar Organ",        "Percussive Organ",       "Rock Organ",
    "Church Organ",         "Reed Organ",             "Accordion",
    "Harmonica",            "Tango Accordion",
    "Acoustic Guitar (Nylon)", "Acoustic Guitar (Steel)", "Electric Guitar (Jazz)",
    "Electric Guitar (Clean)", "Electric Guitar (Muted)", "Overdriven Guitar",
    "Distortion Guitar",    "Guitar Harmonics",
    "Acoustic Bass",        "Electric Bass (Finger)", "Electric Bass (Pick)",
    "Fretless Bass",        "Slap Bass 1",            "Slap Bass 2",
    "Synth Bass 1",         "Synth Bass 2",
    "Violin",               "Viola",                  "Cello",
    "Contrabass",           "Tremolo Strings",        "Pizzicato Strings",
    "Orchestral Harp",      "Timpani",
    "String Ensemble 1",    "String Ensemble 2",      "Synth Strings 1",
    "Synth Strings 2",      "Choir Aahs",             "Voice Oohs",
    "Synth Voice",          "Orchestra Hit",
    "Trumpet",              "Trombone",               "Tuba",
    "Muted Trumpet",        "French Horn",            "Brass Section",
    "Synth Brass 1",        "Synth Brass 2",
    "Soprano Sax",          "Alto Sax",               "Tenor Sax",
    "Baritone Sax",         "Oboe",                   "English Horn",
    "Bassoon",              "Clarinet",
    "Piccolo",              "Flute",                  "Recorder",
    "Pan Flute",            "Blown Bottle",           "Shakuhachi",
    "Whistle",              "Ocarina",
    "Lead 1 (Square)",      "Lead 2 (Sawtooth)",      "Lead 3 (Calliope)",
    "Lead 4 (Chiff)",       "Lead 5 (Charang)",       "Lead 6 (Voice)",
    "Lead 7 (Fifths)",      "Lead 8 (Bass + Lead)",
    "Pad 1 (New Age)",      "Pad 2 (Warm)",           "Pad 3 (Polysynth)",
    "Pad 4 (Choir)",        "Pad 5 (Bowed)",          "Pad 6 (Metallic)",
    "Pad 7 (Halo)",         "Pad 8 (Sweep)",
    "FX 1 (Rain)",          "FX 2 (Soundtrack)",      "FX 3 (Crystal)",
    "FX 4 (Atmosphere)",    "FX 5 (Brightness)",      "FX 6 (Goblins)",
    "FX 7 (Echoes)",        "FX 8 (Sci-Fi)",
    "Sitar",                "Banjo",                  "Shamisen",
    "Koto",                 "Kalimba",                "Bag Pipe",
    "Fiddle",               "Shanai",
    "Tinkle Bell",          "Agogo",                  "Steel Drums",
    "Woodblock",            "Taiko Drum",             "Melodic Tom",
    "Synth Drum",           "Reverse Cymbal",
    "Guitar Fret Noise",    "Breath Noise",           "Seashore",
    "Bird Tweet",           "Telephone Ring",         "Helicopter",
    "Applause",             "Gunshot",
};

static int parseOptionalInt(const char* v, int lo, int hi) {
    if (!v || !v[0]) return -1;
    char* end;
    long n = std::strtol(v, &end, 10);
    if (end == v || *end) return -1;
    if (n < lo || n > hi) return -1;
    return static_cast<int>(n);
}

// ── NameInput ─────────────────────────────────────────────────────────────

class NameInput : public Fl_Input {
    void draw() override {
        Fl_Input::draw();
        fl_color(Fl::focus() == this ? 0x3B82F600 : borderCol);
        fl_line_style(FL_SOLID, Fl::focus() == this ? 2 : 1);
        fl_rect(x(), y(), w(), h());
        fl_line_style(0);
    }
public:
    NameInput(int x, int y, int w, int h) : Fl_Input(x, y, w, h) {
        when(FL_WHEN_NEVER);
        box(FL_BORDER_BOX);
    }
    int handle(int event) override {
        if (event == FL_KEYBOARD) {
            int k = Fl::event_key();
            if (k == FL_Enter || k == FL_KP_Enter) {
                do_callback();
                return 0;  // bubble to overlay for focus advance
            }
        }
        int r = Fl_Input::handle(event);
        if (event == FL_UNFOCUS) do_callback();
        return r;
    }
};

// ── Constructor ───────────────────────────────────────────────────────────────

ConnectionsOverlay::ConnectionsOverlay(int x, int y, int w, int h)
    : BasePopup(x, y, w, h)
{
    box(FL_NO_BOX);
    begin();

    closeBtn = new ModernButton(
        w - closePad - closeSz, (headerH - closeSz) / 2,
        closeSz, closeSz, "\xc3\x97");
    closeBtn->labelsize(22);
    closeBtn->labelcolor(textCol);
    closeBtn->color(bgCol);
    closeBtn->setBorderWidth(0);
    closeBtn->callback([](Fl_Widget*, void* d) {
        static_cast<ConnectionsOverlay*>(d)->hide();
    }, this);

    addBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Port");
    addBtn->labelsize(12);
    addBtn->labelcolor(textCol);
    addBtn->color(addBtnBg);
    addBtn->setBorderWidth(1);
    addBtn->setBorderColor(borderCol);
    addBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<ConnectionsOverlay*>(d);
        std::string name = self->uniquePortName(
            "midi_out_" + std::to_string(self->connections_.size() + 1));
        self->connections_.push_back({self->nextPortId_++, name});
        self->rebuildRows();
        if (self->onPortAdded) self->onPortAdded(name);
    }, this);

    addChanBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Channel");
    addChanBtn->labelsize(12);
    addChanBtn->labelcolor(textCol);
    addChanBtn->color(addBtnBg);
    addChanBtn->setBorderWidth(1);
    addChanBtn->setBorderColor(borderCol);
    addChanBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<ConnectionsOverlay*>(d);
        const std::string portName = self->connections_.empty()
            ? "" : self->connections_[0].portName;
        const std::string name = self->nextNumberedChanName(false);
        self->channels_.push_back({self->nextChanId_++, name, portName, 1, {}, false});
        self->rebuildChannelRows();
        if (self->onChannelsChanged) self->onChannelsChanged();
    }, this);

    addDrumChanBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Drum Channel");
    addDrumChanBtn->labelsize(12);
    addDrumChanBtn->labelcolor(textCol);
    addDrumChanBtn->color(addBtnBg);
    addDrumChanBtn->setBorderWidth(1);
    addDrumChanBtn->setBorderColor(borderCol);
    addDrumChanBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<ConnectionsOverlay*>(d);
        const std::string portName = self->connections_.empty()
            ? "" : self->connections_[0].portName;
        const std::string name = self->nextNumberedChanName(true);
        self->channels_.push_back({self->nextChanId_++, name, portName, 1, {}, true});
        self->rebuildChannelRows();
        if (self->onChannelsChanged) self->onChannelsChanged();
    }, this);

    end();

    connections_.push_back({nextPortId_++, "midi_out_1"});
    channels_.push_back({nextChanId_++, "Instrument 1", "midi_out_1",  1, {}, false});
    channels_.push_back({nextChanId_++, "Drums 1",      "midi_out_1", 10, {}, true});
    rebuildRows();
    hide();
}

// ── Public API ────────────────────────────────────────────────────────────────

void ConnectionsOverlay::hide() {
    syncFromInputs();
    // Port rename safety net (for inputs that didn't fire unfocus before hide)
    for (int i = 0; i < (int)rows_.size() && i < (int)connections_.size(); i++) {
        const std::string newName = connections_[i].portName;
        const std::string oldName = rows_[i].committedName;
        if (newName == oldName) continue;
        for (auto& ch : channels_)
            if (ch.portName == oldName) ch.portName = newName;
        if (onPortRenamed) onPortRenamed(oldName, newName);
    }
    // Channel name safety net
    bool chanChanged = false;
    for (int i = 0; i < (int)chanRows_.size() && i < (int)channels_.size(); i++) {
        if (!chanRows_[i].nameInput) continue;
        std::string newName = chanRows_[i].nameInput->value();
        const std::string oldChanName = chanRows_[i].committedName;
        if (!newName.empty() && newName != oldChanName) {
            newName = uniqueChanName(newName, i);
            channels_[i].name = newName;
            if (onChannelRenamed) onChannelRenamed(oldChanName, newName);
            chanChanged = true;
        }
    }
    if (chanChanged && onChannelsChanged) onChannelsChanged();
    if (onClose) onClose();
    BasePopup::hide();
}

void ConnectionsOverlay::setConnections(const std::vector<std::string>& portNames) {
    connections_.clear();
    for (const auto& n : portNames)
        if (!n.empty()) connections_.push_back({nextPortId_++, n});
    if (connections_.empty())
        connections_.push_back({nextPortId_++, "midi_out_1"});
    rebuildRows();
}

std::vector<std::string> ConnectionsOverlay::getConnections() const {
    std::vector<std::string> result;
    for (const auto& c : connections_)
        result.push_back(c.portName);
    return result;
}

void ConnectionsOverlay::setChannels(const std::vector<ChannelInfo>& chans) {
    channels_.clear();
    for (const auto& ci : chans)
        channels_.push_back({nextChanId_++, ci.name, ci.portName, ci.midiChannel, ci.drumMap,
                             ci.isDrum, ci.fallbackNoteNames, ci.programNumber, ci.bankMsb, ci.bankLsb,
                             ci.gm1Instrument});
    rebuildChannelRows();
}

std::vector<ConnectionsOverlay::ChannelInfo> ConnectionsOverlay::getChannels() const {
    std::vector<ChannelInfo> result;
    for (const auto& ch : channels_)
        result.push_back({ch.id, ch.name, ch.portName, ch.midiChannel, ch.drumMap,
                          ch.isDrum, ch.fallbackNoteNames, ch.programNumber, ch.bankMsb, ch.bankLsb,
                          ch.gm1Instrument});
    return result;
}

void ConnectionsOverlay::updateChannelDrumMap(const std::string& chanName, int midiNote, const std::string& label)
{
    for (auto& ch : channels_) {
        if (ch.name == chanName) {
            if (label.empty())
                ch.drumMap.erase(midiNote);
            else
                ch.drumMap[midiNote] = label;
            return;
        }
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::string ConnectionsOverlay::uniquePortName(const std::string& base, int excludeIdx) const {
    auto isUnique = [&](const std::string& name) {
        for (int i = 0; i < (int)connections_.size(); i++) {
            if (i == excludeIdx) continue;
            if (connections_[i].portName == name) return false;
        }
        return true;
    };
    if (isUnique(base)) return base;
    for (int n = 2; ; n++) {
        std::string c = base + "_" + std::to_string(n);
        if (isUnique(c)) return c;
    }
}

std::string ConnectionsOverlay::uniqueChanName(const std::string& base, int excludeIdx) const {
    auto isUnique = [&](const std::string& name) {
        for (int i = 0; i < (int)channels_.size(); i++) {
            if (i == excludeIdx) continue;
            if (channels_[i].name == name) return false;
        }
        return true;
    };
    if (isUnique(base)) return base;
    for (int n = 2; ; n++) {
        std::string c = base + "_" + std::to_string(n);
        if (isUnique(c)) return c;
    }
}

std::string ConnectionsOverlay::nextNumberedChanName(bool isDrum) const {
    const std::string prefix = isDrum ? "Drums " : "Instrument ";
    auto inUse = [&](const std::string& name) {
        for (const auto& ch : channels_)
            if (ch.name == name) return true;
        return false;
    };
    for (int n = 1; ; n++) {
        std::string name = prefix + std::to_string(n);
        if (!inUse(name)) return name;
    }
}

void ConnectionsOverlay::syncFromInputs() {
    for (int i = 0; i < (int)rows_.size() && i < (int)connections_.size(); i++)
        if (rows_[i].input) connections_[i].portName = rows_[i].input->value();
}

void ConnectionsOverlay::rebuildRows() {
    for (auto& row : rows_) {
        if (row.input)     { remove(row.input);     Fl::delete_widget(row.input); }
        if (row.deleteBtn) { remove(row.deleteBtn); Fl::delete_widget(row.deleteBtn); }
    }
    rows_.clear();

    const int inputW = w() - 2*pad - delBtnSz - 8;
    int y = rowsTopY;

    begin();
    for (int i = 0; i < (int)connections_.size(); i++) {
        const int iy = y + (rowH - inputH) / 2;

        auto* inp = new NameInput(pad, iy, inputW, inputH);
        inp->color(inputBgCol);
        inp->textcolor(textCol);
        inp->textsize(12);
        inp->value(connections_[i].portName.c_str());
        inp->callback(inputCb, this);

        auto* del = new ModernButton(
            w() - pad - delBtnSz, y + (rowH - delBtnSz) / 2,
            delBtnSz, delBtnSz, "\xc3\x97");
        del->labelsize(14);
        del->labelcolor(delRedCol);
        del->color(bgCol);
        del->setBorderWidth(0);
        del->callback(deleteCb, this);

        const std::string& pname = connections_[i].portName;
        bool referenced = std::any_of(channels_.begin(), channels_.end(),
            [&](const Channel& ch){ return ch.portName == pname; });
        if (referenced) del->deactivate();

        rows_.push_back({inp, del, connections_[i].portName});
        y += rowH;
    }
    end();

    addBtn->position(w() - pad - addBtnW, y + addBtnPad);
    y += addBtnPad + addBtnH + addBtnPad;

    chanSectionTopY_ = y;
    chanRowsTopY_    = y + chanSecH + chanColH2;

    rebuildChannelRows();
}

void ConnectionsOverlay::rebuildChannelRows() {
    for (auto& row : chanRows_) {
        if (row.typeLabel)     { remove(row.typeLabel);     Fl::delete_widget(row.typeLabel); }
        if (row.nameInput)     { remove(row.nameInput);     Fl::delete_widget(row.nameInput); }
        if (row.portChoice)    { remove(row.portChoice);    Fl::delete_widget(row.portChoice); }
        if (row.midiChanChoice){ remove(row.midiChanChoice);Fl::delete_widget(row.midiChanChoice); }
        if (row.deleteBtn)     { remove(row.deleteBtn);     Fl::delete_widget(row.deleteBtn); }
        if (row.importBtn)      { remove(row.importBtn);      Fl::delete_widget(row.importBtn); }
        if (row.gmBtn)          { remove(row.gmBtn);          Fl::delete_widget(row.gmBtn); }
        if (row.gsBtn)          { remove(row.gsBtn);          Fl::delete_widget(row.gsBtn); }
        if (row.exportBtn)      { remove(row.exportBtn);      Fl::delete_widget(row.exportBtn); }
        if (row.clearBtn)       { remove(row.clearBtn);       Fl::delete_widget(row.clearBtn); }
        if (row.fallbackLabel)   { remove(row.fallbackLabel);   Fl::delete_widget(row.fallbackLabel); }
        if (row.fallbackChoice)  { remove(row.fallbackChoice);  Fl::delete_widget(row.fallbackChoice); }
        if (row.programInput)    { remove(row.programInput);    Fl::delete_widget(row.programInput); }
        if (row.programDropdown) { remove(row.programDropdown); Fl::delete_widget(row.programDropdown); }
        if (row.bankMsbInput)    { remove(row.bankMsbInput);    Fl::delete_widget(row.bankMsbInput); }
        if (row.bankLsbInput)    { remove(row.bankLsbInput);    Fl::delete_widget(row.bankLsbInput); }
    }
    chanRows_.clear();

    const int usableW  = w() - 2*pad - delBtnSz - 8 - 2*chanGap;
    const int remaining = usableW - chanMidiW - typeLabelW - chanGap;
    chanNameW_ = remaining * 45 / 100;
    chanPortW_ = remaining - chanNameW_;

    int y = chanRowsTopY_;

    begin();
    for (int i = 0; i < (int)channels_.size(); i++) {
        const int iy    = y + (rowH - inputH) / 2;
        const bool drum = channels_[i].isDrum;

        // Type label
        auto* typeLbl = new Fl_Box(pad, iy, typeLabelW, inputH,
            drum ? "Drum" : "Standard");
        typeLbl->box(FL_NO_BOX);
        typeLbl->labelcolor(subTextCol);
        typeLbl->labelsize(10);

        const int nameX = pad + typeLabelW + chanGap;
        auto* nameInp = new NameInput(nameX, iy, chanNameW_, inputH);
        nameInp->color(inputBgCol);
        nameInp->textcolor(textCol);
        nameInp->textsize(12);
        nameInp->value(channels_[i].name.c_str());
        nameInp->callback(chanNameCb, this);

        const int portX = nameX + chanNameW_ + chanGap;
        auto* portCh = new ModernChoice(portX, iy, chanPortW_, inputH);
        portCh->color(inputBgCol);
        portCh->labelcolor(textCol);
        portCh->textsize(12);
        portCh->setBorderColor(borderCol);
        portCh->setArrowColor(subTextCol);
        portCh->setHoverColor(0xF3F4F600);
        int selPort = 0;
        for (int j = 0; j < (int)connections_.size(); j++) {
            portCh->add(connections_[j].portName.c_str());
            if (connections_[j].portName == channels_[i].portName) selPort = j;
        }
        portCh->value(selPort);
        portCh->callback(portChoiceCb, this);

        const int midiX = portX + chanPortW_ + chanGap;
        auto* midiCh = new ModernChoice(midiX, iy, chanMidiW, inputH);
        midiCh->color(inputBgCol);
        midiCh->labelcolor(textCol);
        midiCh->textsize(12);
        midiCh->setBorderColor(borderCol);
        midiCh->setArrowColor(subTextCol);
        midiCh->setHoverColor(0xF3F4F600);
        for (int ch = 1; ch <= 16; ch++)
            midiCh->add(std::to_string(ch).c_str());
        midiCh->value(channels_[i].midiChannel - 1);
        midiCh->callback(midiChanChoiceCb, this);

        auto* del = new ModernButton(
            w() - pad - delBtnSz, y + (rowH - delBtnSz) / 2,
            delBtnSz, delBtnSz, "\xc3\x97");
        del->labelsize(14);
        del->labelcolor(delRedCol);
        del->color(bgCol);
        del->setBorderWidth(0);
        del->callback(chanDeleteCb, this);

        {
            int typeCount = 0;
            for (const auto& ch : channels_) if (ch.isDrum == drum) ++typeCount;
            bool inUse = isChannelInUse && isChannelInUse(channels_[i].name);
            if (typeCount <= 1 || inUse) del->deactivate();
        }

        // Drum mappings sub-row — only for drum channels
        ModernButton* imp  = nullptr;
        ModernButton* gm   = nullptr;
        ModernButton* gs   = nullptr;
        ModernButton* exp  = nullptr;
        ModernButton* clr  = nullptr;
        Fl_Box*       fbLbl = nullptr;
        Fl_Choice*    fbCh  = nullptr;
        if (drum) {
            const int drumBtnY = y + rowH + (drumRowH - drumBtnH) / 2;

            const int drumStartX = pad + typeLabelW + chanGap;
            imp = new ModernButton(drumStartX, drumBtnY, drumImportW, drumBtnH, "Import drum map");
            imp->labelsize(11);
            imp->labelcolor(textCol);
            imp->color(addBtnBg);
            imp->setBorderWidth(1);
            imp->setBorderColor(borderCol);
            imp->callback(importDrumMapCb, this);

            const int gmX = drumStartX + drumImportW + drumBtnGap;
            gm = new ModernButton(gmX, drumBtnY, drumGmW, drumBtnH, "Load GM map");
            gm->labelsize(11);
            gm->labelcolor(textCol);
            gm->color(addBtnBg);
            gm->setBorderWidth(1);
            gm->setBorderColor(borderCol);
            gm->callback(loadGmMapCb, this);

            const int gsX = gmX + drumGmW + drumBtnGap;
            gs = new ModernButton(gsX, drumBtnY, drumGsW, drumBtnH, "Load GS map");
            gs->labelsize(11);
            gs->labelcolor(textCol);
            gs->color(addBtnBg);
            gs->setBorderWidth(1);
            gs->setBorderColor(borderCol);
            gs->callback(loadGsMapCb, this);

            const int expX = gsX + drumGsW + drumBtnGap;
            exp = new ModernButton(expX, drumBtnY, drumExportW, drumBtnH, "Export drum map");
            exp->labelsize(11);
            exp->labelcolor(textCol);
            exp->color(addBtnBg);
            exp->setBorderWidth(1);
            exp->setBorderColor(borderCol);
            exp->callback(exportDrumMapCb, this);

            const int clrX = expX + drumExportW + drumBtnGap;
            clr = new ModernButton(clrX, drumBtnY, drumClearW, drumBtnH, "Clear");
            clr->labelsize(11);
            clr->labelcolor(textCol);
            clr->color(addBtnBg);
            clr->setBorderWidth(1);
            clr->setBorderColor(borderCol);
            clr->callback(clearDrumMapCb, this);

            const int fbLabelX  = clrX + drumClearW + drumBtnGap;
            const int fbChoiceX = fbLabelX + drumFallbackLabelW;
            fbLbl = new Fl_Box(fbLabelX, drumBtnY, drumFallbackLabelW, drumBtnH, "Fallback");
            fbLbl->box(FL_NO_BOX);
            fbLbl->labelcolor(subTextCol);
            fbLbl->labelsize(11);
            fbLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

            auto* fbMc = new ModernChoice(fbChoiceX, drumBtnY, drumFallbackChoiceW, drumBtnH);
            fbMc->color(inputBgCol);
            fbMc->labelcolor(textCol);
            fbMc->textsize(11);
            fbMc->setBorderColor(borderCol);
            fbMc->setArrowColor(subTextCol);
            fbMc->setHoverColor(0xF3F4F600);
            fbMc->add("Notes");
            fbMc->add("Numbers");
            fbMc->value(channels_[i].fallbackNoteNames ? 0 : 1);
            fbMc->callback(fallbackChoiceCb, this);
            fbCh = fbMc;
        }

        // Bank sub-row — above program row, all channels
        const int bankSubY = y + rowH + (drum ? drumRowH : 0);
        const int bankWidY = bankSubY + (progRowH - progBtnH) / 2;

        int bx = pad + chanGap;
        auto* bankLbl = new Fl_Box(bx, bankWidY, 90, progBtnH, "Bank:");
        bankLbl->box(FL_NO_BOX);
        bankLbl->labelcolor(subTextCol);
        bankLbl->labelsize(11);
        bankLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        bx += 90 + 4;

        auto* msbLbl = new Fl_Box(bx, bankWidY, 28, progBtnH, "MSB");
        msbLbl->box(FL_NO_BOX);
        msbLbl->labelcolor(subTextCol);
        msbLbl->labelsize(11);
        msbLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        bx += 28 + 4;

        auto* msbInp = new NameInput(bx, bankWidY, 38, progBtnH);
        msbInp->color(inputBgCol);
        msbInp->textcolor(textCol);
        msbInp->textsize(11);
        msbInp->callback(bankMsbInputCb, this);
        if (channels_[i].bankMsb >= 0)
            msbInp->value(std::to_string(channels_[i].bankMsb).c_str());
        bx += 38 + 12;

        auto* lsbLbl = new Fl_Box(bx, bankWidY, 28, progBtnH, "LSB");
        lsbLbl->box(FL_NO_BOX);
        lsbLbl->labelcolor(subTextCol);
        lsbLbl->labelsize(11);
        lsbLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        bx += 28 + 4;

        auto* lsbInp = new NameInput(bx, bankWidY, 38, progBtnH);
        lsbInp->color(inputBgCol);
        lsbInp->textcolor(textCol);
        lsbInp->textsize(11);
        lsbInp->callback(bankLsbInputCb, this);
        if (channels_[i].bankLsb >= 0)
            lsbInp->value(std::to_string(channels_[i].bankLsb).c_str());

        // Program / instrument sub-row — all channels
        const int progSubY = bankSubY + progRowH;
        const int progWidY = progSubY + (progRowH - progBtnH) / 2;

        int px = pad + typeLabelW + chanGap;
        auto* progLbl = new Fl_Box(px, progWidY, 90, progBtnH, "Program number");
        progLbl->box(FL_NO_BOX);
        progLbl->labelcolor(subTextCol);
        progLbl->labelsize(11);
        progLbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        px += 90 + 4;

        auto* progInp = new NameInput(px, progWidY, 40, progBtnH);
        progInp->color(inputBgCol);
        progInp->textcolor(textCol);
        progInp->textsize(11);
        progInp->callback(programInputCb, this);
        if (channels_[i].programNumber >= 0)
            progInp->value(std::to_string(channels_[i].programNumber + 1).c_str());
        px += 40 + 12;

        auto* gm1Lbl = new Fl_Box(px, progWidY, 125, progBtnH, "Set to GM1 instrument");
        gm1Lbl->box(FL_NO_BOX);
        gm1Lbl->labelcolor(subTextCol);
        gm1Lbl->labelsize(11);
        gm1Lbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);
        px += 125 + 4;

        auto* progDrop = new ModernChoice(px, progWidY, 170, progBtnH);
        progDrop->color(inputBgCol);
        progDrop->labelcolor(textCol);
        progDrop->textsize(11);
        progDrop->setBorderColor(borderCol);
        progDrop->setArrowColor(subTextCol);
        progDrop->setHoverColor(0xF3F4F600);
        progDrop->add("(none)");
        for (int n = 0; n < 128; n++) {
            std::string item = std::to_string(n + 1) + " \xe2\x80\x93 " + kGm1Names[n];
            progDrop->add(item.c_str());
        }
        progDrop->value(channels_[i].gm1Instrument >= 0 ? channels_[i].gm1Instrument + 1 : 0);
        progDrop->callback(programDropdownCb, this);

        chanRows_.push_back({typeLbl, nameInp, portCh, midiCh, del, imp, gm, gs, exp, clr, fbLbl, fbCh,
                             progInp, progDrop, msbInp, lsbInp, channels_[i].name});
        y += rowH + (drum ? drumRowH : 0) + 2 * progRowH;
    }
    end();

    addChanBtn->position(w() - pad - addBtnW, y + addBtnPad);
    addDrumChanBtn->position(w() - pad - 2*addBtnW - chanGap, y + addBtnPad);
    redraw();
}

void ConnectionsOverlay::rebuildPortChoices() {
    for (int i = 0; i < (int)chanRows_.size() && i < (int)channels_.size(); i++) {
        auto* ch = chanRows_[i].portChoice;
        if (!ch) continue;
        ch->clear();
        int selIdx = 0;
        for (int j = 0; j < (int)connections_.size(); j++) {
            ch->add(connections_[j].portName.c_str());
            if (connections_[j].portName == channels_[i].portName) selIdx = j;
        }
        if (!connections_.empty()) {
            ch->value(selIdx);
            channels_[i].portName = connections_[selIdx].portName;
        }
    }
    // Update delete button state for each port.
    for (int i = 0; i < (int)rows_.size() && i < (int)connections_.size(); i++) {
        if (!rows_[i].deleteBtn) continue;
        const std::string& pname = connections_[i].portName;
        bool referenced = std::any_of(channels_.begin(), channels_.end(),
            [&](const Channel& ch){ return ch.portName == pname; });
        if (referenced) rows_[i].deleteBtn->deactivate();
        else            rows_[i].deleteBtn->activate();
    }
    redraw();
}

// ── Static callbacks ──────────────────────────────────────────────────────────

void ConnectionsOverlay::inputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->rows_.size(); i++) {
        if (w != self->rows_[i].input) continue;
        std::string newName = static_cast<Fl_Input*>(w)->value();
        const std::string oldName = self->rows_[i].committedName;
        if (newName.empty() || newName == oldName) return;
        newName = self->uniquePortName(newName, i);
        static_cast<Fl_Input*>(w)->value(newName.c_str());
        self->rows_[i].committedName   = newName;
        self->connections_[i].portName = newName;
        for (auto& ch : self->channels_)
            if (ch.portName == oldName) ch.portName = newName;
        if (self->onPortRenamed) self->onPortRenamed(oldName, newName);
        self->rebuildPortChoices();
        return;
    }
}

void ConnectionsOverlay::deleteCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->rows_.size(); i++) {
        if (w != self->rows_[i].deleteBtn) continue;
        const std::string name = self->rows_[i].committedName;
        self->connections_.erase(self->connections_.begin() + i);
        self->rebuildRows();
        if (self->onPortRemoved) self->onPortRemoved(name);
        return;
    }
}

void ConnectionsOverlay::chanNameCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].nameInput) continue;
        std::string newName = static_cast<Fl_Input*>(w)->value();
        const std::string oldName = self->chanRows_[i].committedName;
        if (newName.empty()) {
            static_cast<Fl_Input*>(w)->value(oldName.c_str());
            return;
        }
        if (newName == oldName) return;
        newName = self->uniqueChanName(newName, i);
        static_cast<Fl_Input*>(w)->value(newName.c_str());
        self->chanRows_[i].committedName = newName;
        self->channels_[i].name = newName;
        if (self->onChannelRenamed) self->onChannelRenamed(oldName, newName);
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::refreshChannelButtons() {
    int drumCount = 0, stdCount = 0;
    for (const auto& ch : channels_) ch.isDrum ? ++drumCount : ++stdCount;
    for (int i = 0; i < (int)chanRows_.size() && i < (int)channels_.size(); i++) {
        if (!chanRows_[i].deleteBtn) continue;
        bool inUse = isChannelInUse && isChannelInUse(channels_[i].name);
        int typeCount = channels_[i].isDrum ? drumCount : stdCount;
        if (typeCount <= 1 || inUse) chanRows_[i].deleteBtn->deactivate();
        else                         chanRows_[i].deleteBtn->activate();
    }
    redraw();
}

void ConnectionsOverlay::chanDeleteCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].deleteBtn) continue;
        if (self->isChannelInUse && self->isChannelInUse(self->channels_[i].name)) return;
        bool isDrum = self->channels_[i].isDrum;
        int typeCount = 0;
        for (const auto& ch : self->channels_) if (ch.isDrum == isDrum) ++typeCount;
        if (typeCount <= 1) return;
        self->channels_.erase(self->channels_.begin() + i);
        self->rebuildChannelRows();
        self->rebuildPortChoices();
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::portChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].portChoice) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx >= 0 && idx < (int)self->connections_.size())
            self->channels_[i].portName = self->connections_[idx].portName;
        self->rebuildPortChoices();
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::midiChanChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].midiChanChoice) continue;
        self->channels_[i].midiChannel = static_cast<Fl_Choice*>(w)->value() + 1;
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::importDrumMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].importBtn) continue;
        Fl_Native_File_Chooser fc;
        fc.title("Import Drum Mappings");
        fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
        fc.filter("MIDNAM Files\t*.midnam\nAll Files\t*");
        if (fc.show() != 0) return;
        const char* path = fc.filename();
        if (!path || !path[0]) return;
        self->channels_[i].drumMap = parseMidnam(path);
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::loadGmMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].gmBtn) continue;
        self->channels_[i].drumMap = gmPercussionMap();
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::loadGsMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].gsBtn) continue;
        self->channels_[i].drumMap = gsPercussionMap();
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::exportDrumMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].exportBtn) continue;
        Fl_Native_File_Chooser fc;
        fc.title("Export Drum Mappings");
        fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        fc.filter("MIDNAM Files\t*.midnam\nAll Files\t*");
        fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        if (fc.show() != 0) return;
        std::string path = fc.filename();
        if (path.empty()) return;
        if (path.size() < 7 || path.substr(path.size() - 7) != ".midnam")
            path += ".midnam";
        exportMidnam(path, self->channels_[i].drumMap, self->channels_[i].name);
        return;
    }
}

void ConnectionsOverlay::clearDrumMapCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].clearBtn) continue;
        self->channels_[i].drumMap.clear();
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

void ConnectionsOverlay::fallbackChoiceCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].fallbackChoice) continue;
        // index 0 = "Notes" (show note names), index 1 = "Numbers" (show MIDI numbers)
        self->channels_[i].fallbackNoteNames = (static_cast<Fl_Choice*>(w)->value() == 0);
        if (self->onChannelsChanged) self->onChannelsChanged();
        return;
    }
}

// ── Program / bank callbacks ──────────────────────────────────────────────────

void ConnectionsOverlay::programInputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].programInput) continue;
        int v = parseOptionalInt(static_cast<Fl_Input*>(w)->value(), 1, 128);
        self->channels_[i].programNumber = (v >= 0) ? v - 1 : -1;
        self->channels_[i].gm1Instrument = -1;
        if (self->chanRows_[i].programDropdown)
            self->chanRows_[i].programDropdown->value(0);
        if (self->onProgramChanged) self->onProgramChanged(self->channels_[i].name);
        return;
    }
}

void ConnectionsOverlay::programDropdownCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].programDropdown) continue;
        int idx = static_cast<Fl_Choice*>(w)->value();
        if (idx <= 0) {
            self->channels_[i].programNumber = -1;
            self->channels_[i].gm1Instrument = -1;
            if (self->chanRows_[i].programInput) self->chanRows_[i].programInput->value("");
        } else {
            self->channels_[i].programNumber = idx - 1;
            self->channels_[i].gm1Instrument = idx - 1;
            if (self->chanRows_[i].programInput)
                self->chanRows_[i].programInput->value(std::to_string(idx).c_str());
        }
        if (self->onProgramChanged) self->onProgramChanged(self->channels_[i].name);
        return;
    }
}

void ConnectionsOverlay::bankMsbInputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].bankMsbInput) continue;
        self->channels_[i].bankMsb = parseOptionalInt(static_cast<Fl_Input*>(w)->value(), 0, 127);
        if (self->onProgramChanged) self->onProgramChanged(self->channels_[i].name);
        return;
    }
}

void ConnectionsOverlay::bankLsbInputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].bankLsbInput) continue;
        self->channels_[i].bankLsb = parseOptionalInt(static_cast<Fl_Input*>(w)->value(), 0, 127);
        if (self->onProgramChanged) self->onProgramChanged(self->channels_[i].name);
        return;
    }
}

// ── Focus navigation ──────────────────────────────────────────────────────────

std::vector<Fl_Widget*> ConnectionsOverlay::getFocusOrder() const {
    std::vector<Fl_Widget*> order;
    for (const auto& row : rows_) {
        if (row.input && row.input->active())     order.push_back(row.input);
        if (row.deleteBtn && row.deleteBtn->active()) order.push_back(row.deleteBtn);
    }
    order.push_back(addBtn);
    for (const auto& row : chanRows_) {
        if (row.nameInput && row.nameInput->active())       order.push_back(row.nameInput);
        if (row.portChoice && row.portChoice->active())     order.push_back(row.portChoice);
        if (row.midiChanChoice && row.midiChanChoice->active()) order.push_back(row.midiChanChoice);
        if (row.deleteBtn && row.deleteBtn->active())       order.push_back(row.deleteBtn);
    }
    order.push_back(addChanBtn);
    order.push_back(closeBtn);
    return order;
}

void ConnectionsOverlay::advanceFocusBy(int dir) {
    auto order = getFocusOrder();
    if (order.empty()) return;
    Fl_Widget* cur = Fl::focus();
    int idx = -1;
    for (int i = 0; i < (int)order.size(); i++)
        if (order[i] == cur) { idx = i; break; }
    int next = idx == -1 ? (dir > 0 ? 0 : (int)order.size() - 1)
                         : (idx + dir + (int)order.size()) % (int)order.size();
    Fl::focus(order[next]);
    order[next]->redraw();
}

int ConnectionsOverlay::handle(int event) {
    if (event == FL_KEYBOARD) {
        int k = Fl::event_key();
        if (k == FL_Tab) {
            advanceFocusBy(Fl::event_state() & FL_SHIFT ? -1 : 1);
            return 1;
        }
        if (k == FL_Enter || k == FL_KP_Enter) {
            advanceFocusBy(1);
            return 1;
        }
    }
    return BasePopup::handle(event);
}

// ── Drawing ───────────────────────────────────────────────────────────────────

void ConnectionsOverlay::draw() {
    if (damage() & ~FL_DAMAGE_CHILD) {
        fl_color(bgCol);
        fl_rectf(0, 0, w(), h());

        fl_color(borderCol);
        fl_line_style(FL_SOLID, 1);
        fl_rect(0, 0, w(), h());
        fl_line_style(0);

        fl_color(dividerCol);
        fl_line_style(FL_SOLID, 1);
        fl_line(0, headerH, w(), headerH);
        fl_line_style(0);

        fl_font(FL_HELVETICA_BOLD, 17);
        fl_color(textCol);
        fl_draw("Connections", titlePad, 0, w() - 2*titlePad, headerH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        fl_font(FL_HELVETICA_BOLD, 13);
        fl_color(textCol);
        fl_draw("MIDI Output Ports", titlePad, headerH + 12,
                w() - 2*titlePad, sectionH - 12, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        const int colY = headerH + sectionH;
        fl_font(FL_HELVETICA, 10);
        fl_color(subTextCol);
        fl_draw("PORT NAME", pad, colY,
                w() - 2*pad, colH, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        fl_color(dividerCol);
        fl_line_style(FL_SOLID, 1);
        fl_line(pad, rowsTopY, w() - pad, rowsTopY);
        fl_line_style(0);

        // ── Channel section ──────────────────────────────────────────────────
        if (chanSectionTopY_ > 0) {
            fl_color(dividerCol);
            fl_line_style(FL_SOLID, 1);
            fl_line(0, chanSectionTopY_, w(), chanSectionTopY_);
            fl_line_style(0);

            fl_font(FL_HELVETICA_BOLD, 13);
            fl_color(textCol);
            fl_draw("Channels", titlePad, chanSectionTopY_ + 12,
                    w() - 2*titlePad, chanSecH - 12, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

            const int chanColY = chanRowsTopY_ - chanColH2;
            const int nameX    = pad + typeLabelW + chanGap;
            const int portX    = nameX + chanNameW_ + chanGap;
            const int midiX    = portX + chanPortW_ + chanGap;
            fl_font(FL_HELVETICA, 10);
            fl_color(subTextCol);
            fl_draw("TYPE",            pad,   chanColY, typeLabelW, chanColH2, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            fl_draw("NAME",            nameX, chanColY, chanNameW_, chanColH2, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            fl_draw("MIDI OUTPUT PORT", portX, chanColY, chanPortW_, chanColH2, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            fl_draw("MIDI CH",          midiX, chanColY, chanMidiW,  chanColH2, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

            fl_color(dividerCol);
            fl_line_style(FL_SOLID, 1);
            fl_line(pad, chanRowsTopY_, w() - pad, chanRowsTopY_);
            fl_line_style(0);
        }
    }
    draw_children();
}
