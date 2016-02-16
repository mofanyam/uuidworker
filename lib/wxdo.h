//
// Created by mofan on 2/15/16.
//

#ifndef UUIDWORKER_TRY_H
#define UUIDWORKER_TRY_H



#include <stdio.h>
#include <setjmp.h>


#define TRY do{ jmp_buf ex_buf__={0}; if( !setjmp(ex_buf__) ){
#define CATCH } else {
#define ETRY } }while(0)
#define THROW longjmp(ex_buf__, 1)


#define wx_do(msg, statment) TRY        \
{                                       \
statment                                \
fprintf(stderr, msg" [ success ]\n");   \
}                                       \
CATCH                                   \
{                                       \
fprintf(stderr, msg" [ failed ]\n");    \
}                                       \
ETRY;

#define wx_break THROW

#endif //UUIDWORKER_TRY_H
