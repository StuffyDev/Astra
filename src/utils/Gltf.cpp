#include "utils/Gltf.h"
#include "utils/AssetIO.h"
#include "utils/Json.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace Gltf {
namespace {

// ---- base64 (для data:-URI во внешних .gltf) ----
int B64Val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<uint8_t> Base64Decode(const std::string& in) {
    std::vector<uint8_t> out;
    out.reserve(in.size() / 4 * 3 + 3);
    int bucket = 0, bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        int v = B64Val(c);
        if (v < 0) continue;
        bucket = (bucket << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)((bucket >> bits) & 0xFF));
        }
    }
    return out;
}

std::string DirOf(const std::string& path) {
    fs::path p(path);
    return p.has_parent_path() ? p.parent_path().string() : std::string(".");
}

uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

struct Buf {
    std::vector<uint8_t> data;
};

// Чтение одного компонента аксессора (float-атрибуты и целые индексы)
double ComponentAt(const std::vector<uint8_t>& buf, size_t bytePos, int compType) {
    if (bytePos + 4 > buf.size() && compType != 5121 && compType != 5120 &&
        compType != 5123 && compType != 5122) return 0.0;
    const uint8_t* p = buf.data() + bytePos;
    switch (compType) {
        case 5126: { float f; std::memcpy(&f, p, 4); return f; }            // FLOAT
        case 5123: { uint16_t u; std::memcpy(&u, p, 2); return u; }          // UNSIGNED_SHORT
        case 5125: return ReadU32(p);                                        // UNSIGNED_INT
        case 5121: return p[0];                                              // UNSIGNED_BYTE
        case 5122: { int16_t s; std::memcpy(&s, p, 2); return s; }           // SHORT
        case 5120: return (int8_t)p[0];                                      // BYTE
        default: return 0.0;
    }
}

int CompSize(int compType) {
    switch (compType) {
        case 5126: case 5125: return 4;   // FLOAT, UNSIGNED_INT
        case 5123: case 5122: return 2;   // UNSIGNED_SHORT, SHORT
        case 5121: case 5120: return 1;   // UNSIGNED_BYTE, BYTE
        default: return 0;
    }
}

int TypeCount(const std::string& t) {
    if (t == "SCALAR") return 1;
    if (t == "VEC2") return 2;
    if (t == "VEC3") return 3;
    if (t == "VEC4") return 4;
    if (t == "MAT4") return 16;
    return 0;
}

// Атрибут аксессора как массив float (comps — сколько компонент брать из VECn)
bool ReadFloatArray(const mini::Json& root, const std::vector<Buf>& buffers, int accIndex,
                    int comps, std::vector<float>& out) {
    if (accIndex < 0 || comps <= 0) return false;
    const auto& accs = root["accessors"];
    if (accs.kind != mini::Json::Kind::Array || (size_t)accIndex >= accs.size()) return false;
    const mini::Json& acc = accs[(size_t)accIndex];
    int bvIndex = acc["bufferView"].asInt(-1);
    if (bvIndex < 0) return false;
    const auto& bvs = root["bufferViews"];
    if ((size_t)bvIndex >= bvs.size()) return false;
    const mini::Json& bv = bvs[(size_t)bvIndex];
    int bufIndex = bv["buffer"].asInt(0);
    if (bufIndex < 0 || (size_t)bufIndex >= buffers.size()) return false;
    const std::vector<uint8_t>& buf = buffers[(size_t)bufIndex].data;
    if (buf.empty()) return false;

    size_t viewOffset = (size_t)bv["byteOffset"].asInt(0);
    size_t stride = (size_t)bv["byteStride"].asInt(0);
    int n = acc["count"].asInt(0);
    int compType = acc["componentType"].asInt(5126);
    int elemCount = TypeCount(acc["type"].asString());
    int csize = CompSize(compType);
    if (elemCount == 0 || csize == 0 || n <= 0) return false;
    int want = std::min(comps, elemCount);
    if (stride == 0) stride = (size_t)elemCount * (size_t)csize;
    size_t accOffset = (size_t)acc["byteOffset"].asInt(0);
    bool normalized = acc.has("normalized") && acc["normalized"].asBool();

    out.reserve(out.size() + (size_t)n * want);
    for (int i = 0; i < n; i++) {
        size_t base = viewOffset + accOffset + (size_t)i * stride;
        for (int c = 0; c < want; c++) {
            size_t at = base + (size_t)c * (size_t)csize;
            if (at + (size_t)csize > buf.size()) return !out.empty();
            double v = ComponentAt(buf, at, compType);
            if (normalized) {
                if (compType == 5121) v /= 255.0;
                else if (compType == 5123) v /= 65535.0;
                else if (compType == 5120) v /= 127.0;
                else if (compType == 5122) v /= 32767.0;
            }
            out.push_back((float)v);
        }
    }
    return true;
}

// Индексы аксессора (SCALAR) в uint32
bool ReadIndexArray(const mini::Json& root, const std::vector<Buf>& buffers, int accIndex,
                    std::vector<uint32_t>& out) {
    std::vector<float> f;
    if (!ReadFloatArray(root, buffers, accIndex, 1, f)) return false;
    out.reserve(out.size() + f.size());
    for (float v : f) out.push_back((uint32_t)std::lround(v));
    return true;
}

std::string ResolveUri(const std::string& uri, const std::string& modelDir) {
    if (uri.rfind("data:", 0) == 0) return uri;
    std::string clean = uri;
    size_t q = clean.find('?');
    if (q != std::string::npos) clean = clean.substr(0, q);
    if (!clean.empty() && clean[0] == '/') clean.erase(0, 1);
    fs::path p = fs::path(modelDir) / clean;
    std::error_code ec;
    if (fs::exists(p, ec)) return p.string();
    return clean;   // как в файле — на случай относительного пути от проекта
}

struct Loaded {
    mini::Json root;
    std::vector<Buf> buffers;
    std::string modelDir;
    bool ok = false;
    std::string error;
};

Loaded LoadJsonAndBuffers(const std::string& path) {
    Loaded L;
    L.modelDir = DirOf(path);
    std::vector<uint8_t> raw = AssetIO::ReadBytes(path);
    if (raw.empty()) { L.error = "файл не найден или пуст"; return L; }

    std::string jsonText;
    std::vector<uint8_t> binChunk;

    if (raw.size() > 12 && ReadU32(raw.data()) == 0x46546C67u) {   // "glTF" — это .glb
        uint32_t version = ReadU32(raw.data() + 4);
        if (version != 2) { L.error = "поддерживается glTF 2.0"; return L; }
        size_t pos = 12;
        bool gotJson = false;
        while (pos + 8 <= raw.size()) {
            uint32_t len = ReadU32(raw.data() + pos);
            uint32_t type = ReadU32(raw.data() + pos + 4);
            pos += 8;
            if (pos + len > raw.size()) break;
            if (type == 0x4E4F534Au && !gotJson) {              // "JSON"
                jsonText.assign((const char*)raw.data() + pos, len);
                gotJson = true;
            } else if (type == 0x004E4942u) {                   // "BIN\0"
                binChunk.assign(raw.begin() + pos, raw.begin() + pos + len);
            }
            pos += len;
        }
        if (!gotJson) { L.error = "в .glb нет JSON-чанка"; return L; }
    } else {
        jsonText.assign((const char*)raw.data(), raw.size());
    }

    if (!L.root.Parse(jsonText, L.root, &L.error)) { L.error = "битый JSON: " + L.error; return L; }

    const auto& buffers = L.root["buffers"];
    for (size_t i = 0; i < buffers.size(); i++) {
        const mini::Json& b = buffers[i];
        Buf buf;
        std::string uri = b["uri"].asString();
        if (uri.empty()) {
            buf.data = binChunk;                              // GLB: buffer 0 ссылается на BIN
        } else if (uri.rfind("data:", 0) == 0) {
            size_t comma = uri.find(',');
            buf.data = Base64Decode(uri.substr(comma == std::string::npos ? uri.size() : comma + 1));
        } else {
            std::string full = ResolveUri(uri, L.modelDir);
            buf.data = AssetIO::ReadBytes(full);
            if (buf.data.empty()) { L.error = "не удалось прочитать буфер " + uri; return L; }
        }
        L.buffers.push_back(std::move(buf));
    }
    L.ok = true;
    return L;
}

void AppendPrimitive(const Loaded& L, const mini::Json& prim, std::vector<float>& verts,
                     std::vector<uint32_t>& idx) {
    const mini::Json& attrs = prim["attributes"];
    int posAcc = attrs["POSITION"].asInt(-1);
    if (posAcc < 0) return;
    std::vector<float> pos, nrm, uv;
    if (!ReadFloatArray(L.root, L.buffers, posAcc, 3, pos)) return;
    size_t count = pos.size() / 3;
    if (count == 0) return;
    ReadFloatArray(L.root, L.buffers, attrs["NORMAL"].asInt(-1), 3, nrm);
    ReadFloatArray(L.root, L.buffers, attrs["TEXCOORD_0"].asInt(-1), 2, uv);

    std::vector<uint32_t> indices;
    int idxAcc = prim["indices"].asInt(-1);
    if (idxAcc >= 0) ReadIndexArray(L.root, L.buffers, idxAcc, indices);
    if (indices.empty()) {
        indices.resize(count);
        for (size_t i = 0; i < count; i++) indices[i] = (uint32_t)i;
    }

    const uint32_t base = (uint32_t)(verts.size() / 8);
    for (size_t i = 0; i < count; i++) {
        glm::vec3 p(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]);
        glm::vec3 n = nrm.size() >= (i + 1) * 3 ? glm::vec3(nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2])
                                                : glm::vec3(0.0f, 1.0f, 0.0f);
        float u = uv.size() >= (i + 1) * 2 ? uv[i * 2] : 0.0f;
        float v = uv.size() >= (i + 1) * 2 ? uv[i * 2 + 1] : 0.0f;
        verts.insert(verts.end(), { p.x, p.y, p.z, n.x, n.y, n.z, u, v });
    }
    if (nrm.empty()) {
        // нормалей в файле нет — считаем плоские нормали по граням
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            uint32_t tri[3] = { base + indices[t], base + indices[t + 1], base + indices[t + 2] };
            glm::vec3 a(verts[tri[0] * 8], verts[tri[0] * 8 + 1], verts[tri[0] * 8 + 2]);
            glm::vec3 b(verts[tri[1] * 8], verts[tri[1] * 8 + 1], verts[tri[1] * 8 + 2]);
            glm::vec3 c(verts[tri[2] * 8], verts[tri[2] * 8 + 1], verts[tri[2] * 8 + 2]);
            glm::vec3 f = glm::cross(b - a, c - a);
            float len = glm::length(f);
            if (len < 1e-8f) continue;
            f /= len;
            for (uint32_t vi : tri) {
                verts[vi * 8 + 3] = f.x; verts[vi * 8 + 4] = f.y; verts[vi * 8 + 5] = f.z;
            }
        }
    }
    for (uint32_t i : indices) idx.push_back(base + i);
}

} // namespace

Data Load(const std::string& path) {
    Data out;
    Loaded L = LoadJsonAndBuffers(path);
    if (!L.ok) { out.error = L.error; return out; }

    const auto& meshes = L.root["meshes"];
    if (meshes.kind != mini::Json::Kind::Array || meshes.size() == 0) {
        out.error = "в файле нет mesh";
        return out;
    }
    // Берём первый mesh (или тот, на который ссылается первый узел с mesh)
    size_t meshIndex = 0;
    const auto& nodes = L.root["nodes"];
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i].has("mesh")) { meshIndex = (size_t)nodes[i]["mesh"].asInt(0); break; }
    }
    if (meshIndex >= meshes.size()) meshIndex = 0;

    const mini::Json& mesh = meshes[meshIndex];
    const mini::Json& prims = mesh["primitives"];
    for (size_t i = 0; i < prims.size(); i++)
        AppendPrimitive(L, prims[i], out.verts, out.idx);

    if (out.verts.empty() || out.idx.empty()) { out.error = "пустые вершины/индексы"; return out; }

    // базовая текстура материала (для подсказки в инспекторе)
    int matIdx = prims.size() ? prims[(size_t)0]["material"].asInt(-1) : -1;
    if (matIdx >= 0 && (size_t)matIdx < L.root["materials"].size()) {
        const mini::Json& tex = L.root["materials"][(size_t)matIdx]["pbrMetallicRoughness"]
                                            ["baseColorTexture"]["index"];
        if (tex.kind == mini::Json::Kind::Number && (size_t)tex.asInt() < L.root["textures"].size()) {
            const mini::Json& img = L.root["textures"][(size_t)tex.asInt()]["source"];
            if ((size_t)img.asInt(-1) < L.root["images"].size())
                out.baseColorPath = L.root["images"][(size_t)img.asInt()]["uri"].asString();
        }
    }
    out.ok = true;
    return out;
}

std::vector<std::string> CollectFiles(const std::string& path) {
    std::vector<std::string> files;
    files.push_back(path);
    std::string ext = fs::path(path).extension().string();
    for (auto& c : ext) c = (char)std::tolower(c);
    if (ext == ".glb") return files;            // GLB самодостаточен

    Loaded L = LoadJsonAndBuffers(path);
    if (!L.ok) return files;
    const auto& buffers = L.root["buffers"];
    for (size_t i = 0; i < buffers.size(); i++) {
        std::string uri = buffers[i]["uri"].asString();
        if (uri.empty() || uri.rfind("data:", 0) == 0) continue;
        files.push_back(ResolveUri(uri, L.modelDir));
    }
    return files;
}

} // namespace Gltf
