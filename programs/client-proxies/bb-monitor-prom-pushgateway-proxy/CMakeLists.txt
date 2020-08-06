add_executable(bb-monitor-prom-pushgateway-proxy main.cpp)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-prom-pushgateway-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-prom-pushgateway-proxy ${RESTINIO_LIBS} ${BB_MONITOR_CLIENT_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-prom-pushgateway-proxy RUNTIME DESTINATION bin)
