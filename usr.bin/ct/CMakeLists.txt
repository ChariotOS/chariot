set(SOURCES main.cpp)

chariot_bin(ct)
target_link_libraries(ct PUBLIC libui)

# include res.txt in the binary in a known location
add_res(ct res.txt)
