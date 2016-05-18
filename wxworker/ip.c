//
// Created by mofan on 5/17/16.
//

#include "ip.h"


int ip2int(char const* ip16char, int32_t* const ip) {
    uint8_t* ips = (uint8_t*)ip;
    return 4 == sscanf(ip16char, "%hhu.%hhu.%hhu.%hhu",ips,ips+1,ips+2,ips+3);
}

int int2ip(int32_t const ip, char* const buf16char) {
    uint8_t* ips = (uint8_t*)&ip;
    return 4 == sprintf(buf16char, "%hhu.%hhu.%hhu.%hhu", *ips, *(ips+1), *(ips+2), *(ips+3));
}