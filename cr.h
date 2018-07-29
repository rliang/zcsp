#pragma once

#include <stdlib.h>
#include <stdarg.h>

struct zcr;

size_t zcr_mem();

struct zcr *zcr_current();

void zcr_spawn_full(struct zcr *cr, void *stk, size_t stk_size,
		    void (*free_cr)(struct zcr *), void (*free_stk)(void *),
		    void (*proc)(va_list), ...);

void zcr_spawn(void (*proc)(va_list), ...);

void zcr_spawn_flush();

void zcr_resume(struct zcr *cr);

void zcr_suspend_current();
