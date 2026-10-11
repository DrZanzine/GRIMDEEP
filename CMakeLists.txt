# =============================================================================
#  CMakeLists.txt — construction alternative (IDE : VS Code, CLion…)
#
#  build.bat reste la méthode de référence. CMake permet en plus une compilation
#  incrémentale (seuls les fichiers modifiés sont recompilés). Exemple avec MSYS2 :
#     cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
#     cmake --build build -j
#  L'exécutable est produit dans build/grimdeep.exe.
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(GRIMDEEP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")  # mêmes options que build.bat

file(GLOB_RECURSE GRIMDEEP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
add_executable(grimdeep ${GRIMDEEP_SOURCES})
target_include_directories(grimdeep PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(grimdeep PRIVATE -Wall -Wextra)
target_link_libraries(grimdeep PRIVATE glew32 glfw3 opengl32 gdi32 winmm)
set_target_properties(grimdeep PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
