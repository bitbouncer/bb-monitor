add_executable(bb-monitor-dd-log-proxy main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-dd-log-proxy ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-dd-log-proxy ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-dd-log-proxy RUNTIME DESTINATION bin)
