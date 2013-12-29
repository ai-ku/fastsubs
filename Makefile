CC=gcc
CFLAGS=-O3 -save-temps -D_GNU_SOURCE -Wall -std=c99 -pedantic
LIBS=-lm -lz

all: fastsubs wordsub subs fastsubs-test lmheap-test lm-test sentence-test

fastsubs: fastsubs-main.o fastsubs.o lm.o ngram.o sentence.o lmheap.o heap.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

fastsubs-main.o: fastsubs-main.c fastsubs.h
	$(CC) -c $(CFLAGS) $< -o $@

fastsubs-test: fastsubs-test.o fastsubs.o lm.o ngram.o sentence.o lmheap.o heap.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

fastsubs-test.o: fastsubs-test.c fastsubs.h
	$(CC) -c $(CFLAGS) $< -o $@

fastsubs.o: fastsubs.c fastsubs.h lm.h lmheap.h sentence.h ngram.h heap.h
	$(CC) -c $(CFLAGS) $< -o $@

lmheap-test: lmheap-test.o lmheap.o lm.o ngram.o heap.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

lmheap-test.o: lmheap-test.c lmheap.h lm.h
	$(CC) -c $(CFLAGS) $< -o $@

lmheap.o: lmheap.c lmheap.h lm.h ngram.h token.h heap.h
	$(CC) -c $(CFLAGS) $< -o $@

sentence-test: sentence-test.o sentence.o lm.o ngram.o dlib.o 
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

sentence-test.o: sentence-test.c sentence.h
	$(CC) -c $(CFLAGS) $< -o $@

sentence.o: sentence.c sentence.h token.h lm.h dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

lm-test: lm-test.o lm.o ngram.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

lm-test.o: lm-test.c lm.h
	$(CC) -c $(CFLAGS) $< -o $@

lm.o: lm.c lm.h ngram.h
	$(CC) -c $(CFLAGS) $< -o $@

ngram.o: ngram.c ngram.h token.h dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

heap.o: heap.c heap.h token.h
	$(CC) -c $(CFLAGS) $< -o $@

wordsub: wordsub.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

wordsub.o: wordsub.c dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

subs: subs.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

subs.o: subs.c dlib.h procinfo.h ghashx.h
	$(CC) -c $(CFLAGS) $< -o $@

dlib.o: dlib.c dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-rm -f *.o *.i *.s *~ fastsubs fastsubs-test lmheap-test sentence-test lm-test wordsub subs
