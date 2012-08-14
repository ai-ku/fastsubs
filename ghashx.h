/* $Id: ghashx.h,v 1.3 2009/05/05 09:47:19 dyuret Exp $ */

#ifndef __GHASHX_H__
#define __GHASHX_H__

#include <glib.h>

/* shorthands */
typedef GHashTable *Hash;
#define hset(h,k,v) g_hash_table_insert((h),(k),(gpointer)(v))
#define hget(h,k) g_hash_table_lookup((h),(k))
#define hgot(h,k) g_hash_table_lookup_extended((h),(k),NULL,NULL)
#define hdel(h,k) g_hash_table_remove((h),(k))
#define hlen(h) g_hash_table_size(h)
#define hmap(h,f,d) g_hash_table_foreach((h),(GHFunc)(f),(d))
#define free_hash(h) g_hash_table_destroy(h)
#define hfree(h) g_hash_table_destroy(h)

/* string-to-int hash functions */
#define I2P(v) GINT_TO_POINTER(v)
#define s2i_new() g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL)
#define s2i_get(h,k) GPOINTER_TO_INT(hget(h,k))
#define s2i_set(h,k,v) (hgot(h,k) ? hset(h,k,I2P(v)) : hset(h,g_strdup(k),I2P(v)))
#define s2i_add(h,k,v) (hgot(h,k) ? hset(h,k,I2P(s2i_get(h,k)+(v))) : hset(h,g_strdup(k),I2P(v)))
#define s2i_inc(h,k) s2i_add(h,k,1)
#define s2i_free(h) hfree(h)

#ifdef __GHASHX_DBG__
/** debugging stuff */

typedef struct _GHashNode      GHashNode;

struct _GHashNode
{
  gpointer   key;
  gpointer   value;
  GHashNode *next;
};

struct _GHashTable
{
  gint             size;
  gint             nnodes;
  GHashNode      **nodes;
  GHashFunc        hash_func;
  GEqualFunc       key_equal_func;
  GDestroyNotify   key_destroy_func;
  GDestroyNotify   value_destroy_func;
};

static void g_hash_table_analyze(Hash h) {
  struct _GHashTable *ht = h;
  int sum=0, zero=0, max=0;
  for (int i = 0; i < ht->size; i++) {
    GHashNode **node = &ht->nodes[i];
    int cnt = 0;
    while (*node) {
      cnt++; node = &(*node)->next;
    }
    sum += cnt;
    if (cnt > max) max = cnt;
    if (cnt == 0) zero++;
  }
  g_message("Hash table size=%d nelem=%d full=%d avg=%g max=%d",
	    ht->size, sum, ht->size - zero, (float)sum/ht->size, max);
}

#endif
#endif
