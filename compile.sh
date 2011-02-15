#!/bin/bash

# -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -lgthread-2.0 -lglib-2.0

gcc -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -lgthread-2.0 -lglib-2.0 -lconfuse -lspf2 request.c utils.c check.c check_str.c check_pcre.c check_dnsbl.c valiant.c score.c thread_pool.c
