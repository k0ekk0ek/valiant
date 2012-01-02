#!/bin/bash

gcc -o valiant \
	error.c buf.c request.c result.c utils.c state.c  \
	dict.c rbl.c dict_dnsbl.c dict_spf.c dict_str.c dict_hash.c dict_pcre.c \
	thread_pool.c slist.c main.c \
	-lrt -lpthread -lconfuse -lspf2 -lpcre -ldb -lm
