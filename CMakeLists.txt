cmake_minimum_required(VERSION 3.16)
project(SimpleChat LANGUAGES CXX)

# Default to Release if no build type is specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost (Beast is header-only; Asio uses Boost::system)
find_package(Boost REQUIRED COMPONENTS system json)

add_executable(SimpleChat)

target_include_directories(SimpleChat PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(SimpleChat PRIVATE Boost::system Boost::json pthread)

# Main entry
target_sources(SimpleChat PRIVATE ${CMAKE_SOURCE_DIR}/src/SimpleChat.cpp)

# Add a module folder (build all .cpp under src/<module>/)
function(add_module MODULE_DIR)
  file(GLOB_RECURSE MODULE_SOURCES
       CONFIGURE_DEPENDS
       "${CMAKE_SOURCE_DIR}/src/${MODULE_DIR}/*.cpp"
  )
  if (MODULE_SOURCES)
    target_sources(SimpleChat PRIVATE ${MODULE_SOURCES})
  else()
    message(WARNING "No .cpp files found under src/${MODULE_DIR}")
  endif()
endfunction()

# Modules (just add folder names here as you grow)
add_module(networking)
add_module(chat)