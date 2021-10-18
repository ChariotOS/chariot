set(SOURCES main.cpp)


chariot_bin(ct)
target_link_libraries(ct PUBLIC libui libscm)

# add_res(ct res.txt ct::resource)
