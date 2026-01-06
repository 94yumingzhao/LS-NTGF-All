# LS-NTGF-All - Unified Production Planning Optimization System
# Supports multiple algorithms: RF, RFO, RR (PP-GCB)

cmake_minimum_required(VERSION 3.24)

project(LS-NTGF-All
    VERSION 2.0.0
    DESCRIPTION "Unified Production Planning Optimization System (RF/RFO/RR)"
    LANGUAGES CXX
)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set output directories for multi-configuration generators (e.g., Visual Studio)
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
endforeach()

# CPLEX configuration
set(CPLEX_DIR "D:/CPLEX" CACHE PATH "CPLEX installation directory")
if(NOT EXISTS ${CPLEX_DIR})
    message(WARNING "CPLEX directory not found at ${CPLEX_DIR}. Please set CPLEX_DIR correctly.")
endif()

# Set source file directories
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SOLVERS_DIR ${CMAKE_SOURCE_DIR}/src/solvers)

# Add include directories
include_directories(
    ${SRC_DIR}                        # Main source directory (for optimizer.h, common.h, etc.)
    ${SOLVERS_DIR}                    # Solvers directory
    "${CPLEX_DIR}/cplex/include"      # CPLEX headers
    "${CPLEX_DIR}/concert/include"    # Concert headers
)

# Define necessary macros
add_definitions(-DIL_STD)

# Set runtime library for MSVC compiler
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

# Add library directories
link_directories(
    "${CPLEX_DIR}/cplex/lib/x64_windows_msvc14/stat_mda"
    "${CPLEX_DIR}/concert/lib/x64_windows_msvc14/stat_mda"
)

# Define source files
set(SOURCES
    # Main entry point
    ${SRC_DIR}/main.cpp

    # Shared infrastructure
    ${SRC_DIR}/input.cpp
    ${SRC_DIR}/output.cpp
    ${SRC_DIR}/cplex_lot_sizing.cpp
    ${SRC_DIR}/big_order.cpp
    ${SRC_DIR}/logger.cpp
    ${SRC_DIR}/case_analysis.cpp

    # Algorithm solvers
    ${SOLVERS_DIR}/rf_solver.cpp       # RF algorithm
    ${SOLVERS_DIR}/rfo_solver.cpp      # RFO algorithm (RF + FO)
    ${SOLVERS_DIR}/pp_gcb_solver.cpp   # PP-GCB (RR) algorithm
)

# Define header files (for IDE display)
set(HEADERS
    ${SRC_DIR}/common.h
    ${SRC_DIR}/logger.h
    ${SRC_DIR}/optimizer.h
    ${SRC_DIR}/case_analysis.h
)

# Organize files in IDE
source_group("Source Files" FILES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/input.cpp
    ${SRC_DIR}/output.cpp
    ${SRC_DIR}/cplex_lot_sizing.cpp
    ${SRC_DIR}/big_order.cpp
    ${SRC_DIR}/logger.cpp
    ${SRC_DIR}/case_analysis.cpp
)
source_group("Source Files\\Solvers" FILES
    ${SOLVERS_DIR}/rf_solver.cpp
    ${SOLVERS_DIR}/rfo_solver.cpp
    ${SOLVERS_DIR}/pp_gcb_solver.cpp
)
source_group("Header Files" FILES ${HEADERS})

# Create main executable
add_executable(LS-NTGF-All ${SOURCES} ${HEADERS})

# Set target properties
set_target_properties(LS-NTGF-All PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    OUTPUT_NAME "LS-NTGF-All"
)

# Link CPLEX libraries
target_link_libraries(LS-NTGF-All PRIVATE
    cplex2210
    ilocplex
    concert
)

# Add compile definitions
target_compile_definitions(LS-NTGF-All PRIVATE
    OPTIMIZER_VERSION="2.0"
    OPTIMIZER_VERSION_MAJOR=2
    OPTIMIZER_VERSION_MINOR=0
)

# Installation rules
install(TARGETS LS-NTGF-All
    RUNTIME DESTINATION bin
)

# Create run targets for each algorithm
add_custom_target(run-rf
    COMMAND LS-NTGF-All --algo=RF
    DEPENDS LS-NTGF-All
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running with RF algorithm..."
)

add_custom_target(run-rfo
    COMMAND LS-NTGF-All --algo=RFO
    DEPENDS LS-NTGF-All
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running with RFO algorithm..."
)

add_custom_target(run-rr
    COMMAND LS-NTGF-All --algo=RR
    DEPENDS LS-NTGF-All
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running with RR (PP-GCB) algorithm..."
)

# Create test targets
enable_testing()
add_test(NAME Test_RF
    COMMAND LS-NTGF-All --algo=RF
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Test_RFO
    COMMAND LS-NTGF-All --algo=RFO
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
add_test(NAME Test_RR
    COMMAND LS-NTGF-All --algo=RR
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Generate configuration summary
message(STATUS "")
message(STATUS "=== LS-NTGF-All Build Configuration ===")
message(STATUS "CMake Version: ${CMAKE_VERSION}")
message(STATUS "Project Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "CPLEX Directory: ${CPLEX_DIR}")
message(STATUS "Source Directory: ${SRC_DIR}")
message(STATUS "Solvers Directory: ${SOLVERS_DIR}")
message(STATUS "Output Directory: ${CMAKE_BINARY_DIR}")
message(STATUS "")
message(STATUS "Supported Algorithms:")
message(STATUS "  --algo=RF   : Relax-and-Fix")
message(STATUS "  --algo=RFO  : RF + Fix-and-Optimize")
message(STATUS "  --algo=RR   : PP-GCB 3-stage decomposition")
message(STATUS "========================================")
message(STATUS "")

# Clean target
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
    COMMENT "Cleaning all build outputs..."
)
