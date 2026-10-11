// =============================================================================
//  game/settings.cpp — lecture / écriture des réglages, plein écran
// =============================================================================
#include "game/settings.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "core/paths.h"

void loadSettings(Settings& st) {
    FILE* f = std::fopen(settingsPath().c_str(), "r");
    if (!f) return;
    char k[64];
    float v;
    while (std::fscanf(f, " %63[^=]=%f", k, &v) == 2) {
        std::string key = k;
        if (key == "master") st.master = v;
        else if (key == "music") st.music = v;
        else if (key == "sfx") st.sfx = v;
        else if (key == "sens") st.sens = v;
        else if (key == "fullscreen") st.fullscreen = v > 0.5f;
    }
    std::fclose(f);
    st.master = std::min(1.0f, std::max(0.0f, st.master));
    st.music = std::min(1.0f, std::max(0.0f, st.music));
    st.sfx = std::min(1.0f, std::max(0.0f, st.sfx));
    st.sens = std::min(2.5f, std::max(0.2f, st.sens));
}

void saveSettings(const Settings& st) {
    FILE* f = std::fopen(settingsPath().c_str(), "w");
    if (!f) return;
    std::fprintf(f, "master=%.3f\nmusic=%.3f\nsfx=%.3f\nsens=%.3f\nfullscreen=%d\n", st.master, st.music, st.sfx, st.sens, st.fullscreen ? 1 : 0);
    std::fclose(f);
}

void applyFullscreen(GLFWwindow* win, bool fs, WindowedState& ws) {
    bool isFs = glfwGetWindowMonitor(win) != nullptr;
    if (fs && !isFs) {
        glfwGetWindowPos(win, &ws.x, &ws.y);  // mémorise la fenêtre pour la restaurer
        glfwGetWindowSize(win, &ws.w, &ws.h);
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* vm = glfwGetVideoMode(mon);
        glfwSetWindowMonitor(win, mon, 0, 0, vm->width, vm->height, vm->refreshRate);
    } else if (!fs && isFs) {
        glfwSetWindowMonitor(win, nullptr, ws.x, ws.y, ws.w, ws.h, 0);
    }
    glfwSwapInterval(1);
}
