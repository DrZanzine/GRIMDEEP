// =============================================================================
//  core/paths.cpp — recherche de la racine du projet
// =============================================================================
#include "core/paths.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// Préfixe ajouté devant les chemins relatifs ("" si la racine est le dossier courant).
std::string gRootPrefix;

/// Dossier contenant l'exécutable en cours (vide en cas d'échec).
fs::path executableDir() {
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return fs::path();
    return fs::path(buf).parent_path();
}

/// Vrai si `dir` ressemble à la racine du projet (contient assets/shaders).
bool isProjectRoot(const fs::path& dir) {
    std::error_code ec;
    return !dir.empty() && fs::is_directory(dir / "assets" / "shaders", ec);
}

}  // namespace

void initPaths() {
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (isProjectRoot(cwd)) { gRootPrefix.clear(); return; }  // cas normal (run.bat)

    const fs::path exe = executableDir();
    const std::vector<fs::path> candidates = {exe, exe.parent_path(), exe.parent_path().parent_path()};
    for (const fs::path& c : candidates) {
        if (isProjectRoot(c)) { gRootPrefix = c.generic_string() + "/"; return; }
    }
    gRootPrefix.clear();  // introuvable : on tente le dossier courant (les erreurs le signaleront)
}

std::string assetPath(const std::string& relative) { return gRootPrefix + "assets/" + relative; }

std::string settingsPath() { return gRootPrefix + "settings.ini"; }
