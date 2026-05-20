#include <nexus/asset/SceneAsset.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace nexus::asset {

// ─────────────────────────────────────────────────────────────────────────────
//  Binary format layout (v1)
//
//  [Header]
//    uint32  magic         = kSceneAssetMagic  ("NXAS")
//    uint32  version       = kSceneAssetVersionCurrent
//    uint32  entryCount
//    uint32  sceneNameLen  (bytes, including null terminator)
//    char[]  sceneName
//
//  [Per entry, entryCount times]
//    uint32  nameLen
//    char[]  name
//    uint32  sourcePathLen
//    char[]  sourcePath
//    float3  translation
//    float4  rotation
//    float3  scale
//    uint8   visible
//    uint32  vertexCount
//    uint32  faceCount
//    float3 positions[vertexCount]
//    [per face]
//      uint32  indexCount
//      uint32  indices[indexCount]
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ── Write helpers ─────────────────────────────────────────────────────────────
bool writeU32(std::FILE* fp, uint32_t v) noexcept
{
    return std::fwrite(&v, sizeof(v), 1, fp) == 1;
}

bool writeF32(std::FILE* fp, float v) noexcept
{
    return std::fwrite(&v, sizeof(v), 1, fp) == 1;
}

bool writeU8(std::FILE* fp, uint8_t v) noexcept
{
    return std::fwrite(&v, sizeof(v), 1, fp) == 1;
}

bool writeString(std::FILE* fp, const std::string& s) noexcept
{
    const uint32_t len = static_cast<uint32_t>(s.size()) + 1u;  // include null
    if (!writeU32(fp, len)) { return false; }
    if (std::fwrite(s.c_str(), 1, len, fp) != len) { return false; }
    return true;
}

bool readBytes(std::FILE* fp, std::vector<uint8_t>& out) noexcept
{
    if (std::fseek(fp, 0, SEEK_END) != 0) { return false; }
    const long size = std::ftell(fp);
    if (size < 0) { return false; }
    if (std::fseek(fp, 0, SEEK_SET) != 0) { return false; }

    out.resize(static_cast<size_t>(size));
    if (out.empty()) { return true; }
    return std::fread(out.data(), 1, out.size(), fp) == out.size();
}

bool readU32At(const std::vector<uint8_t>& bytes, size_t& offset, uint32_t& out) noexcept
{
    if (offset + sizeof(uint32_t) > bytes.size()) { return false; }
    std::memcpy(&out, bytes.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    return true;
}

bool readF32At(const std::vector<uint8_t>& bytes, size_t& offset, float& out) noexcept
{
    if (offset + sizeof(float) > bytes.size()) { return false; }
    std::memcpy(&out, bytes.data() + offset, sizeof(float));
    offset += sizeof(float);
    return true;
}

bool readU8At(const std::vector<uint8_t>& bytes, size_t& offset, uint8_t& out) noexcept
{
    if (offset + sizeof(uint8_t) > bytes.size()) { return false; }
    out = bytes[offset];
    offset += sizeof(uint8_t);
    return true;
}

bool readStringAt(const std::vector<uint8_t>& bytes, size_t& offset, std::string& out) noexcept
{
    uint32_t len = 0u;
    if (!readU32At(bytes, offset, len) || len == 0u) { return false; }
    if (offset + static_cast<size_t>(len) > bytes.size()) { return false; }

    const char* str = reinterpret_cast<const char*>(bytes.data() + offset);
    out = std::string(str);
    offset += static_cast<size_t>(len);
    return true;
}

void appendU32(std::vector<uint8_t>& bytes, uint32_t value)
{
    const size_t prev = bytes.size();
    bytes.resize(prev + sizeof(uint32_t));
    std::memcpy(bytes.data() + prev, &value, sizeof(uint32_t));
}

void appendString(std::vector<uint8_t>& bytes, const std::string& value)
{
    const uint32_t len = static_cast<uint32_t>(value.size()) + 1u;
    appendU32(bytes, len);
    const size_t prev = bytes.size();
    bytes.resize(prev + static_cast<size_t>(len));
    std::memcpy(bytes.data() + prev, value.c_str(), static_cast<size_t>(len));
}

void writeU32At(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) noexcept
{
    if (offset + sizeof(uint32_t) > bytes.size()) { return; }
    std::memcpy(bytes.data() + offset, &value, sizeof(uint32_t));
}

std::map<uint32_t, SceneAsset::MigrationFn>& migrationRegistry() noexcept
{
    static std::map<uint32_t, SceneAsset::MigrationFn> registry;
    return registry;
}

bool migrateSceneAssetV0ToV1(uint32_t fromVersion, std::vector<uint8_t>& rawBytes)
{
    if (fromVersion != 0u) {
        return false;
    }

    size_t offset = 0u;
    uint32_t magic = 0u;
    uint32_t version = 0u;

    if (!readU32At(rawBytes, offset, magic)) {
        return false;
    }
    if (magic != kSceneAssetMagic) {
        return false;
    }
    if (!readU32At(rawBytes, offset, version)) {
        return false;
    }
    if (version != 0u) {
        return false;
    }

    // Legacy v0 header order:
    // [magic][version][sceneName][entryCount][entries...]
    std::string sceneName;
    if (!readStringAt(rawBytes, offset, sceneName)) {
        return false;
    }

    uint32_t entryCount = 0u;
    if (!readU32At(rawBytes, offset, entryCount)) {
        return false;
    }

    // Rewrite to v1 order:
    // [magic][version][entryCount][sceneName][entries...]
    std::vector<uint8_t> migrated;
    migrated.reserve(rawBytes.size() + 16u);
    appendU32(migrated, magic);
    appendU32(migrated, version);
    appendU32(migrated, entryCount);
    appendString(migrated, sceneName);

    if (offset <= rawBytes.size()) {
        migrated.insert(migrated.end(), rawBytes.begin() + static_cast<std::vector<uint8_t>::difference_type>(offset), rawBytes.end());
    }

    rawBytes = std::move(migrated);
    return true;
}

[[maybe_unused]] const bool kRegisteredSceneAssetV0Migration =
    SceneAsset::registerMigration(0u, migrateSceneAssetV0ToV1);

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Entry management
// ─────────────────────────────────────────────────────────────────────────────
void SceneAsset::addEntry(SceneMeshEntry entry)
{
    m_entries.push_back(std::move(entry));
}

void SceneAsset::removeEntry(size_t index) noexcept
{
    if (index < m_entries.size()) {
        m_entries.erase(m_entries.begin()
                        + static_cast<std::vector<SceneMeshEntry>::difference_type>(index));
    }
}

void SceneAsset::clear() noexcept
{
    m_entries.clear();
}

const SceneMeshEntry* SceneAsset::findByName(const std::string& name) const noexcept
{
    for (const auto& e : m_entries) {
        if (e.name == name) { return &e; }
    }
    return nullptr;
}

SceneMeshEntry* SceneAsset::findByName(const std::string& name) noexcept
{
    for (auto& e : m_entries) {
        if (e.name == name) { return &e; }
    }
    return nullptr;
}

bool SceneAsset::registerMigration(uint32_t fromVersion, MigrationFn fn) noexcept
{
    if (!fn) {
        return false;
    }
    migrationRegistry()[fromVersion] = std::move(fn);
    return true;
}

void SceneAsset::resetBuiltinMigrations() noexcept
{
    migrationRegistry().clear();
    migrationRegistry()[0u] = migrateSceneAssetV0ToV1;
}

bool SceneAsset::registerPackageMigration(uint32_t fromVersion,
                                          PackageMigrationFn fn) noexcept
{
    (void)fromVersion;
    (void)fn;
    return false;
}

void SceneAsset::resetBuiltinPackageMigrations() noexcept
{
}

SceneAssetPackageIOReport SceneAsset::savePackageManifest(
    const SceneAssetPackageDescriptor& package,
    const std::string& path) noexcept
{
    SceneAssetPackageIOReport report{};

    if (path.empty()) {
        report.diagnostic = SceneAssetPackageDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        report.diagnostic = SceneAssetPackageDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    auto fail = [&](const std::string& msg) {
        std::fclose(fp);
        report.diagnostic = SceneAssetPackageDiagnostic::WriteError;
        report.messages.push_back(msg);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    };

    const SceneAssetPackageMigrationModeFlags modeFlags =
        hasMigrationModeFlag(package.migrationPolicy.modeFlags,
                             SceneAssetPackageMigrationModeFlags::Lenient)
            ? SceneAssetPackageMigrationModeFlags::Lenient
            : SceneAssetPackageMigrationModeFlags::Strict;

    uint32_t targetVersion = package.migrationPolicy.targetVersion;
    if (targetVersion == 0u || targetVersion > kSceneAssetPackageVersionCurrent) {
        targetVersion = kSceneAssetPackageVersionCurrent;
    }

    if (!writeU32(fp, kSceneAssetPackageMagic)) {
        return fail("Write error: package magic");
    }
    if (!writeU32(fp, kSceneAssetPackageVersionCurrent)) {
        return fail("Write error: package version");
    }
    if (!writeU32(fp, targetVersion)) {
        return fail("Write error: package target version");
    }
    if (!writeU32(fp, static_cast<uint32_t>(modeFlags))) {
        return fail("Write error: package mode flags");
    }
    if (!writeU32(fp, static_cast<uint32_t>(package.entries.size()))) {
        return fail("Write error: package entry count");
    }

    std::set<std::string> seenEntryPaths;

    for (const SceneAssetPackageEntry& entry : package.entries) {
        if (entry.path.empty()) {
            return fail("Write error: package entry path is empty");
        }
        if (!seenEntryPaths.insert(entry.path).second) {
            return fail("Write error: duplicate package entry path");
        }
        if (!writeString(fp, entry.path)) {
            return fail("Write error: package entry path");
        }

        const std::string name = entry.name.empty() ? entry.path : entry.name;
        if (!writeString(fp, name)) {
            return fail("Write error: package entry name");
        }

        if (!writeU32(fp, static_cast<uint32_t>(entry.dependsOn.size()))) {
            return fail("Write error: package dependency count");
        }
        std::set<std::string> seenDependencies;
        for (const std::string& dep : entry.dependsOn) {
            if (dep.empty()) {
                return fail("Write error: package dependency path is empty");
            }
            if (dep == entry.path) {
                return fail("Write error: package dependency references its own path");
            }
            if (!seenDependencies.insert(dep).second) {
                return fail("Write error: duplicate package dependency path");
            }
            if (!writeString(fp, dep)) {
                return fail("Write error: package dependency path");
            }
        }
    }

    std::fclose(fp);
    report.valid = true;
    report.sourceVersion = kSceneAssetPackageVersionCurrent;
    report.version = kSceneAssetPackageVersionCurrent;
    report.targetVersion = targetVersion;
    report.modeFlags = modeFlags;
    report.entryCount = static_cast<uint32_t>(package.entries.size());
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

SceneAssetPackageIOReport SceneAsset::loadPackageManifest(
    const std::string& path,
    SceneAssetPackageDescriptor& outPackage,
    const SceneAssetPackageMigrationPolicy& policy) noexcept
{
    SceneAssetPackageIOReport report{};
    (void)policy;
    outPackage.entries.clear();
    outPackage.version = 0u;
    outPackage.migrationPolicy = {};

    if (path.empty()) {
        report.diagnostic = SceneAssetPackageDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        report.diagnostic = SceneAssetPackageDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for reading: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::vector<uint8_t> bytes;
    if (!readBytes(fp, bytes)) {
        std::fclose(fp);
        report.diagnostic = SceneAssetPackageDiagnostic::ReadError;
        report.messages.push_back("Read error: package file bytes");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    std::fclose(fp);

    auto fail = [&](SceneAssetPackageDiagnostic diag, const std::string& msg) {
        report.diagnostic = diag;
        report.messages.push_back(msg);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    };

    size_t offset = 0u;
    uint32_t magic = 0u;
    uint32_t version = 0u;
    uint32_t entryCount = 0u;
    uint32_t targetVersionMeta = kSceneAssetPackageVersionCurrent;
    SceneAssetPackageMigrationModeFlags modeFlagsMeta = SceneAssetPackageMigrationModeFlags::Strict;

    if (!readU32At(bytes, offset, magic)) {
        return fail(SceneAssetPackageDiagnostic::ReadError, "Read error: package magic");
    }
    if (magic != kSceneAssetPackageMagic) {
        return fail(SceneAssetPackageDiagnostic::InvalidMagic, "Invalid package magic bytes");
    }
    if (!readU32At(bytes, offset, version)) {
        return fail(SceneAssetPackageDiagnostic::ReadError, "Read error: package version");
    }
    report.sourceVersion = version;

    if (version != kSceneAssetPackageVersionCurrent) {
        return fail(SceneAssetPackageDiagnostic::UnsupportedVersion,
                    "Unsupported scene package version: " + std::to_string(version));
    }

    if (!readU32At(bytes, offset, targetVersionMeta)) {
        return fail(SceneAssetPackageDiagnostic::ReadError,
                    "Read error: package target version");
    }
    uint32_t rawModeFlags = 0u;
    if (!readU32At(bytes, offset, rawModeFlags)) {
        return fail(SceneAssetPackageDiagnostic::ReadError,
                    "Read error: package mode flags");
    }
    modeFlagsMeta = hasMigrationModeFlag(static_cast<SceneAssetPackageMigrationModeFlags>(rawModeFlags),
                                         SceneAssetPackageMigrationModeFlags::Lenient)
        ? SceneAssetPackageMigrationModeFlags::Lenient
        : SceneAssetPackageMigrationModeFlags::Strict;

    if (!readU32At(bytes, offset, entryCount)) {
        return fail(SceneAssetPackageDiagnostic::ReadError,
                    "Read error: package entry count");
    }

    outPackage.version = version;
    outPackage.migrationPolicy.targetVersion = targetVersionMeta;
    outPackage.migrationPolicy.modeFlags = modeFlagsMeta;
    outPackage.entries.reserve(entryCount);
    std::set<std::string> seenEntryPaths;

    for (uint32_t i = 0u; i < entryCount; ++i) {
        SceneAssetPackageEntry entry;
        if (!readStringAt(bytes, offset, entry.path)) {
            return fail(SceneAssetPackageDiagnostic::ReadError,
                        "Read error: package entry path");
        }
        if (entry.path.empty()) {
            return fail(SceneAssetPackageDiagnostic::InvalidData,
                        "Package entry path is empty");
        }
        if (!seenEntryPaths.insert(entry.path).second) {
            return fail(SceneAssetPackageDiagnostic::InvalidData,
                        "Package entry path is duplicated: " + entry.path);
        }
        if (!readStringAt(bytes, offset, entry.name)) {
            return fail(SceneAssetPackageDiagnostic::ReadError,
                        "Read error: package entry name");
        }
        if (entry.name.empty()) {
            return fail(SceneAssetPackageDiagnostic::InvalidData,
                        "Package entry name is empty");
        }

        uint32_t dependsCount = 0u;
        if (!readU32At(bytes, offset, dependsCount)) {
            return fail(SceneAssetPackageDiagnostic::ReadError,
                        "Read error: package dependency count");
        }

        entry.dependsOn.reserve(dependsCount);
        std::set<std::string> seenDependencies;
        for (uint32_t di = 0u; di < dependsCount; ++di) {
            std::string dep;
            if (!readStringAt(bytes, offset, dep)) {
                return fail(SceneAssetPackageDiagnostic::ReadError,
                            "Read error: package dependency path");
            }
            if (dep.empty()) {
                return fail(SceneAssetPackageDiagnostic::InvalidData,
                            "Package dependency path is empty");
            }
            if (dep == entry.path) {
                return fail(SceneAssetPackageDiagnostic::InvalidData,
                            "Package dependency path references its own entry path");
            }
            if (!seenDependencies.insert(dep).second) {
                return fail(SceneAssetPackageDiagnostic::InvalidData,
                            "Package dependency path is duplicated: " + dep);
            }
            entry.dependsOn.push_back(std::move(dep));
        }

        outPackage.entries.push_back(std::move(entry));
    }

    if (offset != bytes.size()) {
        return fail(SceneAssetPackageDiagnostic::InvalidData,
                    "Package bytes contain trailing payload");
    }

    report.valid = true;
    report.version = version;
    report.targetVersion = targetVersionMeta;
    report.modeFlags = modeFlagsMeta;
    report.entryCount = static_cast<uint32_t>(outPackage.entries.size());
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Binary save
// ─────────────────────────────────────────────────────────────────────────────
SceneAssetIOReport SceneAsset::save(const std::string& path) const noexcept
{
    SceneAssetIOReport report{};

    if (path.empty()) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    auto fail = [&](const std::string& msg) -> SceneAssetIOReport {
        std::fclose(fp);
        report.diagnostic = SceneAssetDiagnostic::WriteError;
        report.messages.push_back(msg);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    };

    // Header
    if (!writeU32(fp, kSceneAssetMagic))          { return fail("Write error: magic"); }
    if (!writeU32(fp, kSceneAssetVersionCurrent))  { return fail("Write error: version"); }
    if (!writeU32(fp, static_cast<uint32_t>(m_entries.size()))) {
        return fail("Write error: entry count");
    }
    if (!writeString(fp, m_sceneName))             { return fail("Write error: scene name"); }

    // Entries
    for (const SceneMeshEntry& entry : m_entries) {
        if (!writeString(fp, entry.name))       { return fail("Write error: entry name"); }
        if (!writeString(fp, entry.sourcePath)) { return fail("Write error: source path"); }

        // Transform
        if (!writeF32(fp, entry.transform.translation.x)) { return fail("Write error: tx"); }
        if (!writeF32(fp, entry.transform.translation.y)) { return fail("Write error: ty"); }
        if (!writeF32(fp, entry.transform.translation.z)) { return fail("Write error: tz"); }
        if (!writeF32(fp, entry.transform.rotation.x))    { return fail("Write error: rx"); }
        if (!writeF32(fp, entry.transform.rotation.y))    { return fail("Write error: ry"); }
        if (!writeF32(fp, entry.transform.rotation.z))    { return fail("Write error: rz"); }
        if (!writeF32(fp, entry.transform.rotation.w))    { return fail("Write error: rw"); }
        if (!writeF32(fp, entry.transform.scale.x))       { return fail("Write error: sx"); }
        if (!writeF32(fp, entry.transform.scale.y))       { return fail("Write error: sy"); }
        if (!writeF32(fp, entry.transform.scale.z))       { return fail("Write error: sz"); }
        if (!writeU8 (fp, entry.visible ? 1u : 0u))       { return fail("Write error: visible"); }

        // Mesh geometry
        const auto& positions  = entry.mesh.attributes().positions();
        const size_t vCount    = positions.size();
        const size_t fCount    = entry.mesh.topology().faceCount();
        if (!writeU32(fp, static_cast<uint32_t>(vCount))) { return fail("Write error: vCount"); }
        if (!writeU32(fp, static_cast<uint32_t>(fCount))) { return fail("Write error: fCount"); }

        for (const auto& p : positions) {
            if (!writeF32(fp, p.x) || !writeF32(fp, p.y) || !writeF32(fp, p.z)) {
                return fail("Write error: position");
            }
        }

        for (size_t fi = 0; fi < fCount; ++fi) {
            const nexus::geometry::Face& face = entry.mesh.topology().face(fi);
            const uint32_t iCount = static_cast<uint32_t>(face.indices.size());
            if (!writeU32(fp, iCount)) { return fail("Write error: face index count"); }
            for (const uint32_t idx : face.indices) {
                if (!writeU32(fp, idx)) { return fail("Write error: face index"); }
            }
        }
    }

    std::fclose(fp);
    report.valid      = true;
    report.version    = kSceneAssetVersionCurrent;
    report.entryCount = static_cast<uint32_t>(m_entries.size());
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Binary load
// ─────────────────────────────────────────────────────────────────────────────
SceneAssetIOReport SceneAsset::load(const std::string& path) noexcept
{
    SceneAssetIOReport report{};
    m_entries.clear();
    m_sceneName.clear();

    if (path.empty()) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for reading: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::vector<uint8_t> bytes;
    if (!readBytes(fp, bytes)) {
        std::fclose(fp);
        report.diagnostic = SceneAssetDiagnostic::ReadError;
        report.messages.push_back("Read error: file bytes");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }
    std::fclose(fp);

    auto fail = [&](SceneAssetDiagnostic diag, const std::string& msg) -> SceneAssetIOReport {
        report.diagnostic = diag;
        report.messages.push_back(msg);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    };

    size_t offset = 0u;
    uint32_t magic = 0u;
    uint32_t version = 0u;

    if (!readU32At(bytes, offset, magic)) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: magic");
    }
    if (magic != kSceneAssetMagic) {
        return fail(SceneAssetDiagnostic::InvalidMagic, "Invalid magic bytes");
    }
    if (!readU32At(bytes, offset, version)) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: version");
    }
    if (version > kSceneAssetVersionCurrent) {
        return fail(SceneAssetDiagnostic::UnsupportedVersion,
                    "Unsupported scene asset version: " + std::to_string(version));
    }

    while (version < kSceneAssetVersionCurrent) {
        auto it = migrationRegistry().find(version);
        if (it == migrationRegistry().end()) {
            return fail(SceneAssetDiagnostic::UnsupportedVersion,
                        "Missing migration for scene asset version: " + std::to_string(version));
        }
        if (!it->second(version, bytes)) {
            return fail(SceneAssetDiagnostic::MigrationFailed,
                        "Scene migration failed from version: " + std::to_string(version));
        }
        ++version;
        writeU32At(bytes, sizeof(uint32_t), version);
    }

    offset = 0u;
    uint32_t nEntries = 0u;
    if (!readU32At(bytes, offset, magic)) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: magic");
    }
    if (!readU32At(bytes, offset, version)) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: version");
    }
    if (!readU32At(bytes, offset, nEntries)) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: entry count");
    }
    if (!readStringAt(bytes, offset, m_sceneName)) {
        return fail(SceneAssetDiagnostic::ReadError, "Read error: scene name");
    }

    report.version = version;

    m_entries.reserve(nEntries);
    for (uint32_t i = 0; i < nEntries; ++i) {
        SceneMeshEntry entry;

        if (!readStringAt(bytes, offset, entry.name)) {
            return fail(SceneAssetDiagnostic::ReadError, "Read error: entry name");
        }
        if (!readStringAt(bytes, offset, entry.sourcePath)) {
            return fail(SceneAssetDiagnostic::ReadError, "Read error: source path");
        }

        if (!readF32At(bytes, offset, entry.transform.translation.x)) { return fail(SceneAssetDiagnostic::ReadError, "Read error: tx"); }
        if (!readF32At(bytes, offset, entry.transform.translation.y)) { return fail(SceneAssetDiagnostic::ReadError, "Read error: ty"); }
        if (!readF32At(bytes, offset, entry.transform.translation.z)) { return fail(SceneAssetDiagnostic::ReadError, "Read error: tz"); }
        if (!readF32At(bytes, offset, entry.transform.rotation.x))    { return fail(SceneAssetDiagnostic::ReadError, "Read error: rx"); }
        if (!readF32At(bytes, offset, entry.transform.rotation.y))    { return fail(SceneAssetDiagnostic::ReadError, "Read error: ry"); }
        if (!readF32At(bytes, offset, entry.transform.rotation.z))    { return fail(SceneAssetDiagnostic::ReadError, "Read error: rz"); }
        if (!readF32At(bytes, offset, entry.transform.rotation.w))    { return fail(SceneAssetDiagnostic::ReadError, "Read error: rw"); }
        if (!readF32At(bytes, offset, entry.transform.scale.x))       { return fail(SceneAssetDiagnostic::ReadError, "Read error: sx"); }
        if (!readF32At(bytes, offset, entry.transform.scale.y))       { return fail(SceneAssetDiagnostic::ReadError, "Read error: sy"); }
        if (!readF32At(bytes, offset, entry.transform.scale.z))       { return fail(SceneAssetDiagnostic::ReadError, "Read error: sz"); }

        uint8_t visible = 1u;
        if (!readU8At(bytes, offset, visible)) {
            return fail(SceneAssetDiagnostic::ReadError, "Read error: visible");
        }
        entry.visible = (visible != 0u);

        uint32_t vCount = 0u;
        uint32_t fCount = 0u;
        if (!readU32At(bytes, offset, vCount)) { return fail(SceneAssetDiagnostic::ReadError, "Read error: vCount"); }
        if (!readU32At(bytes, offset, fCount)) { return fail(SceneAssetDiagnostic::ReadError, "Read error: fCount"); }

        std::vector<nexus::render::Vec3> positions(vCount);
        for (uint32_t vi = 0; vi < vCount; ++vi) {
            if (!readF32At(bytes, offset, positions[vi].x) ||
                !readF32At(bytes, offset, positions[vi].y) ||
                !readF32At(bytes, offset, positions[vi].z)) {
                return fail(SceneAssetDiagnostic::ReadError, "Read error: position");
            }
        }
        if (vCount > 0u) {
            entry.mesh.attributes().setPositions(std::move(positions));
        }

        for (uint32_t fi = 0; fi < fCount; ++fi) {
            uint32_t iCount = 0u;
            if (!readU32At(bytes, offset, iCount)) {
                return fail(SceneAssetDiagnostic::ReadError, "Read error: face iCount");
            }
            if (iCount > 64u) {
                return fail(SceneAssetDiagnostic::InvalidData,
                            "Face index count too large: " + std::to_string(iCount));
            }

            nexus::geometry::Face face;
            face.indices.resize(iCount);
            for (uint32_t j = 0; j < iCount; ++j) {
                if (!readU32At(bytes, offset, face.indices[j])) {
                    return fail(SceneAssetDiagnostic::ReadError, "Read error: face index");
                }
            }
            entry.mesh.topology().addFace(std::move(face));
        }

        m_entries.push_back(std::move(entry));
    }

    report.valid      = true;
    report.entryCount = static_cast<uint32_t>(m_entries.size());
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

SceneAssetPackageReport SceneAsset::loadPackage(
    const std::vector<SceneAssetPackageEntry>& packageEntries,
    std::map<std::string, SceneAsset>& outAssets) noexcept
{
    SceneAssetPackageReport report{};
    outAssets.clear();

    AssetDependencyGraph graph;
    for (const SceneAssetPackageEntry& entry : packageEntries) {
        if (entry.path.empty()) {
            report.messages.push_back("Package entry has empty path");
            continue;
        }
        if (!graph.addAsset(AssetNodeDesc{entry.path, entry.name})) {
            report.messages.push_back("Duplicate or invalid package asset path: " + entry.path);
        }
    }

    for (const SceneAssetPackageEntry& entry : packageEntries) {
        for (const std::string& dependencyPath : entry.dependsOn) {
            if (!graph.addDependency(entry.path, dependencyPath)) {
                report.messages.push_back("Invalid package dependency edge: "
                                          + entry.path + " -> " + dependencyPath);
            }
        }
    }

    report.dependencyReport = graph.resolveLoadOrder();
    report.loadOrder = report.dependencyReport.loadOrder;

    if (!report.dependencyReport.valid()) {
        report.valid = false;
        if (report.dependencyReport.status == DependencyResolutionStatus::CycleDetected) {
            report.messages.push_back("Package dependency graph contains cycle(s)");
        }
        if (report.dependencyReport.status == DependencyResolutionStatus::MissingDependency) {
            report.messages.push_back("Package dependency graph contains missing dependency target(s)");
        }
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    for (const std::string& path : report.loadOrder) {
        SceneAsset asset;
        const SceneAssetIOReport loadReport = asset.load(path);
        report.loadReports[path] = loadReport;

        if (!loadReport.valid) {
            report.failedPaths.push_back(path);
            continue;
        }

        report.loadedPaths.push_back(path);
        outAssets.emplace(path, std::move(asset));
    }

    report.valid = report.failedPaths.empty();
    if (!report.valid) {
        report.messages.push_back("One or more scene assets failed to load in package");
    }
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Text export (human-readable summary, not re-importable)
// ─────────────────────────────────────────────────────────────────────────────
SceneAssetIOReport SceneAsset::exportText(const std::string& path) const noexcept
{
    SceneAssetIOReport report{};

    if (path.empty()) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Path is empty");
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    std::FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        report.diagnostic = SceneAssetDiagnostic::FileOpenFailed;
        report.messages.push_back("Failed to open file for writing: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    }

    auto fail = [&]() -> SceneAssetIOReport {
        std::fclose(fp);
        report.diagnostic = SceneAssetDiagnostic::WriteError;
        report.messages.push_back("Write error while exporting text: " + path);
        std::sort(report.messages.begin(), report.messages.end());
        return report;
    };

    if (std::fprintf(fp, "# Nexus Scene Asset v%u\n", kSceneAssetVersionCurrent) < 0) return fail();
    if (std::fprintf(fp, "scene_name: %s\n", m_sceneName.c_str()) < 0) return fail();
    if (std::fprintf(fp, "entry_count: %zu\n\n", m_entries.size()) < 0) return fail();

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const SceneMeshEntry& e = m_entries[i];
        if (std::fprintf(fp, "[entry %zu]\n", i) < 0)                         return fail();
        if (std::fprintf(fp, "  name: %s\n",      e.name.c_str()) < 0)        return fail();
        if (std::fprintf(fp, "  source: %s\n",    e.sourcePath.c_str()) < 0)  return fail();
        if (std::fprintf(fp, "  visible: %d\n",   e.visible ? 1 : 0) < 0)    return fail();
        if (std::fprintf(fp, "  translation: %.6g %.6g %.6g\n",
                e.transform.translation.x,
                e.transform.translation.y,
                e.transform.translation.z) < 0) return fail();
        if (std::fprintf(fp, "  rotation: %.6g %.6g %.6g %.6g\n",
                e.transform.rotation.x,
                e.transform.rotation.y,
                e.transform.rotation.z,
                e.transform.rotation.w) < 0) return fail();
        if (std::fprintf(fp, "  scale: %.6g %.6g %.6g\n",
                e.transform.scale.x,
                e.transform.scale.y,
                e.transform.scale.z) < 0) return fail();
        if (std::fprintf(fp, "  vertices: %zu\n", e.mesh.attributes().vertexCount()) < 0) return fail();
        if (std::fprintf(fp, "  faces: %zu\n\n",  e.mesh.topology().faceCount()) < 0)     return fail();
    }

    std::fclose(fp);
    report.valid      = true;
    report.version    = kSceneAssetVersionCurrent;
    report.entryCount = static_cast<uint32_t>(m_entries.size());
    std::sort(report.messages.begin(), report.messages.end());
    return report;
}

} // namespace nexus::asset
