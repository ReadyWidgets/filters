#!/usr/bin/env bash
cd "$( dirname "${BASH_SOURCE[0]}" )" || exit 1

file="c-filters.c"

gcc -std=c11 -Werror-implicit-function-declaration -shared -fPIC "$(pkg-config --cflags --libs gtk+-3.0 glib-2.0 gio-2.0 gio-unix-2.0 luajit cairo)" -lm "$file" -o "${file%.c}.so"
