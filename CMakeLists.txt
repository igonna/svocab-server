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

# ##############################################################################
# If you include the drogon source code locally in your project, use this method
# to add drogon 
# add_subdirectory(drogon) 
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines
find_package(Drogon CONFIG REQUIRED)
# set(Poppler_LIBRARY /Users/c/Downloads/poppler-24.05.0/build/cpp/libpoppler-cpp.dylib)
# set(Gumbo_LIBRARY /opt/homebrew/Cellar/gumbo-parser/0.10.1/lib/libgumbo.dylib)
# set(ICU_INCLUDE_DIRS /opt/homebrew/opt/icu4c/include)

include_directories(${ICU_INCLUDE_DIRS})
find_package(ICU COMPONENTS uc io REQUIRED)

include_directories(${LibOAI_INCLUDE})

# set(SOURCE_FILES
#    topic_generator.cc
# )

# set(SOURCE_DIR src)

# foreach(SOURCE ${SOURCE_FILES})
#     list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/controllers/${SOURCE})
# endforeach()

# Set the directory containing the source files
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/controllers)

# Automatically gather all .cc files from the SOURCE_DIR
file(GLOB_RECURSE SOURCES ${SOURCE_DIR}/*.cc)

add_executable(${PROJECT_NAME} main.cpp ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon ${Poppler_LIBRARY} ${Gumbo_LIBRARY} ${ICU_LIBRARIES})

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
