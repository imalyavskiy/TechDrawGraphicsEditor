cmake_minimum_required(VERSION 3.16)

project(TechDraw VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Gui)

add_executable(TechDraw WIN32
    resources/techdraw.qrc
    resources/techdraw.rc
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
)

target_link_libraries(TechDraw PRIVATE
    Qt5::Widgets
    Qt5::GuiPrivate
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(TechDraw PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(TechDraw PRIVATE /W4)
endif()
