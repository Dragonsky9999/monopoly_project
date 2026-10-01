add_library(monopoly_core STATIC
    src/DataMaster.cpp
    src/MonopolyEngine.cpp
)

target_include_directories(monopoly_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
