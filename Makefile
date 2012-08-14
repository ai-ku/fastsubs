CC=gcc
CFLAGS=-O3 -D_XOPEN_SOURCE -Wall -std=c99 -I. `pkg-config --cflags glib-2.0`
LIBS=`pkg-config --libs glib-2.0` -lm -lz

all: fastsubs wordsub subs

fastsubs: fastsubs-main.o fastsubs.o lm.o ngram.o minialloc.o sentence.o lmheap.o heap.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

fastsubs-main.o: fastsubs-main.c fastsubs.h
	$(CC) -c $(CFLAGS) $< -o $@

fastsubs-test: fastsubs-test.o fastsubs.o lm.o ngram.o minialloc.o sentence.o lmheap.o heap.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

fastsubs-test.o: fastsubs-test.c fastsubs.h
	$(CC) -c $(CFLAGS) $< -o $@

fastsubs.o: fastsubs.c fastsubs.h lm.h lmheap.h sentence.h ngram.h heap.h
	$(CC) -c $(CFLAGS) $< -o $@

lmheap-test: lmheap-test.o lmheap.o lm.o ngram.o minialloc.o heap.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

lmheap-test.o: lmheap-test.c lmheap.h lm.h
	$(CC) -c $(CFLAGS) $< -o $@

lmheap.o: lmheap.c lmheap.h lm.h ngram.h token.h heap.h
	$(CC) -c $(CFLAGS) $< -o $@

sentence-test: sentence-test.o sentence.o lm.o ngram.o minialloc.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

sentence-test.o: sentence-test.c sentence.h
	$(CC) -c $(CFLAGS) $< -o $@

sentence.o: sentence.c sentence.h token.h lm.h
	$(CC) -c $(CFLAGS) $< -o $@

lm-test: lm-test.o lm.o ngram.o minialloc.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

lm-test.o: lm-test.c lm.h
	$(CC) -c $(CFLAGS) $< -o $@

lm.o: lm.c lm.h ngram.h
	$(CC) -c $(CFLAGS) $< -o $@

ngram.o: ngram.c ngram.h token.h foreach.h
	$(CC) -c $(CFLAGS) $< -o $@

heap.o: heap.c heap.h token.h
	$(CC) -c $(CFLAGS) $< -o $@

minialloc.o: minialloc.c minialloc.h
	$(CC) -c $(CFLAGS) $< -o $@

wordsub: wordsub.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

wordsub.o: wordsub.c foreach.h
	$(CC) -c $(CFLAGS) $< -o $@

subs: subs.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

subs.o: subs.c foreach.h procinfo.h ghashx.h
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-rm -f *.o fastsubs fastsubs-test lmheap-test sentence-test lm-test wordsub subs
