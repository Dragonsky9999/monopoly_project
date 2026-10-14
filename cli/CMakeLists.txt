add_executable(monopoly_cli src/main.cpp)

target_include_directories(monopoly_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(monopoly_cli PRIVATE monopoly_core)
