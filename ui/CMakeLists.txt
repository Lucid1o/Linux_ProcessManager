find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(pulse
    include/pulse/ui/main_window.hpp
    include/pulse/ui/dashboard_widget.hpp
    include/pulse/ui/sparkline_widget.hpp
    include/pulse/ui/process_tree_model.hpp
    include/pulse/ui/recommendations_panel.hpp
    include/pulse/ui/action_dialogs.hpp
    src/main.cpp
    src/main_window.cpp
    src/dashboard_widget.cpp
    src/sparkline_widget.cpp
    src/process_tree_model.cpp
    src/recommendations_panel.cpp
    src/action_dialogs.cpp
    ../resources/pulse.qrc
)

target_include_directories(pulse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(pulse PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pulse PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(pulse PRIVATE /W4)
endif()

target_link_libraries(pulse PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    pulse_core
    pulse_collector
)
