PREFIX=/usr/local
CC=gcc

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    OPUSFLAGS := $(shell pkg-config --cflags --libs --static libopusenc)
endif
ifeq ($(UNAME_S),Darwin)
    OPUSFLAGS := $(shell pkg-config --cflags --libs libopusenc)
endif

all: minirec miniogg

miniogg: miniogg.c
	$(CC) -W -Wall miniogg.c -o miniogg -g $(OPUSFLAGS)

minirec: minirec.c miniaudio.h
	$(CC) -W -Wall minirec.c -o minirec -g -lm

clean:
	rm -f minirec miniogg 
	rm -rf *.dSYM

install: miniogg minirec minigram
	install -v miniogg minirec minigram /usr/local/bin/
