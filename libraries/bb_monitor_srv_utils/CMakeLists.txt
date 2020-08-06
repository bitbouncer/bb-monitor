FILE(GLOB LIB_SRCS
        *.cpp
        *.h
        )

if (BUILD_SHARED_LIBS)
    ADD_LIBRARY(bb-monitor-srv-utils SHARED ${LIB_SRCS})
    INSTALL(TARGETS bb-monitor-srv-utils
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()

if (BUILD_STATIC_LIBS)
    ADD_LIBRARY(bb-monitor-srv-utils_s STATIC ${LIB_SRCS})
    INSTALL(TARGETS bb-monitor-srv-utils_s
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()



