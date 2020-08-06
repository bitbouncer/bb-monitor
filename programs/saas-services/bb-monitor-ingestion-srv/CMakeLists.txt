add_executable(bb-monitor-ingestion-srv 
bb_kafka_producer.cpp
kafka_sink_facory.cpp
main.cpp
)

if (LINK_SHARED)
    target_link_libraries(bb-monitor-ingestion-srv ${BB_MONITOR_SERV_LIBS_SHARED})
else ()
    target_link_libraries(bb-monitor-ingestion-srv ${BB_MONITOR_SERV_LIBS_STATIC})
endif ()

INSTALL(TARGETS bb-monitor-ingestion-srv RUNTIME DESTINATION bin)
