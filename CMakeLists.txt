cmake_minimum_required(VERSION 3.5)
project(svocab-server CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


if (CMAKE_SYSTEM_NAME STREQUAL "Darwin") # MacOS
    message(STATUS "macos")
    set(CMAKE_CXX_COMPILER "/opt/homebrew/Cellar/gcc/14.2.0/bin/g++-14")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/icu4c")
    set(ICU_INCLUDE "/opt/homebrew/opt/icu4c/include")
    set(Gumbo_INCLUDE "/opt/homebrew/Cellar/gumbo-parser/0.12.1/include")
    set(Gumbo_LIBRARY "/opt/homebrew/Cellar/gumbo-parser/0.12.1/lib/libgumbo.a")
    set(OPENSSL_INCLUDE "/opt/homebrew/opt/openssl@3/include")
    set(OPENSSL_LIBRARY "/opt/homebrew/opt/openssl@3/lib")
    set(Poppler_LIBRARY "/usr/local/lib/libpoppler-cpp.dylib")

    find_package(Drogon CONFIG REQUIRED)
    find_package(CURL REQUIRED)
    find_package(ICU COMPONENTS uc io REQUIRED)

    include_directories(${ICU_INCLUDE})
    include_directories(${Gumbo_INCLUDE})
    include_directories(${OPENSSL_INCLUDE})

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux") # Linux
    find_package(ICU COMPONENTS uc io REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(Drogon CONFIG REQUIRED)
    find_package(CURL REQUIRED)

    set(Poppler_LIBRARY "/usr/lib/x86_64-linux-gnu/libpoppler-cpp.so")
    set(Gumbo_LIBRARY "/usr/local/lib/libgumbo.a")

    include_directories()

    # target_include_directories(${PROJECT_NAME} PRIVATE
        include_directories(${JSONCPP_INCLUDE_DIRS})
        include_directories(${OPENSSL_INCLUDE_DIR})
        include_directories(${POSTGRESQL_INCLUDE_DIR})
        include_directories(${HIREDIS_INCLUDE_DIR})
        include_directories(${ICU_INCLUDE_DIRS})
        include_directories(${POPLER_INCLUDE_DIRS})
    # )
else()
    message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

# Set the directories
set(CONTROLLERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/controllers)
set(FILTERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/filters)
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core)
set(UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/utils)
set(PLUGINS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/plugins)

# Define a function to gather all .cc files from a directory
function(gather_sources TARGET_VAR DIR)
    file(GLOB_RECURSE SOURCES ${DIR}/*.cc)
    set(${TARGET_VAR} ${SOURCES} PARENT_SCOPE)
endfunction()

# Use the function to gather sources
gather_sources(CONTROLLER_SOURCES ${CONTROLLERS_DIR})
gather_sources(FILTER_SOURCES ${FILTERS_DIR})
gather_sources(CORE_SOURCES ${CORE_DIR})
gather_sources(UTILS_SOURCES ${UTILS_DIR})
gather_sources(PLUGINS_SOURCES ${PLUGINS_DIR})

# Add the executable with all the source files
add_executable(${PROJECT_NAME} main.cpp ${CONTROLLER_SOURCES} ${FILTER_SOURCES} ${CORE_SOURCES} ${UTILS_SOURCES} ${PLUGINS_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon ${Poppler_LIBRARY} ${Gumbo_LIBRARY} ${ICU_LIBRARIES} CURL::libcurl)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/models)

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)

target_sources(${PROJECT_NAME}
   PRIVATE
   ${SRC_DIR}
   ${CTL_SRC}
   ${FILTER_SRC}
   ${PLUGIN_SRC}
   ${MODEL_SRC}
)