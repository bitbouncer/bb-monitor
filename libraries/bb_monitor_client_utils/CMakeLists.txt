FILE(GLOB LIB_SRCS
        *.cpp
        *.h
        )

if (BUILD_SHARED_LIBS)
    ADD_LIBRARY(bb-monitor-client-utils SHARED ${LIB_SRCS})
    INSTALL(TARGETS bb-monitor-client-utils
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
    add_dependencies(bb-monitor-client-utils bb-monitor-proto)
endif ()

if (BUILD_STATIC_LIBS)
    ADD_LIBRARY(bb-monitor-client-utils_s STATIC ${LIB_SRCS})
    INSTALL(TARGETS bb-monitor-client-utils_s
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
    add_dependencies(bb-monitor-client-utils_s bb-monitor-proto_s)
endif ()



