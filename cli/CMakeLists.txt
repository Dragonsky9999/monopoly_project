add_executable(monopoly_cli src/main.cpp)

target_include_directories(monopoly_cli PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/cli/include
)

target_link_libraries(monopoly_cli PRIVATE monopoly_core)
