cmake_minimum_required(VERSION 3.20)
project(MonteCarloOptionSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable optimizations in Release mode
if(MSVC)
    add_compile_options(/W4 /EHsc /O2 /permissive-)
else()
    add_compile_options(-Wall -Wextra -O3 -pthread)
endif()

# Main Production Executable (Interactive TUI by default, batch with --batch)
add_executable(monte_carlo_pricer
    src/main.cpp
    src/Builder.cpp
    src/MCEngine.cpp
    src/Mediator.cpp
    src/Payoff.cpp
    src/RNG.cpp
    src/SDE.cpp
    src/TUI.cpp
)

target_include_directories(monte_carlo_pricer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(UNIX AND NOT APPLE)
    find_package(TBB QUIET)
    if(TBB_FOUND)
        target_link_libraries(monte_carlo_pricer PRIVATE TBB::tbb)
    else()
        target_link_libraries(monte_carlo_pricer PRIVATE pthread)
    endif()
endif()
