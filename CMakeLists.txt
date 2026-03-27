cmake_minimum_required(VERSION 3.25)

project(AshPawServer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)

option(ASHPAW_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

find_package(unofficial-enet CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

set(ASHPAW_PUBLIC_HEADERS
    include/ashpaw/app/server_app.hpp
    include/ashpaw/config/server_config.hpp
    include/ashpaw/net/network_server.hpp
    include/ashpaw/net/protocol.hpp
    include/ashpaw/persistence/player_repository.hpp
    include/ashpaw/session/session.hpp
    include/ashpaw/session/session_manager.hpp
    include/ashpaw/simulation/simulation_clock.hpp
    include/ashpaw/util/logging.hpp
    include/ashpaw/world/map.hpp
    include/ashpaw/world/world.hpp
)

set(ASHPAW_SOURCES
    src/app/server_app.cpp
    src/config/server_config.cpp
    src/net/network_server.cpp
    src/net/protocol.cpp
    src/persistence/player_repository.cpp
    src/session/session_manager.cpp
    src/simulation/simulation_clock.cpp
    src/util/logging.cpp
    src/world/map.cpp
    src/world/world.cpp
)

add_library(ashpaw_server_lib
    ${ASHPAW_PUBLIC_HEADERS}
    ${ASHPAW_SOURCES}
)

target_include_directories(ashpaw_server_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ashpaw_server_lib
    PUBLIC
        unofficial::enet::enet
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        tomlplusplus::tomlplusplus
)

target_compile_definitions(ashpaw_server_lib
    PRIVATE
        $<$<BOOL:${WIN32}>:NOMINMAX>
)

if(MSVC)
    target_compile_options(ashpaw_server_lib PRIVATE /W4 /permissive-)
    if(ASHPAW_WARNINGS_AS_ERRORS)
        target_compile_options(ashpaw_server_lib PRIVATE /WX)
    endif()
else()
    target_compile_options(ashpaw_server_lib PRIVATE -Wall -Wextra -Wpedantic)
    if(ASHPAW_WARNINGS_AS_ERRORS)
        target_compile_options(ashpaw_server_lib PRIVATE -Werror)
    endif()
endif()

add_executable(ashpaw_server src/app/main.cpp)
target_link_libraries(ashpaw_server PRIVATE ashpaw_server_lib)

add_executable(ashpaw_server_tests
    tests/config_tests.cpp
    tests/persistence_tests.cpp
    tests/protocol_tests.cpp
    tests/world_tests.cpp
    tests/integration_tests.cpp
)
target_link_libraries(ashpaw_server_tests
    PRIVATE
        ashpaw_server_lib
        Catch2::Catch2WithMain
        unofficial::enet::enet
)

include(Catch)
catch_discover_tests(ashpaw_server_tests)

install(TARGETS ashpaw_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
