#!/bin/bash

gcc -o valiant \
	request.c utils.c score.c map.c map_bdb.c set.c check.c rbl.c \
	check_dnsbl.c check_rhsbl.c check_str.c check_pcre.c thread_pool.c \
	slist.c main.c \
	-lrt -lpthread -lconfuse -lspf2 -lpcre -ldb
