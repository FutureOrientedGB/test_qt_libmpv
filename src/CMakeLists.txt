cmake_minimum_required(VERSION 3.20)


set(PROJECT_NAME test)


project(${PROJECT_NAME})


# find zlib
find_package(ZLIB REQUIRED)

# find fmt
find_package(fmt CONFIG REQUIRED)

# find spdlog
find_package(spdlog CONFIG REQUIRED)

# find cli11
find_package(CLI11 CONFIG REQUIRED)

# find libmpv
find_package(libmpv CONFIG REQUIRED)

# find qt
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)


# defines
ADD_DEFINITIONS(-DUNICODE -D_UNICODE)
ADD_DEFINITIONS(-DVC_EXTRALEAN)
ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN)


# classify filters
FILE(GLOB_RECURSE HEADER_FILES
        "*.hpp"
)
FILE(GLOB_RECURSE SOURCE_FILES
        "*.cpp"
)
FILE(GLOB_RECURSE MOC_FILES
        "moc_*.cpp"
)
SOURCE_GROUP("Header Files" FILES ${HEADER_FILES})
SOURCE_GROUP("Source Files" FILES ${SRC_FILES})
SOURCE_GROUP("Generated Files" FILES ${MOC_FILES})


# executable
add_executable(${PROJECT_NAME}
        WIN32
        ${HEADER_FILES}
        ${SOURCE_FILES}
)


# auto run qt moc
set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON)


# output dir
set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY $ENV{OUTPUT_EXE_DIR}
)


# Visual Studio - Properity - C/C++ - Code Generation - Rutime Library > /MT
if(MSVC)
set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
endif(MSVC)


# Visual Studio - Properity - C/C++ - General - Additional Link Libraries
if(MSVC)
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        # fmt
        fmt::fmt
        # spdlog
        spdlog::spdlog
        # cli11
        CLI11::CLI11
        # qt
        Qt5::WinMain
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
        # libmpv
        libmpv
)
else()
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        # fmt
        fmt::fmt
        # spdlog
        spdlog::spdlog
        # cli11
        CLI11::CLI11
        # qt
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
        # libmpv
        mpv
)
endif(MSVC)
