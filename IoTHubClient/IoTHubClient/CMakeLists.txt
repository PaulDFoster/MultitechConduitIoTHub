#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists.txt for IoTHubClient

cmake_minimum_required(VERSION 2.8.11)

if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")
        if (NOT IN_OPENWRT)
            set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
        endif()
    endif()
else()
    set (CMAKE_C_STANDARD 99)
    set (CMAKE_CXX_STANDARD 11)
endif()

set(IoTHubClient_c_files
    	base64Decode.c
	uuid4.c
	IoTHubClient.c
)

set(IoTHubClient_h_files
	mosquitto.h
	uuid4.h
	base64Decode.h
)

include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/deps/parson)
include_directories(${PROJECT_SOURCE_DIR}/inc/)
include_directories(${PROJECT_SOURCE_DIR}/provisioning_client/inc/)
include_directories(${IOTHUB_CLIENT_INC_FOLDER})
include(GNUInstallDirs)

link_directories(${PROJECT_SOURCE_DIR}/deps )

add_executable(IoTHubClient ${IoTHubClient_c_files} ${IoTHubClient_h_files})

target_link_libraries(IoTHubClient
libmosquitto.so.1
iothub_client_mqtt_transport
iothub_client_amqp_transport
iothub_client_http_transport
iothub_client_amqp_ws_transport
iothub_client_mqtt_ws_transport
iothub_client
parson
umqtt
uamqp
aziotsharedutil
uuid
rt
m
pthread
crypto
ssl
curl
)

