cmake_minimum_required(VERSION 3.20)

project(FinanceDashboard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(finance_core
    src/Portfolio.cpp
)

target_include_directories(finance_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(finance_core PRIVATE
    -Wall -Wextra -Wpedantic
)

add_executable(finance_dashboard
    src/main.cpp
)

target_link_libraries(finance_dashboard PRIVATE finance_core)
target_compile_options(finance_dashboard PRIVATE
    -Wall -Wextra -Wpedantic
)

find_package(Qt6 COMPONENTS Widgets)
if(Qt6_FOUND)
    add_executable(finance_dashboard_gui
        src/qt_gui_main.cpp
    )

    target_link_libraries(finance_dashboard_gui PRIVATE finance_core Qt6::Widgets)
    target_compile_options(finance_dashboard_gui PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

enable_testing()

add_executable(portfolio_tests
    tests/portfolio_tests.cpp
)

target_link_libraries(portfolio_tests PRIVATE finance_core)
target_compile_options(portfolio_tests PRIVATE
    -Wall -Wextra -Wpedantic
)

add_test(NAME portfolio_tests COMMAND portfolio_tests)
