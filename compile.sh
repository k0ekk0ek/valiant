#!/bin/bash

gcc -o valiant -lrt -lpthread -lconfuse -lspf2 -lpcre \
	request.c utils.c score.c check.c rbl.c check_dnsbl.c check_rhsbl.c check_str.c check_pcre.c \
	thread_pool.c slist.c main.c
