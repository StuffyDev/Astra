#include "utils/ConsoleLog.h"
#include <iostream>
#include <streambuf>
#include <cstring>

namespace {

std::deque<ConsoleLog::Entry> g_Entries;
int g_ErrorCount = 0;
constexpr size_t kMaxLines = 1000;

// Буфер, который пишет и в оригинальный поток, и в журнал (по строкам)
class TeeBuffer : public std::streambuf {
public:
    TeeBuffer(std::streambuf* orig, bool isError) : m_Orig(orig), m_IsError(isError) {}

protected:
    int overflow(int ch) override {
        if (ch != EOF) {
            m_Orig->sputc(static_cast<char>(ch));
            if (ch == '\n') FlushLine();
            else m_Line += static_cast<char>(ch);
            if (m_Line.size() > 4096) FlushLine();
        }
        return ch;
    }

    int sync() override {
        FlushLine();
        m_Orig->pubsync();
        return 0;
    }

private:
    void FlushLine() {
        if (!m_Line.empty()) {
            ConsoleLog::Push(m_Line, m_IsError);
            m_Line.clear();
        }
    }

    std::streambuf* m_Orig;
    bool m_IsError;
    std::string m_Line;
};

TeeBuffer* g_CoutTee = nullptr;
TeeBuffer* g_CerrTee = nullptr;

} // namespace

void ConsoleLog::Push(const std::string& line, bool isError) {
    g_Entries.push_back({ line, isError });
    if (isError) g_ErrorCount++;
    while (g_Entries.size() > kMaxLines) g_Entries.pop_front();
}

const std::deque<ConsoleLog::Entry>& ConsoleLog::Entries() {
    return g_Entries;
}

void ConsoleLog::Clear() {
    g_Entries.clear();
    g_ErrorCount = 0;
}

int ConsoleLog::ErrorCount() {
    return g_ErrorCount;
}

void ConsoleLog::CaptureStdio() {
    if (g_CoutTee) return;
    g_CoutTee = new TeeBuffer(std::cout.rdbuf(), false);
    g_CerrTee = new TeeBuffer(std::cerr.rdbuf(), true);
    std::cout.rdbuf(g_CoutTee);
    std::cerr.rdbuf(g_CerrTee);
}
