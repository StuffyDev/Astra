#include "utils/Json.h"
#include <cmath>
#include <cstdio>

namespace mini {

namespace {

void AppendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out.push_back((char)cp);
    } else if (cp < 0x800) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

bool Hex4(const char* p, const char* end, unsigned& out) {
    if (p + 4 > end) return false;
    out = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        unsigned v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return false;
        out = out * 16 + v;
    }
    return true;
}

} // namespace

const char* Json::ParseString(const char* p, const char* end, std::string& out, std::string* err) {
    if (p >= end || *p != '"') {
        if (err) *err = "ожидалась строка";
        return nullptr;
    }
    p++;
    out.clear();
    while (p < end) {
        char c = *p++;
        if (c == '"') return p;
        if (c != '\\') { out.push_back(c); continue; }
        if (p >= end) break;
        char e = *p++;
        switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned cp = 0;
                if (!Hex4(p, end, cp)) { if (err) *err = "плохой \\u"; return nullptr; }
                p += 4;
                // суррогатная пара
                if (cp >= 0xD800 && cp <= 0xDBFF && p + 6 <= end && p[0] == '\\' && p[1] == 'u') {
                    unsigned lo = 0;
                    if (Hex4(p + 2, end, lo)) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        p += 6;
                    }
                }
                AppendUtf8(out, cp);
                break;
            }
            default: out.push_back(e); break;
        }
    }
    if (err) *err = "незакрытая строка";
    return nullptr;
}

const char* Json::ParseValue(const char* p, const char* end, Json& out, std::string* err) {
    p = SkipWs(p, end);
    if (p >= end) { if (err) *err = "неожиданный конец"; return nullptr; }
    char c = *p;
    if (c == '{') {
        out.kind = Kind::Object;
        p = SkipWs(p + 1, end);
        if (p < end && *p == '}') return p + 1;
        while (p < end) {
            std::string key;
            p = ParseString(p, end, key, err);
            if (!p) return nullptr;
            p = SkipWs(p, end);
            if (p >= end || *p != ':') { if (err) *err = "ожидалось ':'"; return nullptr; }
            Json child;
            p = ParseValue(p + 1, end, child, err);
            if (!p) return nullptr;
            out.obj[key] = std::move(child);
            p = SkipWs(p, end);
            if (p < end && *p == ',') { p = SkipWs(p + 1, end); continue; }
            if (p < end && *p == '}') return p + 1;
            if (err) *err = "ожидался ',' или '}'";
            return nullptr;
        }
        if (err) *err = "незакрытый объект";
        return nullptr;
    }
    if (c == '[') {
        out.kind = Kind::Array;
        p = SkipWs(p + 1, end);
        if (p < end && *p == ']') return p + 1;
        while (p < end) {
            Json child;
            p = ParseValue(p, end, child, err);
            if (!p) return nullptr;
            out.arr.push_back(std::move(child));
            p = SkipWs(p, end);
            if (p < end && *p == ',') { p = SkipWs(p + 1, end); continue; }
            if (p < end && *p == ']') return p + 1;
            if (err) *err = "ожидался ',' или ']'";
            return nullptr;
        }
        if (err) *err = "незакрытый массив";
        return nullptr;
    }
    if (c == '"') {
        out.kind = Kind::String;
        return ParseString(p, end, out.str, err);
    }
    if (end - p >= 4 && p[0] == 't' && p[1] == 'r' && p[2] == 'u' && p[3] == 'e') {
        out.kind = Kind::Bool; out.boolean = true; return p + 4;
    }
    if (end - p >= 5 && p[0] == 'f' && p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e') {
        out.kind = Kind::Bool; out.boolean = false; return p + 5;
    }
    if (end - p >= 4 && p[0] == 'n' && p[1] == 'u' && p[2] == 'l' && p[3] == 'l') {
        out.kind = Kind::Null; return p + 4;
    }
    // число
    const char* start = p;
    if (*p == '-' || *p == '+') p++;
    bool digits = false;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' ||
                       ((*p == '-' || *p == '+') && (p[-1] == 'e' || p[-1] == 'E')))) {
        if (*p >= '0' && *p <= '9') digits = true;
        p++;
    }
    if (!digits) { if (err) *err = "не значение"; return nullptr; }
    out.kind = Kind::Number;
    out.number = strtod(std::string(start, p).c_str(), nullptr);
    return p;
}

bool Json::Parse(const std::string& text, Json& out, std::string* err) {
    const char* p = text.data();
    const char* end = p + text.size();
    p = ParseValue(p, end, out, err);
    if (!p) return false;
    p = SkipWs(p, end);
    if (p != end && err) { *err = "мусор после значения"; return false; }
    return true;
}

} // namespace mini
