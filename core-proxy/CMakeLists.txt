find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL QUIET)
find_package(nlohmann_json 3.11 QUIET)

if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(edgeshield_core
  src/backend/backend_client.cpp
  src/backend/backend_selector.cpp
  src/backend/circuit_breaker.cpp
  src/backend/connection_pool.cpp
  src/cache/response_cache.cpp
  src/config/config.cpp
  src/http/http_helpers.cpp
  src/http/proxy_handler.cpp
  src/metrics/metrics.cpp
  src/metrics/prometheus.cpp
  src/metrics/proxy_log.cpp
  src/ratelimit/rate_limiter.cpp
  src/routing/route_matcher.cpp
  src/state/gateway_state.cpp
)
target_include_directories(edgeshield_core PUBLIC include)
target_link_libraries(edgeshield_core PUBLIC Boost::system nlohmann_json::nlohmann_json Threads::Threads)
if(OpenSSL_FOUND)
  target_link_libraries(edgeshield_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(edgeshield_core PUBLIC EDGESHIELD_WITH_TLS=1)
else()
  target_compile_definitions(edgeshield_core PUBLIC EDGESHIELD_WITH_TLS=0)
endif()
target_compile_options(edgeshield_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(edgeshield-proxy src/main.cpp)
target_link_libraries(edgeshield-proxy PRIVATE edgeshield_core)

include(CTest)
if(BUILD_TESTING)
  find_package(Catch2 3 QUIET)
  if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)
  endif()

  add_executable(edgeshield-tests tests/core_tests.cpp)
  target_link_libraries(edgeshield-tests PRIVATE edgeshield_core Catch2::Catch2WithMain)
  add_test(NAME edgeshield-tests COMMAND edgeshield-tests)
endif()
