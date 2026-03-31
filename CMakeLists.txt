cmake_minimum_required(VERSION 3.16)
project(validate_smalltalk_image VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(validate_smalltalk_image
    src/main.cpp
    src/SpurImage.cpp
    src/ValidationContext.cpp
    src/HeapWalker.cpp
    src/HeapValidator.cpp
    src/OopValidator.cpp
    src/ClassTableValidator.cpp
    src/ClassHierarchyValidator.cpp
    src/SpecialObjectsValidator.cpp
    src/MethodValidator.cpp
    src/ReachabilityAnalyzer.cpp
    src/Report.cpp
)

target_include_directories(validate_smalltalk_image PRIVATE src)

# Optimized release build by default for large images
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
