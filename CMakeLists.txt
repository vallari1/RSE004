cmake_minimum_required(VERSION 3.8)
project(RSE004)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Boost REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(fastcdr REQUIRED)
include_directories(${fastcdr_INCLUDE_DIRS})



pkg_check_modules(SQLite3 REQUIRED IMPORTED_TARGET sqlite3)

# Add cpp-httplib
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.12.1
)
FetchContent_MakeAvailable(httplib)

# Add nlohmann-json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Add executables
add_executable(script_a src/bag_reader.cpp)
add_executable(script_b src/data_server.cpp)

target_link_libraries(script_a PRIVATE fastcdr sqlite3 rt pthread)
# Link libraries for Script A
target_link_libraries(script_a PRIVATE
    PkgConfig::SQLite3
    ${Boost_LIBRARIES}
    pthread
    rt
)

# Link libraries for Script B
target_link_libraries(script_b PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    ${Boost_LIBRARIES}
    pthread
    rt
)