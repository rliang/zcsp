#define _GNU_SOURCE

#include "ch.h"

#include <stdarg.h>
#include <assert.h>
#include <search.h>

#include "cr.h"

static struct {
	struct recv_queue {
		struct recv_queue *next, *prev;
		struct zcr *cr;
		int *ret;
	} recv_queue;
	void *value;
} * channels;

static struct send_queue {
	struct send_queue *next, *prev;
	struct zcr *cr;
	int id;
	void *value;
} send_queue;

static struct time_queue {
	struct time_queue *parent, *left, *next;
	struct zcr *cr;
	int *ret;
	int deadline;
} *time_queue = NULL;

static struct time_queue *time_queue_merge(struct time_queue *t1,
					   struct time_queue *t2)
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

static struct time_queue *time_queue_pop(struct time_queue *t)
{
	if (t == NULL || t->next == NULL)
		return t;
	struct time_queue *a = t->next, *b = t->next->next;
	t->next = (t->next->next = NULL);
	return time_queue_merge(time_queue_merge(t, a), time_queue_pop(b));
}

static struct time_queue *time_queue_remove(struct time_queue *root,
					    struct time_queue *t)
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
		t = time_queue_merge(root, t);
	}
	return time_queue_pop(t->left);
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
	struct time_queue t = {.parent = NULL,
			       .left = NULL,
			       .next = NULL,
			       .cr = zcr_current(),
			       .ret = &ret,
			       .deadline = deadline};
	time_queue = time_queue_merge(time_queue, &t);
	struct recv_queue r[nids];
	for (int i = 0; i < nids; i++) {
		r[i].cr = zcr_current();
		r[i].ret = &ret;
		insque(&r[i], &channels[va_arg(ap, int)].recv_queue);
	}
	va_end(ap);
	zcr_suspend_current();
	for (int i = 0; i < nids; i++)
		remque(&r[i]);
	if (ret != -1)
		time_queue = time_queue_remove(time_queue, &t);
	return ret;
}

void zch_put(int id, void *data)
{
	struct zcr *t = zcr_current();
	struct send_queue w = {.cr = t, .id = id, .value = data};
	insque(&w, &send_queue);
	if (t == NULL)
		zch_put_flush();
	else
		zcr_suspend_current();
}

void zch_put_flush()
{
	assert(zcr_current() == NULL);
	struct send_queue *w = send_queue.next;
	while (w != NULL) {
		struct zcr *t = w->cr;
		int id = w->id;
		channels[id].value = w->value;
		struct recv_queue *r = channels[id].recv_queue.next;
		while (r != NULL) {
			struct recv_queue *next = r->next;
			*r->ret = id;
			zcr_resume(r->cr);
			r = next;
		}
		remque(w);
		if (t != NULL)
			zcr_resume(t);
		w = send_queue.next;
	}
}

int zch_deadline()
{
	return time_queue == NULL ? -1 : time_queue->deadline;
}

void zch_deadline_pop()
{
	assert(zcr_current() == NULL);
	assert(time_queue != NULL);
	struct zcr *c = time_queue->cr;
	*time_queue->ret = -1;
	time_queue = time_queue_pop(time_queue->left);
	zcr_resume(c);
}
