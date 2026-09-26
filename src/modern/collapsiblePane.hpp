// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <FL/Fl_Group.H>
#include <functional>
#include <string>

// A titled section that folds away to its heading.
//
// The heading strip — a rule across the top, a disclosure chevron, the title and an
// optional one-line summary after it ("3 ports") — is drawn by the pane itself and
// toggles it when clicked, or with Space while the pane has keyboard focus. Every
// child is body: collapsing hides them all, expanding shows them again, and a
// child added while collapsed starts hidden.
//
// The pane does not lay its body out. The owner creates the children at absolute
// positions below bodyY(), tells the pane how tall that makes the body, and stacks
// the panes one under another; height() is then kHeaderH collapsed, and kHeaderH +
// bodyHeight() expanded. The pane never resizes its children (resizable is null),
// so moving it — position(), or resize() to a new y — carries the body with it,
// which is what makes scrolling a stack of panes a matter of moving the panes.
//
// Colours default to the overlay palette; override them with the setters.
class CollapsiblePane : public Fl_Group {
public:
    static constexpr int kHeaderH = 44;

    CollapsiblePane(int x, int y, int w, const char* title);

    bool expanded() const { return expanded_; }
    // Shows or hides the body and resizes the pane to match. Fires the callback
    // only when the user toggles, not when this is called.
    void setExpanded(bool on);

    // Top of the body, just under the heading strip.
    int  bodyY() const { return y() + kHeaderH; }
    int  bodyHeight() const { return bodyH_; }
    // Records how tall the body is, and resizes the pane if it is expanded.
    void setBodyHeight(int h);

    void setTitle(std::string t)  { title_ = std::move(t); redraw(); }
    // Drawn after the title in `color` — a count, or a warning worth seeing even
    // while the pane is folded away.
    void setSummary(std::string s, Fl_Color color);
    void setSummary(std::string s) { setSummary(std::move(s), summaryDefaultCol_); }

    void setColors(Fl_Color bg, Fl_Color text, Fl_Color subText, Fl_Color rule);

    // Draws the body's non-widget content — column headings, labels, rules —
    // underneath the children. Only called while expanded.
    std::function<void()> drawBody;

protected:
    void draw() override;
    int  handle(int event) override;
    int  on_insert(Fl_Widget* w, int index) override;

private:
    void toggle();
    bool inHeader() const;

    std::string title_;
    std::string summary_;
    bool        expanded_ = true;
    bool        hover_    = false;
    int         bodyH_    = 0;

    Fl_Color bgCol_             = 0xFFFFFF00;
    Fl_Color textCol_           = 0x37415100;
    Fl_Color subTextCol_        = 0x6B728000;
    Fl_Color ruleCol_           = 0xE5E7EB00;
    Fl_Color summaryDefaultCol_ = 0x6B728000;
    Fl_Color summaryCol_        = 0x6B728000;
};
