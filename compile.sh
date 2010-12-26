#!/bin/bash

# -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -lgthread-2.0 -lglib-2.0

gcc -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -lgthread-2.0 -lglib-2.0 -lconfuse request.c utils.c check.c check_str.c valiant.c score.c
