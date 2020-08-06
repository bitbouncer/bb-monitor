add_executable(bb-monitor-es-log-exporter main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-es-log-exporter ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-es-log-exporter ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-es-log-exporter RUNTIME DESTINATION bin)
