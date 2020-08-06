add_executable(bb-monitor-promstore-exporter main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-promstore-exporter ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-promstore-exporter ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-promstore-exporter RUNTIME DESTINATION bin)
