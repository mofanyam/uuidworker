//
// Created by mofan on 5/17/16.
//

#ifndef WXWORKER_IP_H
#define WXWORKER_IP_H


#include <stdio.h>
#include <stdint.h>


int ip2int(char const* ip16char, int32_t* const ip);

int int2ip(int32_t const ip, char* const buf16char);


#endif //WXWORKER_IP_H
