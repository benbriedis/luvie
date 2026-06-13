#pragma once

#include <FL/Fl.H>

#include <algorithm>

// Map a note velocity (0..1) to a fill colour that runs from a very light blue
// (low velocity) to a dark blue (high velocity). The reference blue 0x5555EE is
// hit at velocity 0.8 so existing notes keep their familiar colour.
inline Fl_Color velocityFill(float vel)
{
    vel = std::clamp(vel, 0.0f, 1.0f);
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    float r, g, b;
    if (vel <= 0.8f) {
        float t = vel / 0.8f;            // 0 -> light blue, 1 -> reference blue
        r = lerp(212.0f, 85.0f, t);
        g = lerp(216.0f, 85.0f, t);
        b = lerp(250.0f, 238.0f, t);
    } else {
        float t = (vel - 0.8f) / 0.2f;   // 0 -> reference blue, 1 -> dark blue
        r = lerp(85.0f, 12.0f, t);
        g = lerp(85.0f, 12.0f, t);
        b = lerp(238.0f, 150.0f, t);
    }

    int ri = (int)(r + 0.5f);
    int gi = (int)(g + 0.5f);
    int bi = (int)(b + 0.5f);
    return ((Fl_Color)ri << 24) | ((Fl_Color)gi << 16) | ((Fl_Color)bi << 8);
}

// A darker accent derived from the fill, used for the note's leading bar / rim.
// Matches the original 0x5555EE -> 0x1111EE relationship at velocity 0.8.
inline Fl_Color velocityAccent(float vel)
{
    Fl_Color f = velocityFill(vel);
    int r = std::max(0, (int)((f >> 24) & 0xFF) - 68);
    int g = std::max(0, (int)((f >> 16) & 0xFF) - 68);
    int b = (int)((f >> 8) & 0xFF);
    return ((Fl_Color)r << 24) | ((Fl_Color)g << 16) | ((Fl_Color)b << 8);
}
