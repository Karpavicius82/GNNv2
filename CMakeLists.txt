cmake_minimum_required(VERSION 3.20)
project(GNNv2 CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(${CMAKE_SOURCE_DIR}/tools)
enable_testing()

# Every contract in tools/ and every research probe builds as a standalone executable.
file(GLOB CONTRACTS CONFIGURE_DEPENDS tools/*.cpp)
file(GLOB PROBES    CONFIGURE_DEPENDS research/*.cpp)
foreach(src ${CONTRACTS} ${PROBES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
endforeach()

# Contracts return exit 0 on full pass, so they ARE the test gates -- register them
# with ctest (this replaces the old PowerShell runners; C/C++ only). Diagnostics are
# exploratory (no pass/fail contract) and research probes need data / report metrics,
# so neither is wired into the default test run. The GNNv3 RC1 contract is held
# SEPARATE from the GNNv2 suite: it still builds as a target, but it is not a ctest
# gate here (build/run it directly per docs/GNNv3_RC1_HANDOFF.md).
set(GNNV3_CONTRACT graph_wave_v3_feeling_gate_contract_test)
set(LINEAR_CONTRACTS
  graph_wave_linear_cayley_flow_contract_test)
set(NONLINEAR_CONTRACTS
  graph_wave_pure_physics_chain_contract_test
  graph_wave_horizon_densification_contract_test
  graph_wave_streaming_densification_contract_test
  graph_wave_nonlinear_compute_contract_test)
foreach(src ${CONTRACTS})
  get_filename_component(name ${src} NAME_WE)
  if(name MATCHES "_contract_test$" AND NOT name STREQUAL GNNV3_CONTRACT)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 600)
    if(name IN_LIST LINEAR_CONTRACTS)
      set_tests_properties(${name} PROPERTIES LABELS stream_linear)
    endif()
    if(name IN_LIST NONLINEAR_CONTRACTS)
      set_tests_properties(${name} PROPERTIES LABELS nonlinear)
    endif()
  endif()
endforeach()

# Self-contained operational smoke: unlike most research probes, this one
# generates its own stream and returns nonzero on compression/value regressions.
add_test(NAME probe_linear_stream_smoke COMMAND probe_linear_stream 60000 7)
set_tests_properties(probe_linear_stream_smoke PROPERTIES TIMEOUT 120 LABELS stream_linear)

add_test(NAME probe_streaming_compression_smoke COMMAND probe_streaming_compression 60000 7)
set_tests_properties(probe_streaming_compression_smoke PROPERTIES TIMEOUT 120 LABELS nonlinear)

# Usage:
#   cmake -B build -S .            configure
#   cmake --build build --config Release              build every contract + probe
#   ctest --test-dir build -C Release --output-on-failure        run GNNv2 contract gates + smokes
#   ctest --test-dir build -C Release -L stream_linear           run only the linear suite
#   ctest --test-dir build -C Release -L nonlinear               run only the nonlinear suite
