// Минимальный JSON-парсер (glTF, игровые манифесты). Без зависимостей, только STL.
// Поддержка: объекты/массивы/строки (с \\uXXXX в UTF-8)/числа/bool/null.
#pragma once
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mini {

struct Json {
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string str;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;

    bool isNull() const { return kind == Kind::Null; }
    bool has(const std::string& key) const { return kind == Kind::Object && obj.count(key) > 0; }
    const Json& operator[](const std::string& key) const {
        static const Json kNull;
        auto it = obj.find(key);
        return it != obj.end() ? it->second : kNull;
    }
    const Json& operator[](size_t i) const {
        static const Json kNull;
        return i < arr.size() ? arr[i] : kNull;
    }
    size_t size() const { return kind == Kind::Array ? arr.size() : (kind == Kind::Object ? obj.size() : 0); }

    double asNumber(double def = 0.0) const {
        if (kind == Kind::Number) return number;
        if (kind == Kind::Bool) return boolean ? 1.0 : 0.0;
        return def;
    }
    int asInt(int def = 0) const { return kind == Kind::Number ? (int)llround(number) : def; }
    bool asBool(bool def = false) const {
        if (kind == Kind::Bool) return boolean;
        if (kind == Kind::Number) return number != 0.0;
        return def;
    }
    std::string asString(const std::string& def = "") const { return kind == Kind::String ? str : def; }

    // Массив чисел в виде std::vector<int>/size_t
    std::vector<int64_t> asIntList() const {
        std::vector<int64_t> out;
        if (kind != Kind::Array) return out;
        for (const auto& v : arr) out.push_back((int64_t)llround(v.asNumber()));
        return out;
    }

    static bool Parse(const std::string& text, Json& out, std::string* err = nullptr);

private:
    static const char* ParseValue(const char* p, const char* end, Json& out, std::string* err);
    static const char* ParseString(const char* p, const char* end, std::string& out, std::string* err);
    static const char* SkipWs(const char* p, const char* end) {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        return p;
    }
};

} // namespace mini
