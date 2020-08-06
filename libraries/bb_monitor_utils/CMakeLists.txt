FILE(GLOB LIB_SRCS
        avro/*.h
        splunk/*.h
        splunk/*.cpp
        nagios/*.h
        nagios/*.cpp
        *.cpp
        *.h
        )

if (BUILD_SHARED_LIBS)
    ADD_LIBRARY(bb-monitor-utils SHARED ${LIB_SRCS})
    INSTALL(TARGETS bb-monitor-utils
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()

if (BUILD_STATIC_LIBS)
    ADD_LIBRARY(bb-monitor-utils_s STATIC ${LIB_SRCS})
    INSTALL(TARGETS bb-monitor-utils_s
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
endif ()



