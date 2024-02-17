CC=gcc
OPUSFLAGS=$(shell pkg-config --cflags --libs libopusenc)

all: minirec miniogg

miniogg: miniogg.c
	$(CC) -W -Wall miniogg.c -o miniogg -g $(OPUSFLAGS)

minirec: minirec.c miniaudio.h
	$(CC) -W -Wall minirec.c -o miniaudio -g

clean:
	rm -f minirec miniogg

