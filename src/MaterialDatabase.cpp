#include "MaterialDatabase.h"

#include "Assert.h"
#include "Log.h"

#define _WIN32_WINNT 0x0A00
#include <windows.h>

#include <fstream>
#include <new>

namespace apex {

MaterialDatabase* MaterialDatabase::s_instance = nullptr;

namespace {

// Returns the directory of the running executable so data paths work
// regardless of the current working directory at launch.
std::filesystem::path GetExecutableDirectory() {
    char buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
}

// Read a UTF-8 text file into a string. Empty on failure.
std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

} // anonymous namespace

bool MaterialDatabase::Init() {
    if (s_instance != nullptr) {
        return true;
    }

    s_instance = new (std::nothrow) MaterialDatabase();
    if (s_instance == nullptr) {
        LOG_FATAL("MaterialDB", "Failed to allocate MaterialDatabase.");
        return false;
    }

    const std::filesystem::path elementsDir = GetExecutableDirectory() / "data" / "elements";
    s_instance->LoadElementDirectory(elementsDir);

    LOG_INFO("MaterialDB", "Loaded {} element(s).", s_instance->m_elements.size());
    return true;
}

void MaterialDatabase::Shutdown() {
    delete s_instance;
    s_instance = nullptr;
}

MaterialDatabase& MaterialDatabase::Get() {
    APEX_ASSERT(s_instance != nullptr, "MaterialDatabase::Get() called before Init().");
    return *s_instance;
}

const Element* MaterialDatabase::GetElement(std::string_view symbol) const {
    auto it = m_elements.find(std::string(symbol));
    if (it == m_elements.end()) {
        return nullptr;
    }
    return &it->second;
}

bool MaterialDatabase::LoadElementDirectory(const std::filesystem::path& directory) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        LOG_WARN("MaterialDB", "Element directory not found: {}", directory.string());
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string ext = entry.path().extension().string();
        if (ext != ".json" && ext != ".json5") {
            continue;
        }
        LoadElementFile(entry.path());
    }
    return true;
}

bool MaterialDatabase::LoadElementFile(const std::filesystem::path& path) {
    const std::string content = ReadTextFile(path);
    if (content.empty()) {
        LOG_ERROR("MaterialDB", "Failed to read element file: {}", path.string());
        return false;
    }

    nlohmann::json doc = nlohmann::json::parse(content,
                                               /*cb*/ nullptr,
                                               /*allow_exceptions*/ false,
                                               /*ignore_comments*/ true);
    if (doc.is_discarded()) {
        LOG_ERROR("MaterialDB", "Parse error in {}.", path.string());
        return false;
    }

    if (!doc.is_object()) {
        LOG_ERROR("MaterialDB", "{} top-level must be an object.", path.string());
        return false;
    }

    // Required fields — check presence and type before .get<>() to avoid
    // any chance of nlohmann throwing inside a -fno-exceptions binary.
    if (!doc.contains("symbol") || !doc["symbol"].is_string() || !doc.contains("name") ||
        !doc["name"].is_string() || !doc.contains("atomic_number") ||
        !doc["atomic_number"].is_number_integer() || !doc.contains("atomic_mass") ||
        !doc["atomic_mass"].is_number()) {
        LOG_ERROR("MaterialDB", "{} missing or invalid required field(s).", path.string());
        return false;
    }

    Element e;
    e.symbol = doc["symbol"].get<std::string>();
    e.name = doc["name"].get<std::string>();
    e.atomicNumber = doc["atomic_number"].get<u32>();
    e.atomicMass = doc["atomic_mass"].get<f32>();
    e.properties = doc;
    // Strip the typed-out fields from the property bag so subsystems
    // don't accidentally re-query them through the bag.
    e.properties.erase("symbol");
    e.properties.erase("name");
    e.properties.erase("atomic_number");
    e.properties.erase("atomic_mass");

    if (m_elements.contains(e.symbol)) {
        LOG_INFO("MaterialDB", "Overriding element {} from {}", e.symbol, path.string());
    }
    m_elements[e.symbol] = std::move(e);
    return true;
}

} // namespace apex