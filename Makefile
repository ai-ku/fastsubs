CC=gcc
CFLAGS=-O3 -DNDEBUG -D_POSIX_C_SOURCE=200809L -Wall -std=c99 -pedantic
#CFLAGS=-O3 -DNDEBUG -D_GNU_SOURCE -Wall -std=c99 -pedantic
#CFLAGS=-O3 -save-temps -D_GNU_SOURCE -Wall -std=c99 -pedantic
LIBS=-lm
TARGETS=fastsubs fastsubs-omp fastsubs-test lmheap-test lm-test sentence-test wordsub

all: ${TARGETS}

fastsubs-omp: fastsubs-omp.o fastsubs.o lm.o ngram.o sentence.o heap.o dlib.o
	$(CC) -fopenmp $(CFLAGS) $^ $(LIBS) -o $@

fastsubs-omp.o: fastsubs-omp.c fastsubs.h
	$(CC) -c -fopenmp $(CFLAGS) $< -o $@

fastsubs: fastsubs-main.o fastsubs.o lm.o ngram.o sentence.o heap.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

fastsubs-main.o: fastsubs-main.c fastsubs.h
	$(CC) -c $(CFLAGS) $< -o $@

fastsubs-test: fastsubs-test.o fastsubs.o lm.o ngram.o sentence.o heap.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

fastsubs-test.o: fastsubs-test.c fastsubs.h
	$(CC) -c $(CFLAGS) $< -o $@

fastsubs.o: fastsubs.c fastsubs.h lm.h sentence.h ngram.h heap.h
	$(CC) -c $(CFLAGS) $< -o $@

lmheap-test: lmheap-test.o lm.o ngram.o heap.o dlib.o sentence.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

lmheap-test.o: lmheap-test.c lm.h
	$(CC) -c $(CFLAGS) $< -o $@

lm-test: lm-test.o lm.o ngram.o dlib.o heap.o sentence.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

lm-test.o: lm-test.c lm.h
	$(CC) -c $(CFLAGS) $< -o $@

lm.o: lm.c lm.h ngram.h
	$(CC) -c $(CFLAGS) $< -o $@

sentence-test: sentence-test.o sentence.o lm.o ngram.o dlib.o heap.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

sentence-test.o: sentence-test.c sentence.h
	$(CC) -c $(CFLAGS) $< -o $@

sentence.o: sentence.c sentence.h token.h lm.h dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

ngram.o: ngram.c ngram.h token.h dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

heap.o: heap.c heap.h token.h
	$(CC) -c $(CFLAGS) $< -o $@

wordsub: wordsub.o dlib.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

wordsub.o: wordsub.c dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

dlib.o: dlib.c dlib.h
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-rm -f *.o *.i *.s *~ ${TARGETS}
