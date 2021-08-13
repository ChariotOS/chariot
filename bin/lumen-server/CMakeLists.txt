

set(SOURCES
    main.cpp
	server.cpp
	window.cpp
	screen.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
chariot_bin(lumen-server)


target_link_libraries(lumen-server PUBLIC libui)
