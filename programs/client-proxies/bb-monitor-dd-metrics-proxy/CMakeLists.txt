add_executable(bb-monitor-dd-metrics-proxy main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-dd-metrics-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-dd-metrics-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-dd-metrics-proxy RUNTIME DESTINATION bin)
