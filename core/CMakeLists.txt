add_library(pulse_core STATIC
    src/app_grouper.cpp
    src/process_manager.cpp
    src/rule_engine.cpp
)

target_include_directories(pulse_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(pulse_core PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pulse_core PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(pulse_core PRIVATE /W4)
endif()

if(BUILD_TESTING)
    add_executable(test_grouper tests/test_grouper.cpp)
    target_link_libraries(test_grouper PRIVATE pulse_core)
    add_test(NAME test_grouper COMMAND test_grouper)

    add_executable(test_termination tests/test_termination.cpp)
    target_link_libraries(test_termination PRIVATE pulse_core)
    add_test(NAME test_termination COMMAND test_termination)

    add_executable(test_rule_engine tests/test_rule_engine.cpp)
    target_link_libraries(test_rule_engine PRIVATE pulse_core)
    add_test(NAME test_rule_engine COMMAND test_rule_engine)
endif()
