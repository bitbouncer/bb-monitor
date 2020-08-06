add_executable(bb-monitor-promstore-write-proxy main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-promstore-write-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-promstore-write-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-promstore-write-proxy RUNTIME DESTINATION bin)
