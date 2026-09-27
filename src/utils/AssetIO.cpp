#include "utils/AssetIO.h"
#include <cstdint>
#include <cstring>
#include <fstream>

namespace {
constexpr char kMagic[4] = { 'A', 'E', 'N', 'C' };
constexpr uint64_t kProjectSalt = 0xA572A17C0DEADBEEULL;

uint64_t SplitMix(uint64_t z) {
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

unsigned char KeyByte(uint64_t salt, size_t i) {
    uint64_t r = SplitMix(salt ^ (0x9E3779B97F4A7C15ULL * (i + 1)));
    return static_cast<unsigned char>((r >> ((i % 8) * 8)) & 0xFF);
}

void XorStream(uint64_t salt, std::vector<unsigned char>& data) {
    for (size_t i = 0; i < data.size(); i++) data[i] ^= KeyByte(salt, i);
}

bool ReadRaw(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    std::streamoff size = f.tellg();
    if (size < 0) return false;
    out.resize(static_cast<size_t>(size));
    f.seekg(0);
    if (size > 0) f.read(reinterpret_cast<char*>(out.data()), size);
    return f.good() || size == 0;
}
} // namespace

namespace AssetIO {

bool IsEncrypted(const std::vector<unsigned char>& bytes) {
    return bytes.size() >= 12 && std::memcmp(bytes.data(), kMagic, 4) == 0;
}

std::string Encrypt(const std::string& plain) {
    uint64_t salt = kProjectSalt ^ (static_cast<uint64_t>(plain.size()) * 0x100000001B3ULL);
    std::vector<unsigned char> data(plain.begin(), plain.end());
    XorStream(salt, data);
    std::string out;
    out.reserve(12 + data.size());
    out.append(kMagic, 4);
    out.append(reinterpret_cast<const char*>(&salt), 8);
    out.append(reinterpret_cast<const char*>(data.data()), data.size());
    return out;
}

bool ReadBytes(const std::string& path, std::vector<unsigned char>& out) {
    if (!ReadRaw(path, out)) return false;
    if (IsEncrypted(out)) {
        uint64_t salt = 0;
        std::memcpy(&salt, out.data() + 4, 8);
        std::vector<unsigned char> body(out.begin() + 12, out.end());
        XorStream(salt, body);
        out.swap(body);
    }
    return true;
}

std::vector<unsigned char> ReadBytes(const std::string& path) {
    std::vector<unsigned char> out;
    ReadBytes(path, out);
    return out;
}

std::string ReadAll(const std::string& path) {
    std::vector<unsigned char> bytes;
    if (!ReadBytes(path, bytes)) return std::string();
    return std::string(bytes.begin(), bytes.end());
}

bool WriteEncrypted(const std::string& path, const std::string& data) {
    const std::string enc = Encrypt(data);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f.write(enc.data(), static_cast<std::streamsize>(enc.size()));
    return f.good();
}

} // namespace AssetIO
