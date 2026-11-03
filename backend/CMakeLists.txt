cmake_minimum_required(VERSION 3.20)
project(ChronoPlan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHRONOPLAN_BUILD_TESTS "Build unit tests" ON)

# Dependencies come from vcpkg manifest mode (vcpkg.json).
find_package(Crow CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
# Authentication: Argon2id + CSPRNG + SHA-256 (libsodium), RS256 verification of Google ID tokens
# (OpenSSL) and the HTTPS fetch of Google's signing keys (libcurl).
find_package(unofficial-sodium CONFIG QUIET)
if(TARGET unofficial-sodium::sodium)
    set(CHRONOPLAN_SODIUM_TARGET unofficial-sodium::sodium)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    set(CHRONOPLAN_SODIUM_TARGET PkgConfig::SODIUM)
endif()
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
# Recent CMake names the imported target SQLite3::SQLite3 and deprecates SQLite::SQLite3;
# older CMake (3.20+) only has the latter. Use whichever exists.
if(TARGET SQLite3::SQLite3)
    set(CHRONOPLAN_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(CHRONOPLAN_SQLITE_TARGET SQLite::SQLite3)
endif()

# Embed db/migrations/*.sql into a generated header so the schema lives in one
# place (the .sql file) and the binary needs no files at runtime.
set(CHRONOPLAN_MIGRATION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/db/migrations/001_initial.sql)
set(CHRONOPLAN_MIGRATION_FILE_2 ${CMAKE_CURRENT_SOURCE_DIR}/db/migrations/002_auth.sql)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CHRONOPLAN_MIGRATION_FILE} ${CHRONOPLAN_MIGRATION_FILE_2})
file(READ ${CHRONOPLAN_MIGRATION_FILE} MIGRATION_001)
file(READ ${CHRONOPLAN_MIGRATION_FILE_2} MIGRATION_002)
configure_file(src/database/MigrationsData.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/MigrationsData.hpp @ONLY)

# Shared compiler settings.
add_library(chronoplan_warnings INTERFACE)
if(MSVC)
    target_compile_options(chronoplan_warnings INTERFACE /W4 /permissive- /utf-8)
    # NOMINMAX: stop <windows.h> (pulled in by asio) from defining min/max macros.
    target_compile_definitions(chronoplan_warnings INTERFACE
        _WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(chronoplan_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Phase 3 algorithms: custom data structures. They depend only on the standard library and
# models/Validation.hpp (Id, ValidationError) -- no Crow, no SQLite.
add_library(chronoplan_algorithms STATIC
    src/algorithms/IntervalTree.cpp
    src/algorithms/DependencyGraph.cpp
)
target_include_directories(chronoplan_algorithms PUBLIC include)
target_link_libraries(chronoplan_algorithms PUBLIC nlohmann_json::nlohmann_json chronoplan_warnings)

# Phase 4: the scheduling engine. Depends only on the algorithms + header-only models (no Crow,
# no SQLite); services::SchedulingService (in chronoplan_core) is the only part that reads repositories.
add_library(chronoplan_scheduler STATIC
    src/scheduler/SchedulingObjective.cpp
    src/scheduler/SchedulingConstraints.cpp
    src/scheduler/SchedulingContext.cpp
    src/scheduler/TaskPlacement.cpp
    src/scheduler/DeadlineAnalyzer.cpp
    src/scheduler/ScheduleBuilder.cpp
    src/scheduler/SchedulingValidator.cpp
    src/scheduler/SchedulingEngine.cpp
)
target_include_directories(chronoplan_scheduler PUBLIC include)
target_link_libraries(chronoplan_scheduler PUBLIC chronoplan_algorithms)

# Core library: everything except main(), so tests can link against it.
add_library(chronoplan_core STATIC
    src/api/ApiResponse.cpp
    src/api/RouteHelpers.cpp
    src/api/ServerConfig.cpp
    src/api/Routes.cpp
    src/api/TaskRoutes.cpp
    src/api/EventRoutes.cpp
    src/api/DependencyRoutes.cpp
    src/api/ScheduleRoutes.cpp
    src/api/AuthMiddleware.cpp
    src/api/AuthRoutes.cpp
    src/auth/AuthConfig.cpp
    src/auth/AuthService.cpp
    src/auth/Cookies.cpp
    src/auth/Crypto.cpp
    src/auth/GoogleTokenVerifier.cpp
    src/auth/HttpJwksProvider.cpp
    src/auth/PasswordHasher.cpp
    src/auth/RateLimiter.cpp
    src/auth/RsaVerify.cpp
    src/auth/SessionRepository.cpp
    src/auth/UserRepository.cpp
    src/auth/Validation.cpp
    src/database/Database.cpp
    src/database/DatabaseManager.cpp
    src/database/Migrations.cpp
    src/database/SQLiteStatement.cpp
    src/database/SQLiteTransaction.cpp
    src/repositories/DependencyRepository.cpp
    src/repositories/EventRepository.cpp
    src/repositories/TaskRepository.cpp
    src/services/SchedulingDataProvider.cpp
    src/services/SchedulingService.cpp
)
target_include_directories(chronoplan_core PUBLIC include src PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(chronoplan_core PUBLIC chronoplan_scheduler chronoplan_algorithms Crow::Crow nlohmann_json::nlohmann_json ${CHRONOPLAN_SQLITE_TARGET}
    ${CHRONOPLAN_SODIUM_TARGET} OpenSSL::Crypto CURL::libcurl chronoplan_warnings)

add_executable(chronoplan main.cpp)
target_link_libraries(chronoplan PRIVATE chronoplan_core)

if(CHRONOPLAN_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    # Model tests need only the header-only models + nlohmann/json (no Crow).
    add_executable(chronoplan_model_tests tests/test_models.cpp)
    target_include_directories(chronoplan_model_tests PRIVATE include)
    target_link_libraries(chronoplan_model_tests PRIVATE
        chronoplan_warnings nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
    add_test(NAME chronoplan_model_tests COMMAND chronoplan_model_tests)

    # API tests drive the Crow router in-process (no socket is opened) against a
    # temporary SQLite file; database/repository tests use temporary files too.
    foreach(t api server_config database task_repository event_repository dependency_repository api_crud
             auth_core auth_service auth_google auth_api auth_isolation)
        add_executable(chronoplan_${t}_tests tests/test_${t}.cpp tests/TestDb.hpp)
        target_link_libraries(chronoplan_${t}_tests PRIVATE
            chronoplan_core GTest::gtest GTest::gtest_main)
        add_test(NAME chronoplan_${t}_tests COMMAND chronoplan_${t}_tests)
    endforeach()

    # Phase 3: algorithm tests need no database; the provider test uses a temp SQLite file.
    foreach(t interval_tree priority_queue dependency_graph)
        add_executable(chronoplan_${t}_tests tests/test_${t}.cpp tests/AlgoTestUtil.hpp)
        target_link_libraries(chronoplan_${t}_tests PRIVATE
            chronoplan_algorithms GTest::gtest GTest::gtest_main)
        add_test(NAME chronoplan_${t}_tests COMMAND chronoplan_${t}_tests)
    endforeach()
    add_executable(chronoplan_scheduling_data_provider_tests
        tests/test_scheduling_data_provider.cpp tests/TestDb.hpp)
    target_link_libraries(chronoplan_scheduling_data_provider_tests PRIVATE
        chronoplan_core GTest::gtest GTest::gtest_main)
    add_test(NAME chronoplan_scheduling_data_provider_tests COMMAND chronoplan_scheduling_data_provider_tests)

    # Phase 4: scheduling engine and validator (no database), then the REST route (temporary SQLite).
    foreach(t scheduling_engine scheduling_validator)
        add_executable(chronoplan_${t}_tests tests/test_${t}.cpp tests/SchedTestUtil.hpp)
        target_link_libraries(chronoplan_${t}_tests PRIVATE
            chronoplan_scheduler GTest::gtest GTest::gtest_main)
        add_test(NAME chronoplan_${t}_tests COMMAND chronoplan_${t}_tests)
    endforeach()
    add_executable(chronoplan_scheduling_api_tests
        tests/test_scheduling_api.cpp tests/SchedTestUtil.hpp tests/TestDb.hpp)
    target_link_libraries(chronoplan_scheduling_api_tests PRIVATE
        chronoplan_core GTest::gtest GTest::gtest_main)
    add_test(NAME chronoplan_scheduling_api_tests COMMAND chronoplan_scheduling_api_tests)
endif()
