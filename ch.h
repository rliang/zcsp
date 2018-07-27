#pragma once

#include <stdlib.h>

size_t zch_mem();

void zch_init_full(void *mem);

void zch_init(int nids);

void zch_free_full(void (*free)(void *));

void zch_free();

void *zch_data(int id);

int zch_choose(int deadline, int nids, ...);

void zch_put(int id, void *data);

void zch_put_flush();

int zch_deadline();

void zch_deadline_pop();
