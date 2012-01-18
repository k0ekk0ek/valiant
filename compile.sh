#!/bin/bash

gcc -g -O0 -o valiant \
	error.c buf.c request.c result.c utils.c state.c conf.c \
	dict.c rbl.c dict_dnsbl.c dict_rhsbl.c dict_spf.c dict_str.c dict_hash.c dict_pcre.c \
	worker.c watchdog.c thread_pool.c slist.c context.c stats.c main.c \
	-lrt -lpthread -lconfuse -lspf2 -lpcre -ldb -lm
