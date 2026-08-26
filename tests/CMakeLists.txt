cmake_minimum_required(VERSION 3.16)

project(diesel_heater_protocol_smoke_tests LANGUAGES CXX)

include(CTest)

option(ENABLE_SANITIZERS "Build host tests with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  else()
    message(FATAL_ERROR "ENABLE_SANITIZERS requires a GNU or Clang C++ compiler")
  endif()
endif()

add_library(diesel_heater_protocol STATIC
  ../components/diesel_heater_ble/protocol.cpp
  ../components/diesel_heater_ble/protocol_aa.cpp
)
target_compile_features(diesel_heater_protocol PUBLIC cxx_std_17)
target_include_directories(diesel_heater_protocol PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/../components/diesel_heater_ble"
)
target_compile_options(diesel_heater_protocol PRIVATE -Wall -Wextra -Wpedantic)

add_executable(protocol_request_smoke protocol_request_smoke.cpp)
target_compile_features(protocol_request_smoke PRIVATE cxx_std_17)
target_link_libraries(protocol_request_smoke PRIVATE diesel_heater_protocol)
target_compile_options(protocol_request_smoke PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME protocol_request_smoke COMMAND protocol_request_smoke)

add_executable(protocol_contracts_test protocol_contracts_test.cpp)
target_compile_features(protocol_contracts_test PRIVATE cxx_std_17)
target_link_libraries(protocol_contracts_test PRIVATE diesel_heater_protocol)
target_compile_options(protocol_contracts_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME protocol_contracts_test COMMAND protocol_contracts_test)

add_executable(response_parser_safety_test response_parser_safety_test.cpp)
target_compile_features(response_parser_safety_test PRIVATE cxx_std_17)
target_link_libraries(response_parser_safety_test PRIVATE diesel_heater_protocol)
target_compile_options(response_parser_safety_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME response_parser_safety_test COMMAND response_parser_safety_test)

add_executable(protocol_selection_test protocol_selection_test.cpp)
target_compile_features(protocol_selection_test PRIVATE cxx_std_17)
target_link_libraries(protocol_selection_test PRIVATE diesel_heater_protocol)
target_compile_options(protocol_selection_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME protocol_selection_test COMMAND protocol_selection_test)

add_executable(lifecycle_admission_test lifecycle_admission_test.cpp)
target_compile_features(lifecycle_admission_test PRIVATE cxx_std_17)
target_link_libraries(lifecycle_admission_test PRIVATE diesel_heater_protocol)
target_compile_options(lifecycle_admission_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME lifecycle_admission_test COMMAND lifecycle_admission_test)

add_executable(command_delivery_test command_delivery_test.cpp)
target_compile_features(command_delivery_test PRIVATE cxx_std_17)
target_link_libraries(command_delivery_test PRIVATE diesel_heater_protocol)
target_compile_options(command_delivery_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME command_delivery_test COMMAND command_delivery_test)

add_executable(entity_control_test entity_control_test.cpp)
target_compile_features(entity_control_test PRIVATE cxx_std_17)
target_link_libraries(entity_control_test PRIVATE diesel_heater_protocol)
target_compile_options(entity_control_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME entity_control_test COMMAND entity_control_test)

add_executable(climate_control_session_test climate_control_session_test.cpp)
target_compile_features(climate_control_session_test PRIVATE cxx_std_17)
target_link_libraries(climate_control_session_test PRIVATE diesel_heater_protocol)
target_compile_options(climate_control_session_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME climate_control_session_test COMMAND climate_control_session_test)
add_test(NAME entity_static_guard COMMAND ${CMAKE_COMMAND}
  -DCOMPONENT_DIR=${CMAKE_CURRENT_LIST_DIR}/../components/diesel_heater_ble
  -P ${CMAKE_CURRENT_LIST_DIR}/entity_static_guard.cmake
)

add_executable(optional_platform_non_esp_guard_test optional_platform_non_esp_guard_test.cpp)
target_compile_features(optional_platform_non_esp_guard_test PRIVATE cxx_std_17)
target_compile_options(optional_platform_non_esp_guard_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME optional_platform_non_esp_guard_test COMMAND optional_platform_non_esp_guard_test)
