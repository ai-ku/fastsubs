#include <glib.h>

#define BLOCKSIZE (1<<20)
static gpointer data;
static gpointer free;
static gsize remaining;

static void newdata(gpointer olddata) {
    data = g_slice_alloc(BLOCKSIZE);
    *((gpointer *)data) = NULL;
    free = data + sizeof(gpointer);
    remaining = BLOCKSIZE - sizeof(gpointer);
}

gpointer minialloc(gsize size) {
  g_assert(size <= BLOCKSIZE);
  if ((data == NULL) || (size > remaining)) {
    newdata(data);
  }
  gpointer rval = free;
  free += size;
  remaining -= size;
  return rval;
}

void free_all() {
  g_slice_free_chain_with_offset(BLOCKSIZE, data, 0);
}
