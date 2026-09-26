// Журнал консоли редактора: строки движка, скриптов и ошибок.
// CaptureStdio() подменяет cout/cerr — всё, что пишется через них, попадает в панель Console.
#pragma once
#include <deque>
#include <string>

class ConsoleLog {
public:
    struct Entry {
        std::string text;
        bool error = false;
    };

    static void Push(const std::string& line, bool isError);
    static const std::deque<Entry>& Entries();
    static void Clear();
    static int ErrorCount();
    static void CaptureStdio();
};
