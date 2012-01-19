#include <stdio.h>
#include <glib.h>

#define BLOCKSIZE (1<<20)
static gpointer data;
static gpointer free;
static gsize remaining;

static void newdata(gpointer olddata) {
    data = g_malloc(BLOCKSIZE);
    *((gpointer *)data) = olddata;
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

void minialloc_free_all() {
  int n = 0;
  while (data != NULL) {
    gpointer p = *((gpointer*)data);
    g_free(data);
    data = p;
    n++;
  }
  fprintf(stderr, "Freed %d blocks\n", n);
}
