#include "connectionsOverlay.hpp"
#include "modernButton.hpp"
#include "modernChoice.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl_Input.H>
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

static constexpr int chanSecH  = 44;
static constexpr int chanColH2 = 22;
static constexpr int chanGap   = 6;
static constexpr int chanMidiW = 70;

// ── Colors ────────────────────────────────────────────────────────────────────

static constexpr Fl_Color bgCol      = 0xFFFFFF00;
static constexpr Fl_Color borderCol  = 0xCBD5E100;
static constexpr Fl_Color dividerCol = 0xE5E7EB00;
static constexpr Fl_Color textCol    = 0x37415100;
static constexpr Fl_Color subTextCol = 0x6B728000;
static constexpr Fl_Color inputBgCol = 0xF9FAFB00;
static constexpr Fl_Color delRedCol  = 0xEF444400;
static constexpr Fl_Color addBtnBg   = 0xF3F4F600;

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
        const int midiCh = 1;
        const std::string name = self->nextLetterChanName();
        self->channels_.push_back({self->nextChanId_++, name, portName, midiCh});
        self->rebuildChannelRows();
        if (self->onChannelsChanged) self->onChannelsChanged();
    }, this);

    end();

    connections_.push_back({nextPortId_++, "midi_out_1"});
    channels_.push_back({nextChanId_++, "A", "midi_out_1", 1});
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
        channels_.push_back({nextChanId_++, ci.name, ci.portName, ci.midiChannel});
    rebuildChannelRows();
}

std::vector<ConnectionsOverlay::ChannelInfo> ConnectionsOverlay::getChannels() const {
    std::vector<ChannelInfo> result;
    for (const auto& ch : channels_)
        result.push_back({ch.id, ch.name, ch.portName, ch.midiChannel});
    return result;
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

std::string ConnectionsOverlay::nextLetterChanName() const {
    auto inUse = [&](const std::string& name) {
        for (const auto& ch : channels_)
            if (ch.name == name) return true;
        return false;
    };
    for (int round = 1; ; round++) {
        std::string suffix = round == 1 ? "" : std::to_string(round);
        for (char c = 'A'; c <= 'Z'; c++) {
            std::string name = std::string(1, c) + suffix;
            if (!inUse(name)) return name;
        }
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
        if (row.nameInput)    { remove(row.nameInput);    Fl::delete_widget(row.nameInput); }
        if (row.portChoice)   { remove(row.portChoice);   Fl::delete_widget(row.portChoice); }
        if (row.midiChanChoice){ remove(row.midiChanChoice); Fl::delete_widget(row.midiChanChoice); }
        if (row.deleteBtn)    { remove(row.deleteBtn);    Fl::delete_widget(row.deleteBtn); }
    }
    chanRows_.clear();

    const int usableW = w() - 2*pad - delBtnSz - 8 - 2*chanGap;
    const int remaining = usableW - chanMidiW;
    chanNameW_ = remaining * 45 / 100;
    chanPortW_ = remaining - chanNameW_;

    int y = chanRowsTopY_;

    begin();
    for (int i = 0; i < (int)channels_.size(); i++) {
        const int iy = y + (rowH - inputH) / 2;

        auto* nameInp = new NameInput(pad, iy, chanNameW_, inputH);
        nameInp->color(inputBgCol);
        nameInp->textcolor(textCol);
        nameInp->textsize(12);
        nameInp->value(channels_[i].name.c_str());
        nameInp->callback(chanNameCb, this);

        const int portX = pad + chanNameW_ + chanGap;
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

        if (channels_.size() <= 1 || (isChannelInUse && isChannelInUse(channels_[i].name)))
            del->deactivate();

        chanRows_.push_back({nameInp, portCh, midiCh, del, channels_[i].name});
        y += rowH;
    }
    end();

    addChanBtn->position(w() - pad - addBtnW, y + addBtnPad);
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
    for (int i = 0; i < (int)chanRows_.size() && i < (int)channels_.size(); i++) {
        if (!chanRows_[i].deleteBtn) continue;
        bool inUse = isChannelInUse && isChannelInUse(channels_[i].name);
        if (channels_.size() <= 1 || inUse) chanRows_[i].deleteBtn->deactivate();
        else                                chanRows_[i].deleteBtn->activate();
    }
    redraw();
}

void ConnectionsOverlay::chanDeleteCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->chanRows_.size(); i++) {
        if (w != self->chanRows_[i].deleteBtn) continue;
        if (self->isChannelInUse && self->isChannelInUse(self->channels_[i].name)) return;
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
            const int portX    = pad + chanNameW_ + chanGap;
            const int midiX    = portX + chanPortW_ + chanGap;
            fl_font(FL_HELVETICA, 10);
            fl_color(subTextCol);
            fl_draw("NAME",            pad,   chanColY, chanNameW_, chanColH2, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
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
