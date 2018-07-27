#define _GNU_SOURCE

#include "ch.h"

#include <stdarg.h>
#include <assert.h>
#include <search.h>

#include "cr.h"

static struct {
	struct r_queue {
		struct r_queue *next, *prev;
		struct zcr *co;
		int *ret;
	} r_queue;
	void *value;
} * channels;

static struct w_queue {
	struct w_queue *next, *prev;
	struct zcr *co;
	int id;
	void *value;
} w_queue;

static struct t_queue {
	struct t_queue *parent, *left, *next;
	struct zcr *co;
	int *ret;
	int deadline;
} * t_queue = NULL;

static struct t_queue *t_queue_merge(struct t_queue *t1, struct t_queue *t2)
{
	if (t1 == NULL)
		return t2;
	if (t2 == NULL)
		return t1;
	if (t1->deadline < t2->deadline) {
		if (t1->left != NULL)
			(t2->next = t1->left)->parent = t2;
		(t1->left = t2)->parent = t1;
		return t1;
	} else {
		if (t2->left != NULL)
			(t1->next = t2->left)->parent = t1;
		(t2->left = t1)->parent = t2;
		return t2;
	}
}

static struct t_queue *t_queue_pop(struct t_queue *t)
{
	if (t == NULL || t->next == NULL)
		return t;
	struct t_queue *a = t->next, *b = t->next->next;
	t->next = (t->next->next = NULL);
	return t_queue_merge(t_queue_merge(t, a), t_queue_pop(b));
}

static struct t_queue *t_queue_remove(struct t_queue *root, struct t_queue *t)
{
	assert(root != NULL);
	assert(t != NULL);
	t->deadline = -1;
	if (t != root) {
		if (t->parent->left == t)
			t->parent->left = NULL;
		else
			t->parent->next = NULL;
		t->parent = NULL;
		t = t_queue_merge(root, t);
	}
	return t_queue_pop(t->left);
}

size_t zch_mem()
{
	return sizeof(channels[0]);
}

void zch_init_full(void *mem)
{
	channels = mem;
}

void zch_init(int nids)
{
	zch_init_full(calloc(nids, zch_mem()));
}

void zch_free_full(void (*free)(void *))
{
	free(channels);
}

void zch_free()
{
	free(channels);
}

void *zch_data(int id)
{
	return channels[id].value;
}

int zch_choose(int deadline, int nids, ...)
{
	assert(zcr_current() != NULL);
	va_list ap;
	va_start(ap, nids);
	int ret;
	struct t_queue t = {.parent = NULL,
			    .left = NULL,
			    .next = NULL,
			    .co = zcr_current(),
			    .ret = &ret,
			    .deadline = deadline};
	t_queue = t_queue_merge(t_queue, &t);
	struct r_queue r[nids];
	for (int i = 0; i < nids; i++) {
		r[i].co = zcr_current();
		r[i].ret = &ret;
		insque(&r[i], &channels[va_arg(ap, int)].r_queue);
	}
	va_end(ap);
	zcr_suspend_current();
	for (int i = 0; i < nids; i++)
		remque(&r[i]);
	if (ret != -1)
		t_queue = t_queue_remove(t_queue, &t);
	return ret;
}

void zch_put(int id, void *data)
{
	struct zcr *t = zcr_current();
	struct w_queue w = {.co = t, .id = id, .value = data};
	insque(&w, &w_queue);
	if (t == NULL)
		zch_put_flush();
	else
		zcr_suspend_current();
}

void zch_put_flush()
{
	assert(zcr_current() == NULL);
	struct w_queue *w = w_queue.next;
	while (w != NULL) {
		struct zcr *t = w->co;
		int id = w->id;
		channels[id].value = w->value;
		struct r_queue *r = channels[id].r_queue.next;
		while (r != NULL) {
			struct r_queue *next = r->next;
			*r->ret = id;
			zcr_resume(r->co);
			r = next;
		}
		remque(w);
		if (t != NULL)
			zcr_resume(t);
		w = w_queue.next;
	}
}

int zch_deadline()
{
	return t_queue == NULL ? -1 : t_queue->deadline;
}

void zch_deadline_pop()
{
	assert(zcr_current() == NULL);
	assert(t_queue != NULL);
	struct zcr *c = t_queue->co;
	*t_queue->ret = -1;
	t_queue = t_queue_pop(t_queue->left);
	zcr_resume(c);
}
