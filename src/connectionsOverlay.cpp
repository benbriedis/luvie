#include "connectionsOverlay.hpp"
#include "modernButton.hpp"
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

// ── Colors ────────────────────────────────────────────────────────────────────

static constexpr Fl_Color bgCol      = 0xFFFFFF00;
static constexpr Fl_Color borderCol  = 0xCBD5E100;
static constexpr Fl_Color dividerCol = 0xE5E7EB00;
static constexpr Fl_Color textCol    = 0x37415100;
static constexpr Fl_Color subTextCol = 0x6B728000;
static constexpr Fl_Color inputBgCol = 0xF9FAFB00;
static constexpr Fl_Color delRedCol  = 0xEF444400;
static constexpr Fl_Color addBtnBg   = 0xF3F4F600;

// ── PortNameInput ─────────────────────────────────────────────────────────────

class PortNameInput : public Fl_Input {
public:
    PortNameInput(int x, int y, int w, int h) : Fl_Input(x, y, w, h) {}
    int handle(int event) override {
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

    addBtn = new ModernButton(0, 0, addBtnW, addBtnH, "+ Add Connection");
    addBtn->labelsize(12);
    addBtn->labelcolor(textCol);
    addBtn->color(addBtnBg);
    addBtn->setBorderWidth(1);
    addBtn->setBorderColor(borderCol);
    addBtn->callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<ConnectionsOverlay*>(d);
        std::string name = "midi_out_" + std::to_string(self->connections_.size() + 1);
        self->connections_.push_back({name});
        self->rebuildRows();
        if (self->onPortAdded) self->onPortAdded(name);
    }, this);

    end();

    // Default connection — caller must register this with JACK after wiring callbacks.
    connections_.push_back({"midi_out_1"});
    rebuildRows();
    hide();
}

// ── Public API ────────────────────────────────────────────────────────────────

void ConnectionsOverlay::hide() {
    syncFromInputs();
    // Apply any uncommitted renames (user typed but didn't press Enter)
    for (int i = 0; i < (int)rows_.size() && i < (int)connections_.size(); i++) {
        const std::string newName = connections_[i].portName;
        const std::string oldName = rows_[i].committedName;
        if (newName == oldName) continue;
        if (onPortRenamed) onPortRenamed(oldName, newName);
        // rows_ rebuilt on next show; committedName update not needed
    }
    if (onClose) onClose();
    BasePopup::hide();
}

void ConnectionsOverlay::setConnections(const std::vector<std::string>& portNames) {
    connections_.clear();
    for (const auto& n : portNames)
        if (!n.empty()) connections_.push_back({n});
    if (connections_.empty())
        connections_.push_back({"midi_out_1"});
    rebuildRows();
}

std::vector<std::string> ConnectionsOverlay::getConnections() const {
    std::vector<std::string> result;
    for (const auto& c : connections_)
        result.push_back(c.portName);
    return result;
}

// ── Private helpers ───────────────────────────────────────────────────────────

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

        auto* inp = new PortNameInput(pad, iy, inputW, inputH);
        inp->box(FL_BORDER_BOX);
        inp->color(inputBgCol);
        inp->textcolor(textCol);
        inp->textsize(12);
        inp->when(FL_WHEN_ENTER_KEY_ALWAYS);
        inp->value(connections_[i].portName.c_str());
        inp->callback(inputCb, this);

        auto* del = new ModernButton(
            w() - pad - delBtnSz, y + (rowH - delBtnSz) / 2,
            delBtnSz, delBtnSz, "\xc3\x97");
        del->labelsize(14);
        del->labelcolor(delRedCol);
        del->color(bgCol);
        del->setBorderWidth(0);
        if ((int)connections_.size() <= 1) del->deactivate();
        del->callback(deleteCb, this);

        rows_.push_back({inp, del, connections_[i].portName});
        y += rowH;
    }

    addBtn->position(w() - pad - addBtnW, y + addBtnPad);
    end();
    redraw();
}

// ── Static callbacks ──────────────────────────────────────────────────────────

void ConnectionsOverlay::inputCb(Fl_Widget* w, void* d) {
    auto* self = static_cast<ConnectionsOverlay*>(d);
    for (int i = 0; i < (int)self->rows_.size(); i++) {
        if (w != self->rows_[i].input) continue;
        const std::string newName = static_cast<Fl_Input*>(w)->value();
        const std::string oldName = self->rows_[i].committedName;
        if (newName.empty() || newName == oldName) return;
        self->rows_[i].committedName   = newName;
        self->connections_[i].portName = newName;
        if (self->onPortRenamed) self->onPortRenamed(oldName, newName);
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
        fl_draw("PORT NAME  (press Enter to rename)", pad, colY,
                w() - 2*pad, colH, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        fl_color(dividerCol);
        fl_line_style(FL_SOLID, 1);
        fl_line(pad, rowsTopY, w() - pad, rowsTopY);
        fl_line_style(0);
    }
    draw_children();
}
