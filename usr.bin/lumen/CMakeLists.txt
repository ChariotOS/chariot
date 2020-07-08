set(SOURCES
    main.cpp
		context.cpp
		window.cpp
		screen.cpp
)
chariot_bin(lumen)


target_link_libraries(lumen libui)
