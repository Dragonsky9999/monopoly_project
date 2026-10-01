cmake_minimum_required(VERSION 3.14)
project(MonopolyProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(core/include cli/include)

add_library(monopoly_core
    core/src/MonopolyEngine.cpp
    core/src/DataMaster.cpp
    core/src/AIPlayer.cpp
)

add_executable(monopoly_cli
    cli/src/main.cpp
)

target_link_libraries(monopoly_cli PRIVATE monopoly_core)
