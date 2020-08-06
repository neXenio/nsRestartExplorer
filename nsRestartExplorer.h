#ifndef _NS_RESTART_EXPLORER_H
#define _NS_RESTART_EXPLORER_H

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <windows.h>

typedef struct _stack_t {
    struct _stack_t *next;
    TCHAR            text[1];
} stack_t;

extern stack_t **   g_stackTop;
extern unsigned int g_stringLength;

static inline void pushString(const TCHAR *str)
{
    stack_t *th;
    if (!g_stackTop)
        return;
    th = (stack_t *)GlobalAlloc(GPTR, sizeof(stack_t) + g_stringLength);
    lstrcpyn(th->text, str, g_stringLength);
    th->next    = *g_stackTop;
    *g_stackTop = th;
}

extern BOOL RestartExplorer();

#endif /* _NS_RESTART_EXPLORER_H */
