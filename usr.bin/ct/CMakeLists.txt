set(SOURCES main.cpp)

chariot_bin(ct)
target_link_libraries(ct PUBLIC libui)

add_res(ct cat.png)
