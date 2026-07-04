#include "patternNames.hpp"

std::string PatternNames::makeUnique(const std::string& base, int startN) const
{
    for (int n = startN; ; n++) {
        std::string cand = base + " " + std::to_string(n);
        bool exists = false;
        for (const auto& p : data.patterns)
            if (p.name == cand) { exists = true; break; }
        if (!exists) return cand;
    }
}

std::string PatternNames::trim(const std::string& s)
{
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string PatternNames::stem(const std::string& name)
{
    std::string s = name.empty() ? "Pattern" : name;
    auto sp = s.find_last_of(' ');
    if (sp != std::string::npos && sp + 1 < s.size() &&
        s.find_first_not_of("0123456789", sp + 1) == std::string::npos)
        s = s.substr(0, sp);
    return s;
}
