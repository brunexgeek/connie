#!/bin/bash

gcc -g -fsanitize=address -Wall -Wextra -pedantic -std=c11 \
    -Werror=incompatible-pointer-types \
    -Werror=implicit-function-declaration \
    -Werror=return-type \
    -Werror=int-conversion \
    -Wunused-function \
    -Wunused-variable \
    -Wmissing-declarations \
    -Wmissing-prototypes \
    connie.c example1.c -o /tmp/connie
