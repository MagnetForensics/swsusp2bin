.PHONY: build clean

PROG_NAME=swsusp2bin
OUT_DIR=out
CC=g++

build:
	mkdir -p ${OUT_DIR}
	${CC} -std=c++11 -o ${OUT_DIR}/${PROG_NAME} \
		lzo1x_decompress.cpp swsusp2bin.cpp

clean:
	rm -rf ${OUT_DIR}
