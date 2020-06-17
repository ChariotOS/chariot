set(SOURCES
    main.cpp
		context.cpp
		screen.cpp
)
chariot_bin(lumen)


target_link_libraries(lumen libgui)
