cmake_minimum_required(VERSION 3.16)

project(diesel_heater_protocol_smoke_tests LANGUAGES CXX)

include(CTest)

add_executable(protocol_request_smoke protocol_request_smoke.cpp)
target_compile_features(protocol_request_smoke PRIVATE cxx_std_17)
target_include_directories(protocol_request_smoke PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../components/diesel_heater_ble"
)
target_compile_options(protocol_request_smoke PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME protocol_request_smoke COMMAND protocol_request_smoke)

add_executable(protocol_contracts_test protocol_contracts_test.cpp)
target_compile_features(protocol_contracts_test PRIVATE cxx_std_17)
target_include_directories(protocol_contracts_test PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../components/diesel_heater_ble"
)
target_compile_options(protocol_contracts_test PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME protocol_contracts_test COMMAND protocol_contracts_test)
