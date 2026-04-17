cmake_minimum_required(VERSION 3.16)

set(RANDOMIZER_ONLY "0" CACHE STRING "Runs only the randomizer generator")
set(RANDO_SAVE_PATH "${CMAKE_BINARY_DIR}/randomizer/")

set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} 
                        RANDOMIZER_ONLY=${RANDOMIZER_ONLY}
                        RANDO_SAVE_PATH="${RANDO_SAVE_PATH}"
                        RANDO_DATA_PATH="${CMAKE_SOURCE_DIR}/src/dusk/randomizer/generator/data/")

if(RANDO_ERROR_LOG)
  message("Error Log will be saved")
  set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} RANDO_ERROR_LOG)
endif()

if(RANDO_DEBUG)
  set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} RANDO_DEBUG)
endif()

if(LOGIC_TESTS)
  message("Configuring for Logic Tests")

  set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} LOGIC_TESTS)

  if(TEST_COUNT)
    message("Test Count: " ${TEST_COUNT})
    set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} TEST_COUNT=${TEST_COUNT})
  endif()
  set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} SETTINGS_PATH="${RANDO_SAVE_PATH}/randomizer_settings.yaml.test" PREFERENCES_PATH="${RANDO_SAVE_PATH}/randomizer_preferences.yaml.test")
else()
  set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} SETTINGS_PATH="${RANDO_SAVE_PATH}/randomizer_settings.yaml" PREFERENCES_PATH="${RANDO_SAVE_PATH}/randomizer_preferences.yaml")
endif()

message(STATUS "randomizer: Fetching yaml-cpp")
FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG yaml-cpp-0.9.0
)
message(STATUS "randomizer: Fetching base64pp")
FetchContent_Declare(
        base64pp
        GIT_REPOSITORY https://github.com/matheusgomes28/base64pp.git
        GIT_TAG v0.2.0-rc0
)
message(STATUS "randomizer: Fetching zlib-ng")
FetchContent_Declare(
        zlib-ng
        GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng.git
        GIT_TAG 2.3.3
)

FetchContent_MakeAvailable(yaml-cpp base64pp zlib-ng)

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
set(GAME_COMPILE_DEFS ${GAME_COMPILE_DEFS} SOURCE_PATH_SIZE=${SOURCE_PATH_SIZE})
set(GAME_LIBS ${GAME_LIBS} yaml-cpp::yaml-cpp zlib base64pp)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/randomizer")
# Put data files together for easier manipulation
# file(COPY "${CMAKE_SOURCE_DIR}/src/dusk/randomizer/data/" DESTINATION "${CMAKE_BINARY_DIR}/randomizer/data/" REGEX "^.*example.*$" EXCLUDE) # World, macros, and location info
