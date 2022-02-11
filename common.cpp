//
// Created by WAN on 2021/12/12.
//

#include "common.h"

void Dprint(const char *cmd, ...)
{
    if(Debug > 0) {
        va_list args;
        va_start(args, cmd);

        while(*cmd) {
            switch(*cmd) {
                case '%':
                    switch(*(cmd+1)) {
                        case 'd': {
                            int v = va_arg(args, int);
                            printf("%d", v);
                            cmd += 2;
                            break;
                        }
                        case 's': {
                            char *v = va_arg(args, char *);
                            printf("%s", v);
                            cmd += 2;
                            break;
                        }
                    }
                    break;
                default:
                    printf("%c", *cmd);
                    cmd++;
                    break;
            }
        }

        va_end(args);
    }
}