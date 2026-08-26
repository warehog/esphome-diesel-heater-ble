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
