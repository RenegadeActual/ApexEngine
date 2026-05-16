#pragma once

#include "Common.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

/// @file MaterialDatabase.h
/// @brief Loads and stores chemical elements (and later compounds and
/// other crafting materials) from data files.

namespace apex {

/// A single chemical element. Combines required identity fields with a
/// JSON property bag for everything else.
struct Element {
    /// Chemical symbol. Primary key. "H", "Fe", "Au".
    std::string symbol;

    /// Display name. "Hydrogen", "Iron".
    std::string name;

    /// Atomic number. 1 for hydrogen, 26 for iron.
    u32 atomicNumber = 0;

    /// Standard atomic mass in unified atomic mass units (u).
    f32 atomicMass = 0.0f;

    /// All other properties from the data file. Subsystems query specific
    /// keys on demand. The engine itself does not interpret these.
    nlohmann::json properties;
};

/// Singleton database of materials loaded from data files at startup.
///
/// Lifecycle:
/// @code
/// apex::MaterialDatabase::Init();
/// // ... gameplay code queries Get() ...
/// apex::MaterialDatabase::Shutdown();
/// @endcode
class MaterialDatabase {
public:
    // ---- Lifecycle ----

    /// Initialize the database. Loads data/elements/*.json5 relative to
    /// the executable's directory. Returns true on success.
    static bool Init();

    /// Tear down the database, releasing all loaded data.
    static void Shutdown();

    /// Access the singleton instance.
    /// @pre Init has been called and not followed by Shutdown.
    static MaterialDatabase& Get();

    /// @cond
    MaterialDatabase(const MaterialDatabase&) = delete;
    MaterialDatabase& operator=(const MaterialDatabase&) = delete;
    MaterialDatabase(MaterialDatabase&&) = delete;
    MaterialDatabase& operator=(MaterialDatabase&&) = delete;
    /// @endcond

    // ---- Element queries ----

    /// Look up an element by chemical symbol. Returns nullptr if not loaded.
    const Element* GetElement(std::string_view symbol) const;

    /// Number of elements currently loaded.
    size_t ElementCount() const { return m_elements.size(); }

    /// All loaded elements, keyed by symbol. For iteration.
    const std::unordered_map<std::string, Element>& AllElements() const { return m_elements; }

private:
    MaterialDatabase() = default;
    ~MaterialDatabase() = default;

    bool LoadElementDirectory(const std::filesystem::path& directory);
    bool LoadElementFile(const std::filesystem::path& path);

    static MaterialDatabase* s_instance;

    /// Loaded elements, keyed by chemical symbol.
    std::unordered_map<std::string, Element> m_elements;
};

} // namespace apex