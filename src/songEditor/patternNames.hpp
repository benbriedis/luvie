#ifndef PATTERN_NAMES_HPP
#define PATTERN_NAMES_HPP

#include "timeline.hpp"
#include <string>
#include <vector>

// Single funnel for creating and changing pattern display names.
//
// All pattern-name assignment goes through this class so that names stay
// unique across the song. It binds to the song's live timeline, so every
// query reflects the current set of patterns (and instrument names). Nothing
// outside this class should write to Pattern::name directly.
class PatternNames {
public:
    explicit PatternNames(Timeline& data) : data(data) {}

    // Give a brand-new pattern (not yet added to the list) a unique auto name
    // based on its instrument, e.g. "Piano 1", "Piano 2". The pattern's
    // instrumentId must already be set. Falls back to "Pattern" when the
    // instrument has no name.
    void assignAuto(Pattern& p) const { p.name = makeUnique(autoBase(p), 1); }

    // Give a copy/clone a unique name derived from its source: "Name 2", then
    // "Name 3" etc. A trailing " <n>" on the source is stripped first so that
    // cloning "Bass 2" yields "Bass 3" rather than "Bass 2 2".
    void assignDerived(Pattern& p, const std::string& srcName) const {
        p.name = makeUnique(stem(srcName), 2);
    }

    // Apply a user-supplied rename. Returns true if the trimmed name was
    // accepted; false (leaving the name unchanged) if it was blank or collides
    // with another pattern.
    bool rename(Pattern& p, const std::string& proposed) const {
        std::string n = trim(proposed);
        if (n.empty() || collides(n, p.id)) return false;
        p.name = std::move(n);
        return true;
    }

    // Would this name collide with an existing pattern other than exceptId?
    // Used by the editors for live duplicate feedback while typing.
    bool collides(const std::string& name, int exceptId) const {
        std::string n = trim(name);
        for (const auto& p : data.patterns)
            if (p.id != exceptId && p.name == n) return true;
        return false;
    }

private:
    // Base name for a new pattern's auto name: its instrument name, or
    // "Pattern" when the instrument is unknown/unnamed. Default instrument
    // names of the form "Instrument X" are abbreviated to "Inst X" so that
    // patterns read "Inst A 1" rather than "Instrument A 1".
    std::string autoBase(const Pattern& p) const {
        std::string name = data.instrumentName(p.instrumentId);
        if (name.empty()) return "Pattern";
        const std::string prefix = "Instrument ";
        if (name.rfind(prefix, 0) == 0)
            return "Inst " + name.substr(prefix.size());
        return name;
    }

    // Lowest "base N" (N starting at startN) that no pattern currently uses.
    std::string makeUnique(const std::string& base, int startN) const;
    static std::string trim(const std::string& s);
    static std::string stem(const std::string& name);

    Timeline& data;
};

#endif
