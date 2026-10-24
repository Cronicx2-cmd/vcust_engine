# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(VcustEngine LANGUAGES C CXX) # Dodane C dla glad.c

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Binarka ląduje w <build>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Zależności
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(game 
    src/main.cpp 
    src/OpenGLEngine.cpp 
    src/SceneManager.cpp
    src/Model.cpp
    src/GameLogic.cpp
    src/Enemy.cpp          # <--- DODAJ TĘ LINIJKĘ
    src/external/glad/src/glad.c
)

target_include_directories(game PRIVATE src src/external/glad/include)
target_link_libraries(game PRIVATE glfw glm::glm OpenGL::GL)
target_link_libraries(game PRIVATE ole32 winmm)

# glad.c jest C, wymaga jawnego ustawienia języka
set_source_files_properties(src/external/glad/src/glad.c PROPERTIES LANGUAGE C)

target_compile_definitions(game PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    USE_OPENGL) # Zostawiamy dla kompatybilności z Camera.hpp

# Kopiowanie zasobów (DLLki + content)
if(WIN32)
    set(MINGW_DLLS
        "C:/msys64/mingw64/bin/glfw3.dll"
        "C:/msys64/mingw64/bin/libstdc++-6.dll"
        "C:/msys64/mingw64/bin/libgcc_s_seh-1.dll"
        "C:/msys64/mingw64/bin/libwinpthread-1.dll"
    )

    foreach(DLL_PATH ${MINGW_DLLS})
        add_custom_command(TARGET game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL_PATH}" $<TARGET_FILE_DIR:game>)
    endforeach()
    
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/content
        $<TARGET_FILE_DIR:game>/content)

    # Edytor map / MenuMaker (Python) obok binarki
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/editor.py")
        add_custom_command(TARGET game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/editor.py"
            $<TARGET_FILE_DIR:game>/editor.py)
    endif()
endif()

target_compile_options(game PRIVATE -Wall -Wextra -Wno-missing-field-initializers)