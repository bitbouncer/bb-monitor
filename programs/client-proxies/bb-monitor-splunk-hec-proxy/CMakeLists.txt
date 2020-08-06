add_executable(bb-monitor-splunk-hec-proxy main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-splunk-hec-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-splunk-hec-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-splunk-hec-proxy  RUNTIME DESTINATION bin)
