#!/bin/bash

gcc -o valiant \
	error.c buf.c request.c utils.c stats.c conf.c score.c map.c map_bdb.c check.c \
	rbl.c check_dnsbl.c check_rhsbl.c check_str.c check_pcre.c stage.c \
	thread_pool.c slist.c context.c worker.c main.c \
	-lrt -lpthread -lconfuse -lspf2 -lpcre -ldb
