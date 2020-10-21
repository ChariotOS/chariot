set(SOURCES main.cpp)
chariot_bin(ct)
# target_link_libraries(ct libui libgl libglu)
target_link_libraries(ct PUBLIC libui)



