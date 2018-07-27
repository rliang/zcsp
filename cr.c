#define _GNU_SOURCE

#include "cr.h"

#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include <search.h>
#include <sys/mman.h>

#ifdef VALGRIND
#include <valgrind/valgrind.h>
#endif

#ifndef STACK_SIZE
#define STACK_SIZE (1 << 14)
#endif

static ucontext_t main_ctx;

static struct zcr {
	ucontext_t ctx;
} *current = NULL;

static struct spawn_queue {
	struct spawn_queue *next, *prev;
	struct zcr *parent, *child;
	void (*proc)(va_list);
	va_list ap;
	void (*free_co)(struct zcr *);
	void (*free_stk)(void *);
} spawn_queue = {NULL};

size_t zcr_mem()
{
	return sizeof(struct zcr);
}

struct zcr *zcr_current()
{
	return current;
}

static void zcr_wrapper()
{
	assert(spawn_queue.next != NULL);
	assert(current == spawn_queue.next->child);
	void (*free_co)(struct zcr *) = spawn_queue.next->free_co;
	void (*free_stk)(void *) = spawn_queue.next->free_stk;
	spawn_queue.next->proc(spawn_queue.next->ap);
	free_stk(current->ctx.uc_stack.ss_sp);
	free_co(current);
	current = NULL;
}

static void zcr_default_free(struct zcr *co)
{
	free(co);
}

static void zcr_default_free_stk(void *stk)
{
	munmap(stk, 0);
}

static void zcr_spawn_va(struct zcr *co, void *stk, size_t stk_size,
			void (*free_co)(struct zcr *), void (*free_stk)(void *),
			void (*proc)(va_list), va_list ap)
{
	getcontext(&co->ctx);
	co->ctx.uc_link = &main_ctx;
	co->ctx.uc_stack.ss_size = stk_size;
	co->ctx.uc_stack.ss_sp = stk;
#ifdef VALGRIND
	VALGRIND_STACK_REGISTER(co->ctx.uc_stack.ss_sp,
				co->ctx.uc_stack.ss_sp
					+ co->ctx.uc_stack.ss_size);
#endif
	makecontext(&co->ctx, zcr_wrapper, 0);
	struct spawn_queue s = {.parent = current,
				.child = co,
				.proc = proc,
				.free_co = free_co,
				.free_stk = free_stk};
	va_copy(s.ap, ap);
	insque(&s, &spawn_queue);
	if (current == NULL)
		zcr_spawn_flush();
	else
		zcr_suspend_current();
	va_end(s.ap);
}

void zcr_spawn_full(struct zcr *t, void *stk, size_t stk_size,
		   void (*free_co)(struct zcr *), void (*free_stk)(void *),
		   void (*proc)(va_list), ...)
{
	va_list ap;
	va_start(ap, proc);
	zcr_spawn_va(t, stk, stk_size, free_co, free_stk, proc, ap);
	va_end(ap);
}

void zcr_spawn(void (*proc)(va_list), ...)
{
	va_list ap;
	va_start(ap, proc);
	zcr_spawn_va(
		malloc(zcr_mem()),
		mmap(0, STACK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
		     MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN,
		     -1, 0),
		STACK_SIZE, zcr_default_free, zcr_default_free_stk, proc, ap);
	va_end(ap);
}

void zcr_spawn_flush()
{
	assert(current == NULL);
	struct spawn_queue *n = spawn_queue.next;
	while (n != NULL) {
		struct zcr *parent = n->parent, *child = n->child;
		zcr_resume(child);
		remque(n);
		if (parent != NULL)
			zcr_resume(parent);
		n = spawn_queue.next;
	}
}

void zcr_suspend_current()
{
	assert(current != NULL);
	struct zcr *co = current;
	current = NULL;
	swapcontext(&co->ctx, &main_ctx);
}

void zcr_resume(struct zcr *co)
{
	assert(co != NULL);
	assert(current == NULL);
	swapcontext(&main_ctx, &(current = co)->ctx);
}
