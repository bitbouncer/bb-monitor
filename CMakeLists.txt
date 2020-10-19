cmake_minimum_required(VERSION 2.8.11)
project(bb-monitor)

SET(CMAKE_CXX_STANDARD 17)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

option(BUILD_STATIC_LIBS "enable static linking support" ON)
option(BUILD_SHARED_LIBS "enable shared linking support" OFF)
option(LINK_SHARED "enable static linking support" OFF)

#GLOBAL DEFINITIONS
add_definitions(-DRAPIDJSON_HAS_STDSTRING)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wpedantic -Wreturn-type -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wpedantic -Wreturn-type -g -O0")

#find_package(Boost COMPONENTS program_options system date_time REQUIRED)
#set(BOOST_LIBS
#        ${Boost_PROGRAM_OPTIONS_LIBRARY}
#        ${Boost_SYSTEM_LIBRARY}
#        ${Boost_DATE_TIME}
#        boost_date_time
#        pthread
#        rt
#        c
#        )
set(BOOST_LIBS boost_program_options boost_iostreams boost_regex boost_system z bz2 pthread rt c)

# avro + schema registry
find_package(rapidjson REQUIRED)
find_package(AvroCPP REQUIRED)

find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")
find_package(GRPC REQUIRED)

SET(PROTOS
  ./proto/bb_streaming.proto
  ./proto/bb_monitor_sink.proto
)

SET(PROMETHEUS_PROTOS
        ./proto/remote.proto
        ./proto/types.proto
        ./proto/gogo.proto)

set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR})

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${PROTOS} ${PROMETHEUS_PROTOS})

# Print path to generated files
message("PROTO_SRCS = ${PROTO_SRCS}")
message("PROTO_HDRS = ${PROTO_HDRS}")

grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})
message("GRPC_SRCS = ${GRPC_SRCS}")
message("GRPC_HDRS = ${GRPC_HDRS}")

#include protobuf generated files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(PostgreSQL REQUIRED)
include_directories(${PostgreSQL_INCLUDE_DIRS})

find_package(absl REQUIRED)

if (BUILD_STATIC_LIBS)
    ADD_LIBRARY(bb-monitor-proto_s STATIC ${PROTO_SRCS} ${GRPC_SRCS})
    INSTALL(TARGETS bb-monitor-proto_s
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()

if (BUILD_SHARED_LIBS)
    ADD_LIBRARY(bb-monitor-proto SHARED ${PROTO_SRCS} ${GRPC_SRCS})
    INSTALL(TARGETS bb-monitor-proto
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()


SET(EXT_INCLUDE_PATH
        ${Boost_INCLUDE_DIR}
        ${RAPIDJSON_INCLUDE_DIR}
        ${AVRO_INCLUDE_DIRS}
        )

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

add_definitions(-D_FILE_OFFSET_BITS=64 -D_REENTRANT -DEXTERNAL_LOCKS -DMULTITHREAD)
add_definitions(-fPIC)
add_definitions(-Wno-deprecated)
add_definitions(-g)

SET(ROCKSDB_LIBS rocksdb snappy bz2 z)
SET(AVRO_LIBS avrocpp curl snappy)
SET(LIBRDKAFKA_LIBS rdkafka++ rdkafka)
SET(PROMETHEUS_CPP_LIBS prometheus-cpp-core prometheus-cpp-push)
SET(PostgreSQL_LIBRARIES pq)
SET(AWS_S3_LIBS aws-cpp-sdk-s3 aws-cpp-sdk-core aws-c-event-stream aws-checksums aws-c-common)

SET(BB_GRPC_LIBS gRPC::grpc++_reflection gRPC::grpc++ gRPC::grpc protobuf::libprotobuf gpr upb address_sorting re2 boringssl-ssl boringssl-crypto)
SET(ABSEIL_LIBS absl::time absl::status absl::synchronization absl::strings absl::str_format absl::memory absl::base)
SET(EXT_LIBS ${AVRO_LIBS} ${AWS_S3_LIBS} ${ROCKSDB_LIBS} ${LIBRDKAFKA_LIBS} ${PROMETHEUS_CPP_LIBS} ${BOOST_LIBS} ${PostgreSQL_LIBRARIES} ${BB_GRPC_LIBS} ${ABSEIL_LIBS} cares glog gflags  stdc++fs dl)

add_definitions(-DRESTINIO_USE_BOOST_ASIO)
SET(RESTINIO_LIBS fmt http_parser)

SET(BB_MONITOR_SERV_LIBS_SHARED bb-monitor-srv-utils   bb-monitor-client-utils   bb-monitor-utils bb-monitor-proto kspp_grpc kspp      ${EXT_LIBS})
SET(BB_MONITOR_SERV_LIBS_STATIC bb-monitor-srv-utils_s bb-monitor-client-utils_s bb-monitor-utils_s bb-monitor-proto_s kspp_grpc_s kspp_s  ${EXT_LIBS})

SET(BB_MONITOR_CLIENT_LIBS_SHARED bb-monitor-client-utils bb-monitor-utils bb-monitor-proto kspp_grpc kspp ${EXT_LIBS})
SET(BB_MONITOR_CLIENT_LIBS_STATIC bb-monitor-client-utils_s bb-monitor-utils_s bb-monitor-proto_s kspp_grpc_s kspp_s ${EXT_LIBS})

include_directories(${EXT_INCLUDE_PATH} ${CMAKE_SOURCE_DIR}/libraries)

add_subdirectory(libraries)
add_subdirectory(programs)






