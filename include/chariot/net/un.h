#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#define UNIX_PATH_MAX 108
struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[UNIX_PATH_MAX];
};



#ifdef __cplusplus
}
#endif
