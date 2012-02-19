/*
	libxbee - a C library to aid the use of Digi's XBee wireless modules
	          running in API mode (AP=2).

	Copyright (C) 2009	Attie Grande (attie@attie.co.uk)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.	If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>

#include "internal.h"
#include "ll.h"

/* DO NOT RE-ORDER! */
struct ll_info {
	struct ll_info *next;
	struct ll_info *prev;
	int is_head;
	struct ll_head *head;
	void *item;
};

xbee_err __ll_get_item(void *list, void *item, struct ll_info **retItem, int needMutex);

/* this file is scary, sorry it isn't commented... i nearly broke myself writing it
   maybe oneday soon i'll be brave and put some commends down */

xbee_err ll_init(struct ll_head *list) {
	if (!list) return XBEE_EMISSINGPARAM;
	list->is_head = 1;
	list->head = NULL;
	list->tail = NULL;
	list->self = list;
	if (xsys_mutex_init(&list->mutex)) return XBEE_EMUTEX;
	return 0;
}

void ll_destroy(struct ll_head *list, void (*freeCallback)(void *)) {
	void *p;
	while ((ll_ext_tail(list, &p)) == XBEE_ENONE && p) {
		if (freeCallback) freeCallback(p);
	}
	xsys_mutex_destroy(&list->mutex);
}

/* ################################################################ */

void *ll_alloc(void) {
	struct ll_head *h;
	if ((h = calloc(1, sizeof(struct ll_head))) == NULL) return NULL;
	if (ll_init(h) != 0) {
		free(h);
		h = NULL;
	}
	return h;
}

void ll_free(void *list, void (*freeCallback)(void *)) {
	ll_destroy(list, freeCallback);
	free(list);
}

/* ################################################################ */

xbee_err ll_lock(void *list) {
	struct ll_head *h;
	struct ll_info *i;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	xsys_mutex_lock(&h->mutex);
	return XBEE_ENONE;
}

xbee_err ll_unlock(void *list) {
	struct ll_head *h;
	struct ll_info *i;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	xsys_mutex_unlock(&h->mutex);
	return XBEE_ENONE;
}

/* ################################################################ */

xbee_err _ll_add_head(void *list, void *item, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *p;
	xbee_err ret;
	ret = XBEE_ENONE;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	p = h->head;
	if (!(h->head = calloc(1, sizeof(struct ll_info)))) {
		h->head = p;
		ret = XBEE_ENOMEM;
		goto out;
	}
	h->head->head = h;
	h->head->prev = NULL;
	if (p) {
		h->head->next = p;
		p->prev = h->head;
	} else {
		h->head->next = NULL;
		h->tail = h->head;
	}
	h->head->item = item;
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

xbee_err _ll_add_tail(void *list, void *item, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *p;
	xbee_err ret;
	ret = XBEE_ENONE;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	p = h->tail;
	if (!(h->tail = calloc(1, sizeof(struct ll_info)))) {
		h->tail = p;
		ret = XBEE_ENOMEM;
		goto out;
	}
	h->tail->head = h;
	h->tail->next = NULL;
	if (p) {
		h->tail->prev = p;
		p->next = h->tail;
	} else {
		h->tail->prev = NULL;
		h->head = h->tail;
	}
	h->tail->item = item;
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

/* NULL ref will add to tail */
xbee_err _ll_add_after(void *list, void *ref, void *item, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *t;
	xbee_err ret;
	ret = XBEE_ENONE;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (!ref) return ll_add_tail(h, item);
	if (needMutex) xsys_mutex_lock(&h->mutex);
	i = h->head;
	while (i) {
		if (i->item == ref) break;
		i = i->next;
	}
	if (!i) {
		ret = XBEE_ENOTEXISTS;
		goto out;
	}
	if (!(t = calloc(1, sizeof(struct ll_info)))) {
		ret = XBEE_ENOMEM;
		goto out;
	}
	t->head = i->head;
	if (!i->next) {
		h->tail = t;
		t->next = NULL;
	} else {
		i->next->prev = t;
		t->next = i->next;
	}
	i->next = t;
	t->prev = i;
	t->item = item;
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

/* NULL ref will add to head */
xbee_err _ll_add_before(void *list, void *ref, void *item, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *t;
	xbee_err ret;
	ret = XBEE_ENONE;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (!ref) return ll_add_tail(h, item);
	if (needMutex) xsys_mutex_lock(&h->mutex);
	i = h->head;
	while (i) {
		if (i->item == ref) break;
		i = i->next;
	}
	if (!i) {
		ret = XBEE_ENOTEXISTS;
		goto out;
	}
	if (!(t = calloc(1, sizeof(struct ll_info)))) {
		ret = XBEE_ENOMEM;
		goto out;
	}
	t->head = i->head;
	if (!i->prev) {
		h->head = t;
		t->prev = NULL;
	} else {
		i->prev->next = t;
		t->prev = i->prev;
	}
	i->prev = t;
	t->next = i;
	t->item = item;
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

/* ################################################################ */

xbee_err _ll_get_head(void *list, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i;
	xbee_err ret;
	if (!list || !retItem) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	if (h->head) {
		*retItem = h->head->item;
		ret = XBEE_ENONE;
	} else {
		ret = XBEE_ERANGE;
	}
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

xbee_err _ll_get_tail(void *list, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i;
	xbee_err ret;
	if (!list || !retItem) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	if (h->tail) {
		*retItem = h->tail->item;
		ret = XBEE_ENONE;
	} else {
		ret = XBEE_ERANGE;
	}
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

/* returns struct ll_info* or NULL - don't touch the pointer if you don't know what you're doing ;) */
xbee_err __ll_get_item(void *list, void *item, struct ll_info **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	i = h->head;
	while (i) {
		if (i->item == item) break;
		i = i->next;
	}
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	if (retItem) *retItem = (void*)i;
	if (!i) return XBEE_ENOTEXISTS;
	return XBEE_ENONE;
}
xbee_err _ll_get_item(void *list, void *item, int needMutex) {
	return __ll_get_item(list, item, NULL, needMutex);
}

xbee_err _ll_get_next(void *list, void *ref, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i;
	void *ret;
	if (!list || !retItem) return XBEE_EMISSINGPARAM;
	ret = NULL;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (!ref) return _ll_get_head(h, retItem, needMutex);
	if (needMutex) xsys_mutex_lock(&h->mutex);
	i = h->head;
	if (__ll_get_item(h, ref, &i, 0) != XBEE_ENONE) goto out;
	if (!i) {
		ret = NULL;
		goto out;
	}
	i = i->next;
	if (i) {
		ret = i->item;
	} else {
		ret = NULL;
	}
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	*retItem = ret;
	if (!ret) return XBEE_ERANGE;
	return XBEE_ENONE;
}

xbee_err _ll_get_prev(void *list, void *ref, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i;
	void *ret;
	if (!list | !retItem) return XBEE_EMISSINGPARAM;
	if (!ref) return _ll_get_tail(list, retItem, needMutex);
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	i = h->head;
	if (__ll_get_item(h, ref, &i, 0) != XBEE_ENONE) goto out;
	if (!i) {
		ret = NULL;
		goto out;
	}
	i = i->prev;
	if (i) {
		ret = i->item;
	} else {
		ret = NULL;
	}
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	*retItem = ret;
	if (!ret) return XBEE_ERANGE;
	return XBEE_ENONE;
}

xbee_err _ll_get_index(void *list, unsigned int index, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i;
	int o;
	if (!list || !retItem) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	i = h->head;
	for (o = 0; o < index; o++) {
		i = i->next;
		if (!i) break;
	}
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	if (!i) {
		*retItem = NULL;
		return XBEE_ERANGE;
	}
	*retItem = i->item;
	return XBEE_ENONE;
}

/* ################################################################ */

xbee_err _ll_ext_head(void *list, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *p;
	void *ret;
	if (!list || !retItem) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	p = h->head;
	if (!p) {
		ret = NULL;
		goto out;
	}
	ret = p->item;
	h->head = p->next;
	if (h->head) h->head->prev = NULL;
	if (h->tail == p) h->tail = NULL;
	free(p);
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	*retItem = ret;
	if (!ret) return XBEE_ERANGE;
	return XBEE_ENONE;
}

xbee_err _ll_ext_tail(void *list, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *p;
	void *ret;
	if (!list || !retItem) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	p = h->tail;
	if (!p) {
		ret = NULL;
		goto out;
	}
	ret = p->item;
	h->tail = p->prev;
	if (h->tail) h->tail->next = NULL;
	if (h->head == p) h->head = NULL;
	free(p);
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	*retItem = ret;
	if (!ret) return XBEE_ERANGE;
	return XBEE_ENONE;
}

xbee_err _ll_ext_item(void *list, void *item, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *p;
	xbee_err ret;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	p = h->head;
	ret = XBEE_ENONE;
	while (p) {
		if (p->is_head) {
			ret = XBEE_ELINKEDLIST;
			break;
		}
		if (p->item == item) {
			if (p->next) {
				p->next->prev = p->prev;
			} else {
				h->tail = p->prev;
			}
			if (p->prev) {
				p->prev->next = p->next;
			} else {
				h->head = p->next;
			}
			free(p);
			break;
		}
		p = p->next;
	}
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	if (!p) return XBEE_ENOTEXISTS;
	return ret;
}

xbee_err _ll_ext_index(void *list, unsigned int index, void **retItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i;
	int o;
	xbee_err ret;
	if (!list || !retItem) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	i = h->head;
	for (o = 0; o < index; o++) {
		i = i->next;
		if (!i) break;
	}
	if (!i) {
		*retItem = NULL;
		ret = XBEE_ERANGE;
		goto out;
	}
	*retItem = i->item;
	if (i->next) {
		i->next->prev = i->prev;
	} else {
		h->tail = i->prev;
	}
	if (i->prev) {
		i->prev->next = i->next;
	} else {
		h->head = i->next;
	}
	free(i);
	ret = XBEE_ENONE;
out:
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

/* ################################################################ */

xbee_err _ll_modify_item(void *list, void *oldItem, void *newItem, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *p;
	xbee_err ret;
	if (!list) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	if ((ret = __ll_get_item(h, oldItem, &p, 0)) == XBEE_ENONE) {
		p->item = newItem;
	}
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	return ret;
}

/* ################################################################ */

xbee_err _ll_count_items(void *list, unsigned int *retCount, int needMutex) {
	struct ll_head *h;
	struct ll_info *i, *p;
	int count;
	if (!list || !retCount) return XBEE_EMISSINGPARAM;
	i = list;
	h = i->head;
	if (!(h && h->is_head && h->self == h)) return XBEE_EINVAL;
	if (needMutex) xsys_mutex_lock(&h->mutex);
	for (p = h->head, count = 0; p; p = p->next, count++);
	if (needMutex) xsys_mutex_unlock(&h->mutex);
	*retCount = count;
	return XBEE_ENONE;
}

/* ################################################################ */

xbee_err ll_combine(void *head, void *tail) {
	struct ll_head *hH, *hT;
	struct ll_info *iH, *iT;
	void *v;
	xbee_err ret;
	ret = XBEE_ENONE;
	if (!head || !tail) return XBEE_EMISSINGPARAM;
	if (head == tail) return XBEE_EINVAL;
	iH = head;
	hH = iH->head;
	if (!(hH && hH->is_head && hH->self == hH)) return XBEE_EINVAL;
	xsys_mutex_lock(&hH->mutex);
	iT = tail;
	hT = iT->head;
	if (!(hT && hT->is_head && hT->self == hT)) { ret = XBEE_EINVAL; goto out; }
	xsys_mutex_lock(&hT->mutex);
	while ((ret = _ll_ext_head(tail, &v, 0)) == XBEE_ENONE && v) {
		_ll_add_tail(head, v, 0);
	}
	xsys_mutex_unlock(&hH->mutex);
out:
	xsys_mutex_unlock(&hT->mutex);
	return XBEE_ENONE;
}
