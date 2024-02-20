PREFIX=/usr/local
CC=gcc
CFLAGS=-W -Wall

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  OPUSFLAGS := -static $(shell pkg-config --cflags --libs --static libopusenc)
endif
ifeq ($(UNAME_S),Darwin)
  OPUSFLAGS := $(shell pkg-config --cflags --libs libopusenc)
endif

all: minirec miniogg

miniogg: miniogg.c
	$(CC) $(CFLAGS) miniogg.c -o miniogg -g $(OPUSFLAGS)

minirec: minirec.c miniaudio.h
	$(CC) $(CFLAGS) minirec.c -o minirec -g -lm

clean:
	rm -f minirec miniogg 
	rm -rf *.dSYM

install: miniogg minirec minigram minibash whisper
	install -v miniogg minirec minigram minibash whisper $(PREFIX)/bin/
