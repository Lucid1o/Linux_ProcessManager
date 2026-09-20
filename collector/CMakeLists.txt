add_library(pulse_collector STATIC
    src/linux_proc_collector.cpp
    src/simulated_collector.cpp
)

target_include_directories(pulse_collector PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(pulse_collector PUBLIC cxx_std_20)
target_link_libraries(pulse_collector PUBLIC pulse_core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pulse_collector PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(pulse_collector PRIVATE /W4)
endif()

if(BUILD_TESTING)
    add_executable(test_collector ../core/tests/test_collector.cpp)
    target_link_libraries(test_collector PRIVATE pulse_collector pulse_core)
    target_compile_definitions(test_collector PRIVATE TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/core/tests/fixtures")
    add_test(NAME test_collector COMMAND test_collector)
endif()
