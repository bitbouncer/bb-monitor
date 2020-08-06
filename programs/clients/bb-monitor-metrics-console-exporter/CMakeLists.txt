add_executable(bb-monitor-metrics-console-exporter main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-metrics-console-exporter ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-metrics-console-exporter ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-metrics-console-exporter RUNTIME DESTINATION bin)
