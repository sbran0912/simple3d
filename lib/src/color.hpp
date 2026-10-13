#pragma once

#include <string>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::string;

// ====================================================================
// FARBE  (Port von lib-wgl.ts: parseColor + ColorState)
// ====================================================================

// ColorState ist eine RGBA-Farbe mit Werten 0..1.
struct ColorState {
    double R = 1;
    double G = 1;
    double B = 1;
    double A = 1;
};

// ParseColor parst in einen ColorState mit Werten 0..1.
//
// Formen:
//   - Hex-String        -> ("#rgb" / "#rrggbb")
//   - 1 Zahl            -> Graustufe 0..255
//   - 3 Zahlen          -> r,g,b 0..255
//   - 4 Zahlen          -> r,g,b,a 0..255
ColorState ParseColor(const string& hex);
ColorState ParseColor(double gray);
ColorState ParseColor(double r, double g, double b);
ColorState ParseColor(double r, double g, double b, double a);

// parseHex parst einen Hex-Farbstring in einen ColorState.
ColorState ParseHex(const string& s);

// colEq vergleicht zwei Farbzustaende exakt.
bool ColEq(const ColorState& a, const ColorState& b);

// ====================================================================
// COLORINPUT  (Ersatz fuer Go interface{}-Farbeingaben)
//
// Kapselt eine der ParseColor-Formen. Default (nicht gesetzt) bedeutet
// "nicht ueberschreiben" im Immediate-Mode.
// ====================================================================
struct ColorInput {
    bool Set = false;
    ColorState Color;

    ColorInput() = default;
    ColorInput(const char* s) : Set(true), Color(ParseHex(s)) {}
    ColorInput(const string& s) : Set(true), Color(ParseHex(s)) {}
    ColorInput(double gray) : Set(true), Color(ParseColor(gray)) {}
    ColorInput(double r, double g, double b) : Set(true), Color(ParseColor(r, g, b)) {}
    ColorInput(double r, double g, double b, double a)
        : Set(true), Color(ParseColor(r, g, b, a)) {}
};
