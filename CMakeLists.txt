cmake_minimum_required(VERSION 3.5)
project(svocab-server CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


SET(CMAKE_C_COMPILER "/opt/homebrew/Cellar/gcc/14.1.0_2/bin/gcc-14")
SET(CMAKE_CXX_COMPILER "/opt/homebrew/Cellar/gcc/14.1.0_2/bin/g++-14")

# cmake -DCMAKE_BUILD_TYPE=Release \
#       -DCMAKE_C_COMPILER=/opt/homebrew/Cellar/gcc/14.1.0_2/bin/gcc-14 \
#       -DCMAKE_CXX_COMPILER=/opt/homebrew/Cellar/gcc/14.1.0_2/bin/g++-14 \
#       ..

set(CMAKE_CXX_STANDARD 20)
include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ##############################################################################
# If you include the drogon source code locally in your project, use this method
# to add drogon 
# add_subdirectory(drogon) 
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines
find_package(Drogon CONFIG REQUIRED)
include_directories(${POPPLER_INCLUDE_DIRS})
# set(Poppler_LIBRARY /Users/c/Downloads/poppler-24.05.0/build/cpp/libpoppler-cpp.dylib)
# set(Gumbo_LIBRARY /opt/homebrew/Cellar/gumbo-parser/0.10.1/lib/libgumbo.dylib)
# set(ICU_INCLUDE_DIRS /opt/homebrew/opt/icu4c/include)
find_package(CURL REQUIRED)

include_directories(${ICU_INCLUDE_DIRS})
include_directories(${Gumbo_INCLUDE_DIR})
find_package(ICU COMPONENTS uc io REQUIRED)


# add_subdirectory(${JWT_CPP_DIR})
# target_link_libraries(your_target_name jwt-cpp)


# Specify OpenSSL include directories and libraries
include_directories(${OPENSSL_INCLUDE})
link_directories(${OPENSSL_LIB_DIR})
include_directories(${JWT_CPP_INCLUDE})
include_directories(${LibOAI_INCLUDE})

# set(SOURCE_FILES
#    topic_generator.cc
# )

# set(SOURCE_DIR src)

# foreach(SOURCE ${SOURCE_FILES})
#     list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/controllers/${SOURCE})
# endforeach()

# Set the directory containing the source files

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


# message(STATUS "Controller sources: ${CONTROLLER_SOURCES}")
# message(STATUS "Filter sources: ${FILTER_SOURCES}")
# message(STATUS "Core sources: ${CORE_SOURCES}")


# Add the executable with all the source files
add_executable(${PROJECT_NAME} main.cpp ${CONTROLLER_SOURCES} ${FILTER_SOURCES} ${CORE_SOURCES} ${UTILS_SOURCES} ${PLUGINS_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon ${Poppler_LIBRARY} ${Gumbo_LIBRARY} ${ICU_LIBRARIES} CURL::libcurl)

# Link jwt-cpp and OpenSSL
# target_link_libraries(JWTExample PRIVATE jwt-cpp OpenSSL::SSL OpenSSL::Crypto)

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/models)

target_sources(${PROJECT_NAME}
   PRIVATE
   ${SRC_DIR}
   ${CTL_SRC}
   ${FILTER_SRC}
   ${PLUGIN_SRC}
   ${MODEL_SRC}
)

# cmake -D ICU_ROOT=$(brew --prefix icu4c) -D CMAKE_PREFIX_PATH=$(brew --prefix icu4c) -D CMAKE_INCLUDE_PATH=$(brew --prefix icu4c)/include -D CMAKE_LIBRARY_PATH=$(brew --prefix icu4c)/lib ..
