cmake_minimum_required(VERSION 3.16)

project(TechDraw VERSION 0.1.0 LANGUAGES CXX RC)

set(TECHDRAW_ARCHITECTURE "x64" CACHE STRING "Target Windows architecture: x64 or x86")
set_property(CACHE TECHDRAW_ARCHITECTURE PROPERTY STRINGS x64 x86)
if(NOT TECHDRAW_ARCHITECTURE STREQUAL "x64" AND NOT TECHDRAW_ARCHITECTURE STREQUAL "x86")
    message(FATAL_ERROR "TECHDRAW_ARCHITECTURE must be x64 or x86")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Gui LinguistTools)

if(WIN32)
    if(TECHDRAW_ARCHITECTURE STREQUAL "x64" AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The x64 configuration requires a 64-bit compiler and Qt package")
    elseif(TECHDRAW_ARCHITECTURE STREQUAL "x86" AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "The x86 configuration requires a 32-bit compiler and Qt package")
    endif()
endif()

set(TECHDRAW_TRANSLATION_SOURCES
    translations/techdraw_ru.ts
)
qt5_add_translation(TECHDRAW_TRANSLATIONS ${TECHDRAW_TRANSLATION_SOURCES})
add_custom_target(TechDrawTranslations ALL DEPENDS ${TECHDRAW_TRANSLATIONS})

set(TECHDRAW_WINDOWS_RESOURCE)
if(WIN32 AND MINGW)
    set(TECHDRAW_WINDOWS_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/techdraw_resources.o")
    add_custom_command(
        OUTPUT "${TECHDRAW_WINDOWS_RESOURCE}"
        COMMAND "${CMAKE_RC_COMPILER}" -i techdraw.rc -o "${TECHDRAW_WINDOWS_RESOURCE}" --include-dir=.
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/techdraw.ico"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/techdraw.manifest"
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/techdraw.rc"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        VERBATIM
    )
    set_source_files_properties("${TECHDRAW_WINDOWS_RESOURCE}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
elseif(WIN32)
    set(TECHDRAW_WINDOWS_RESOURCE resources/techdraw.rc)
endif()

add_executable(TechDraw WIN32
    resources/techdraw.qrc
    ${TECHDRAW_WINDOWS_RESOURCE}
    src/canvas.cpp
    src/canvas.h
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/project.cpp
    src/project.h
    src/rolloutsection.cpp
    src/rolloutsection.h
    src/selftest.cpp
    src/selftest.h
    src/taskbarpin.cpp
    src/taskbarpin.h
)

add_dependencies(TechDraw TechDrawTranslations)
add_custom_command(
    TARGET TechDraw POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:TechDraw>/translations"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${TECHDRAW_TRANSLATIONS}
            "$<TARGET_FILE_DIR:TechDraw>/translations"
    VERBATIM
)

target_link_libraries(TechDraw PRIVATE
    Qt5::Widgets
    Qt5::GuiPrivate
)

if(WIN32)
    # TaskbarManager is a WinRT API; MinGW supplies its activation functions through runtimeobject.
    target_link_libraries(TechDraw PRIVATE runtimeobject)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(TechDraw PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(TechDraw PRIVATE /W4)
endif()
