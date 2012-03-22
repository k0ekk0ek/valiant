#!/bin/sh

gcc -g -O0 -o config -Iifupdown \
	../src/vt_error.c ../src/vt_buf.c ../src/vt_lexer.c ../src/vt_config.c config.c
