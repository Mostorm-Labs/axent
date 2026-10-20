cmake_minimum_required(VERSION 3.21)
project(Axent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(CTest)
option(AXENT_BUILD_TESTING "Build Axent test executables" ${BUILD_TESTING})
include(cmake/CompilerWarnings.cmake)
include(cmake/AxentDependencies.cmake)

add_library(axent_control_contract STATIC
    src/control/control_contract.cpp
)
add_library(axent::control_contract ALIAS axent_control_contract)
target_include_directories(axent_control_contract PUBLIC include)
target_link_libraries(axent_control_contract PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(axent_control_contract PRIVATE axtp::core)
axent_enable_warnings(axent_control_contract)

add_library(axent_media_contract INTERFACE)
add_library(axent::media_contract ALIAS axent_media_contract)
target_include_directories(axent_media_contract INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(axent_media_contract INTERFACE cxx_std_17)

add_library(axent_firmware STATIC
    src/firmware/firmware_update_service.cpp
    src/firmware/md5.cpp
)
add_library(axent::firmware ALIAS axent_firmware)
target_include_directories(axent_firmware PUBLIC include)
target_compile_features(axent_firmware PUBLIC cxx_std_17)
axent_enable_warnings(axent_firmware)

add_library(axent_axtp_transport_tcp_native INTERFACE)
add_library(axent::transport_tcp_native ALIAS axent_axtp_transport_tcp_native)
target_include_directories(axent_axtp_transport_tcp_native INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/transports>
)
target_link_libraries(axent_axtp_transport_tcp_native INTERFACE axtp::runtime)
if(WIN32)
    target_link_libraries(axent_axtp_transport_tcp_native INTERFACE ws2_32)
endif()

if(TARGET ixwebsocket::ixwebsocket)
    add_library(axent_axtp_transport_websocket_ix INTERFACE)
    add_library(axent::transport_websocket_ix ALIAS axent_axtp_transport_websocket_ix)
    target_include_directories(axent_axtp_transport_websocket_ix INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/transports>
    )
    target_link_libraries(axent_axtp_transport_websocket_ix INTERFACE
        axtp::runtime
        ixwebsocket::ixwebsocket
    )
endif()

if(TARGET hidapi::hidapi)
    add_library(axent_axtp_transport_hidapi STATIC
        src/transports/hidapi/hid_transport.cpp
    )
    add_library(axent::transport_hidapi ALIAS axent_axtp_transport_hidapi)
    target_include_directories(axent_axtp_transport_hidapi PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/transports>
    )
    target_link_libraries(axent_axtp_transport_hidapi
        PUBLIC axtp::runtime
        PRIVATE hidapi::hidapi
    )
    if(WIN32)
        target_link_libraries(axent_axtp_transport_hidapi PRIVATE hid)
    endif()
    axent_enable_warnings(axent_axtp_transport_hidapi)
endif()

add_library(libaxent
    src/version.cpp
    src/control/protocol_codecs.cpp
    src/control/control_plane.cpp
    src/core/json.cpp
    src/core/device_manager.cpp
    src/core/session_manager.cpp
    src/core/capability_registry.cpp
    src/core/adapter_registry.cpp
    src/core/control_operation.cpp
    src/core/route_manager.cpp
    src/core/middleware.cpp
    src/core/flow_control.cpp
    src/core/broker.cpp
    src/config/config.cpp
    src/diagnostics/diagnostics.cpp
    src/firmware/firmware_task.cpp
    src/cli/options.cpp
    src/logging/logger.cpp
    src/adapters/axdp_adapter.cpp
    src/adapters/axtp_endpoint_identity.cpp
    src/adapters/axtp_adapter.cpp
    src/adapters/mock_adapter.cpp
    src/adapters/tea_adapter.cpp
    src/host/axent_host.cpp
    src/media/media_relay.cpp
    src/media/media_subscription.cpp
    src/media/video_stream_params.cpp
)
add_library(axent::libaxent ALIAS libaxent)
target_include_directories(libaxent PUBLIC include)
target_include_directories(libaxent PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/transports>
)
target_link_libraries(libaxent
    PUBLIC nlohmann_json::nlohmann_json axent::media_contract axent::firmware
    PRIVATE axtp::core axtp::json_rpc axtp::runtime axtp::sdk
)
if(TARGET axent::transport_hidapi)
    target_link_libraries(libaxent PRIVATE axent::transport_hidapi)
    target_compile_definitions(libaxent PRIVATE AXENT_HAS_AXTP_HID_TRANSPORT=1)

    add_library(axent_axtp_adapter_test_support STATIC EXCLUDE_FROM_ALL
        src/adapters/axtp_adapter_test_seam.cpp
    )
    add_library(axent::axtp_adapter_test_support ALIAS axent_axtp_adapter_test_support)
    target_include_directories(axent_axtp_adapter_test_support
        PUBLIC
            include
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters>
    )
    target_link_libraries(axent_axtp_adapter_test_support
        PUBLIC axent::libaxent axent::transport_hidapi
    )
    axent_enable_warnings(axent_axtp_adapter_test_support)
endif()

set(AXENT_HAS_AXTP_DIRECT_CLI OFF)
if(TARGET axent::transport_hidapi AND
   TARGET axent::transport_tcp_native AND
   TARGET axent::transport_websocket_ix)
    set(AXENT_HAS_AXTP_DIRECT_CLI ON)
endif()

if(TARGET axtpctl)
    message(FATAL_ERROR
        "The axtpctl target is owned by Axent. Disable the retired cpp-runtime tool target."
    )
endif()

add_library(axent_axtp_tooling STATIC
    src/tooling/axtp_cli.cpp
)
add_library(axent::axtp_tooling ALIAS axent_axtp_tooling)
target_include_directories(axent_axtp_tooling
    PUBLIC include
    PRIVATE src/tooling
)
target_link_libraries(axent_axtp_tooling PRIVATE axent::firmware)
if(AXENT_HAS_AXTP_DIRECT_CLI)
    target_sources(axent_axtp_tooling PRIVATE
        src/firmware/axtp_firmware_backend.cpp
    )
    target_link_libraries(axent_axtp_tooling
        PRIVATE
            axtp::core
            axtp::json_rpc
            axtp::runtime
            axtp::sdk
            axent::transport_tcp_native
            axent::transport_websocket_ix
            axent::transport_hidapi
    )
    target_compile_definitions(axent_axtp_tooling PRIVATE AXENT_HAS_AXTP_DIRECT_CLI=1)
endif()
axent_enable_warnings(axent_axtp_tooling)

if(AXENT_HAS_AXDP_HEADERS)
    target_include_directories(libaxent PRIVATE third_party/axdp/include)
endif()

if(AXENT_HAS_TEA_MACOS_SDK)
    target_include_directories(libaxent PRIVATE third_party/tea/macos/TeaSdkMacOS_1.0.0.36/include)
endif()
axent_enable_warnings(libaxent)

set(AXENT_HAS_WEBSOCKET_TRANSPORT OFF)
if(TARGET axent::transport_websocket_ix)
    set(AXENT_HAS_WEBSOCKET_TRANSPORT ON)

    add_library(axent_axtp_control_endpoint STATIC
        src/control/axtp_control_endpoint.cpp
    )
    add_library(axent::axtp_control_endpoint ALIAS axent_axtp_control_endpoint)
    target_include_directories(axent_axtp_control_endpoint PUBLIC include)
    target_link_libraries(axent_axtp_control_endpoint
        PUBLIC axent::control_contract
        PRIVATE
            axent::libaxent
            axtp::core
            axtp::json_rpc
            axtp::runtime
            axent::transport_websocket_ix
    )
    axent_enable_warnings(axent_axtp_control_endpoint)

    add_library(axent_websocket_control
        src/control/websocket_server.cpp
    )
    target_include_directories(axent_websocket_control PUBLIC include)
    target_link_libraries(axent_websocket_control
        PUBLIC axent::libaxent
        PRIVATE ixwebsocket::ixwebsocket
    )
    axent_enable_warnings(axent_websocket_control)

    add_executable(axentd src/daemon/main.cpp)
    target_link_libraries(axentd PRIVATE axent_websocket_control)
    axent_enable_warnings(axentd)
endif()

add_executable(axent src/cli/main.cpp)
target_link_libraries(axent PRIVATE axent::libaxent axent::axtp_tooling)
axent_enable_warnings(axent)

if(AXENT_HAS_AXTP_DIRECT_CLI)
    add_executable(axtpctl src/cli/axtpctl_main.cpp)
    target_link_libraries(axtpctl PRIVATE axent::axtp_tooling)
    axent_enable_warnings(axtpctl)
endif()

if(AXENT_BUILD_TESTING)
    add_test(
        NAME axent_dependency_boundary
        COMMAND ${CMAKE_COMMAND}
            -DAXENT_REPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/verify_axent_dependency_boundary.cmake
    )

    add_executable(version_smoke_test tests/version_smoke_test.cpp)
    target_link_libraries(version_smoke_test PRIVATE axent::libaxent)
    axent_enable_warnings(version_smoke_test)
    add_test(NAME version_smoke_test COMMAND version_smoke_test)

    add_executable(core_types_test tests/core_types_test.cpp)
    target_link_libraries(core_types_test PRIVATE axent::libaxent)
    axent_enable_warnings(core_types_test)
    add_test(NAME core_types_test COMMAND core_types_test)

    add_executable(transport_media_types_test tests/transport_media_types_test.cpp)
    target_link_libraries(transport_media_types_test PRIVATE axent::libaxent)
    axent_enable_warnings(transport_media_types_test)
    add_test(NAME transport_media_types_test COMMAND transport_media_types_test)

    add_executable(tcp_transport_test tests/tcp_transport_test.cpp)
    target_link_libraries(tcp_transport_test PRIVATE axent::transport_tcp_native)
    axent_enable_warnings(tcp_transport_test)
    add_test(NAME tcp_transport_test COMMAND tcp_transport_test)

    if(TARGET axent::transport_hidapi)
        add_executable(hid_transport_test tests/hid_transport_test.cpp)
        target_link_libraries(hid_transport_test PRIVATE axent::transport_hidapi)
        axent_enable_warnings(hid_transport_test)
        add_test(NAME hid_transport_test COMMAND hid_transport_test)
    endif()

    add_executable(media_relay_test tests/media_relay_test.cpp)
    target_link_libraries(media_relay_test PRIVATE axent::libaxent)
    axent_enable_warnings(media_relay_test)
    add_test(NAME media_relay_test COMMAND media_relay_test)

    add_executable(axent_host_test tests/axent_host_test.cpp)
    target_link_libraries(axent_host_test PRIVATE axent::libaxent axent::axtp_adapter_test_support)
    axent_enable_warnings(axent_host_test)
    add_test(NAME axent_host_test COMMAND axent_host_test)

    add_executable(firmware_update_service_test tests/firmware_update_service_test.cpp)
    target_link_libraries(firmware_update_service_test PRIVATE axent::firmware)
    axent_enable_warnings(firmware_update_service_test)
    add_test(NAME firmware_update_service_test COMMAND firmware_update_service_test)

    add_executable(maintenance_lease_test tests/maintenance_lease_test.cpp)
    target_link_libraries(maintenance_lease_test PRIVATE axent::libaxent)
    axent_enable_warnings(maintenance_lease_test)
    add_test(NAME maintenance_lease_test COMMAND maintenance_lease_test)

    add_executable(manager_registry_test tests/manager_registry_test.cpp)
    target_link_libraries(manager_registry_test PRIVATE axent::libaxent)
    axent_enable_warnings(manager_registry_test)
    add_test(NAME manager_registry_test COMMAND manager_registry_test)

    add_executable(mock_adapter_test tests/mock_adapter_test.cpp)
    target_link_libraries(mock_adapter_test PRIVATE axent::libaxent)
    axent_enable_warnings(mock_adapter_test)
    add_test(NAME mock_adapter_test COMMAND mock_adapter_test)

    add_executable(diagnostics_config_test tests/diagnostics_config_test.cpp)
    target_link_libraries(diagnostics_config_test PRIVATE axent::libaxent)
    axent_enable_warnings(diagnostics_config_test)
    add_test(NAME diagnostics_config_test COMMAND diagnostics_config_test)

    add_executable(logging_foundation_test tests/logging_foundation_test.cpp)
    target_link_libraries(logging_foundation_test PRIVATE axent::libaxent)
    axent_enable_warnings(logging_foundation_test)
    add_test(NAME logging_foundation_test COMMAND logging_foundation_test)

    add_executable(cli_options_test tests/cli_options_test.cpp)
    target_link_libraries(cli_options_test PRIVATE axent::libaxent)
    axent_enable_warnings(cli_options_test)
    add_test(NAME cli_options_test COMMAND cli_options_test)

    if(AXENT_HAS_AXTP_DIRECT_CLI)
        add_executable(axtp_firmware_backend_test tests/axtp_firmware_backend_test.cpp)
        target_include_directories(axtp_firmware_backend_test PRIVATE src/firmware)
        target_link_libraries(axtp_firmware_backend_test
            PRIVATE axent::axtp_tooling axtp::core)
        axent_enable_warnings(axtp_firmware_backend_test)
        add_test(NAME axtp_firmware_backend_test COMMAND axtp_firmware_backend_test)

        add_executable(axtp_cli_runner_test tests/axtp_cli_runner_test.cpp)
        target_link_libraries(axtp_cli_runner_test PRIVATE axent::axtp_tooling axtp::core)
        target_compile_definitions(axtp_cli_runner_test PRIVATE AXENT_HAS_AXTP_FIRMWARE_SERVICE=1)
        axent_enable_warnings(axtp_cli_runner_test)
        add_test(NAME axtp_cli_runner_test COMMAND axtp_cli_runner_test)
    endif()

    add_executable(broker_flow_test tests/broker_flow_test.cpp)
    target_link_libraries(broker_flow_test PRIVATE axent::libaxent)
    axent_enable_warnings(broker_flow_test)
    add_test(NAME broker_flow_test COMMAND broker_flow_test)

    add_executable(protocol_codecs_test tests/protocol_codecs_test.cpp)
    target_link_libraries(protocol_codecs_test PRIVATE axent::libaxent)
    axent_enable_warnings(protocol_codecs_test)
    add_test(NAME protocol_codecs_test COMMAND protocol_codecs_test)

    add_executable(control_contract_test tests/control_contract_test.cpp)
    target_link_libraries(control_contract_test PRIVATE axent::control_contract axtp::core)
    axent_enable_warnings(control_contract_test)
    add_test(NAME control_contract_test COMMAND control_contract_test)

    if(AXENT_HAS_WEBSOCKET_TRANSPORT)
        add_executable(websocket_transport_test tests/websocket_transport_test.cpp)
        target_link_libraries(websocket_transport_test
            PRIVATE axent::transport_websocket_ix ixwebsocket::ixwebsocket)
        axent_enable_warnings(websocket_transport_test)
        add_test(NAME websocket_transport_test COMMAND websocket_transport_test)

        add_executable(axtp_control_endpoint_test tests/axtp_control_endpoint_test.cpp)
        target_link_libraries(axtp_control_endpoint_test
            PRIVATE
                axent::axtp_control_endpoint
                axent::libaxent
                axtp::runtime
                ixwebsocket::ixwebsocket)
        axent_enable_warnings(axtp_control_endpoint_test)
        add_test(NAME axtp_control_endpoint_test COMMAND axtp_control_endpoint_test)

        add_executable(websocket_server_test tests/websocket_server_test.cpp)
        target_link_libraries(websocket_server_test
            PRIVATE axent_websocket_control ixwebsocket::ixwebsocket)
        axent_enable_warnings(websocket_server_test)
        add_test(NAME websocket_server_test COMMAND websocket_server_test)
    endif()

    add_executable(cli_smoke_test tests/cli_smoke_test.cpp)
    axent_enable_warnings(cli_smoke_test)
    if(AXENT_HAS_AXTP_DIRECT_CLI)
        add_test(NAME cli_smoke_test
            COMMAND cli_smoke_test $<TARGET_FILE:axent> $<TARGET_FILE:axtpctl>)
    else()
        add_test(NAME cli_smoke_test COMMAND cli_smoke_test $<TARGET_FILE:axent>)
    endif()

    add_executable(adapter_skeleton_test tests/adapter_skeleton_test.cpp)
    target_link_libraries(adapter_skeleton_test PRIVATE axent::libaxent)
    axent_enable_warnings(adapter_skeleton_test)
    add_test(NAME adapter_skeleton_test COMMAND adapter_skeleton_test)

    add_executable(axtp_real_adapter_test tests/axtp_real_adapter_test.cpp)
    target_link_libraries(axtp_real_adapter_test PRIVATE axent::libaxent axent::axtp_adapter_test_support)
    axent_enable_warnings(axtp_real_adapter_test)
    add_test(NAME axtp_real_adapter_test COMMAND axtp_real_adapter_test)

    add_executable(axtp_endpoint_identity_test tests/axtp_endpoint_identity_test.cpp)
    target_link_libraries(axtp_endpoint_identity_test PRIVATE axent::libaxent)
    axent_enable_warnings(axtp_endpoint_identity_test)
    add_test(NAME axtp_endpoint_identity_test COMMAND axtp_endpoint_identity_test)
endif()
