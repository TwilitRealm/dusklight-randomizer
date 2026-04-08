cmake_minimum_required(VERSION 3.16)

set(RANDOMIZER_ONLY "0" CACHE STRING "Runs only the randomizer generator")
set(RANDO_SAVE_PATH "${CMAKE_BINARY_DIR}/randomizer/")

add_compile_definitions(RANDO_SAVE_PATH="${RANDO_SAVE_PATH}" 
                        LOGS_PATH="${RANDO_SAVE_PATH}logs/" 
                        DATA_PATH="${RANDO_SAVE_PATH}data/")

if(WRITE_ERROR_LOG)
  message("Error Log will be saved")
  add_compile_definitions(WRITE_ERROR_LOG)
endif()

if(ENABLE_TIMING)
  message("Some events will be timed")

  add_compile_definitions(ENABLE_TIMING)
endif()

if(DRY_RUN)
  message("Game patching will be skipped")

  add_compile_definitions(DRY_RUN)
endif()

if(RANDO_DEBUG)
  add_compile_definitions(RANDO_DEBUG)
endif()

if(LOGIC_TESTS)
  message("Configuring for Logic Tests")

  add_compile_definitions(LOGIC_TESTS)

  if(TEST_COUNT)
    message("Test Count: " ${TEST_COUNT})
    add_compile_definitions(TEST_COUNT=${TEST_COUNT})
  endif()
  add_compile_definitions(SETTINGS_PATH="${RANDO_SAVE_PATH}randomizer_settings.yaml.test" PREFERENCES_PATH="${RANDO_SAVE_PATH}randomizer_preferences.yaml.test")
else()
  add_compile_definitions(SETTINGS_PATH="${RANDO_SAVE_PATH}randomizer_settings.yaml" PREFERENCES_PATH="${RANDO_SAVE_PATH}randomizer_preferences.yaml")
endif()

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_compile_definitions(SOURCE_PATH_SIZE=${SOURCE_PATH_SIZE})

# Put data files together for easier manipulation
file(COPY "./data/" DESTINATION "${CMAKE_BINARY_DIR}/randomizer/data/" REGEX "^.*example.*$" EXCLUDE) # World, macros, and location info

message(STATUS "randomizer: Fetching yaml-cpp")
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG yaml-cpp-0.9.0
)
FetchContent_MakeAvailable(yaml-cpp)

message(STATUS "randomizer: Fetching base64pp")
FetchContent_Declare(
  base64pp
  GIT_REPOSITORY https://github.com/matheusgomes28/base64pp.git
  GIT_TAG v0.2.0-rc0
)
FetchContent_MakeAvailable(base64pp)

message(STATUS "randomizer: Fetching zlib-ng")
FetchContent_Declare(
  zlib-ng
  GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng.git
  GIT_TAG 2.3.3
)
FetchContent_MakeAvailable(zlib-ng)

file(GLOB_RECURSE CODE_SOURCES CONFIGURE_DEPENDS  "*.cpp" "*.hpp")
add_library(randomizer OBJECT ${CODE_SOURCES})
target_link_libraries(randomizer yaml-cpp::yaml-cpp zlib base64pp)