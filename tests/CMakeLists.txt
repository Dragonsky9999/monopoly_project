add_executable(core_tests test_core.cpp)
target_link_libraries(core_tests PRIVATE monopoly_core)
add_test(NAME core_tests COMMAND core_tests)
