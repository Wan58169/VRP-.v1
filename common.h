//
// Created by WAN on 2021/12/12.
//

#ifndef VRP_COMMON_H
#define VRP_COMMON_H

#include <stdio.h>
#include <stdarg.h>

#define BUF_SIZE 256

const int TaskEnd = -1;
const int TaskWait = -2;
const int TaskNonHandle = -3;

const int Debug = 0;

/* for printf */
void Dprint(const char *cmd, ...);

#endif //VRP_COMMON_H
