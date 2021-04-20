Build the static library by running
    make all
Locate the directory of the resulting library file `libmy_pthread.a`. This will
be the directory in which the source files are located, unless something goes
awry.
Link executables with
    -lmy_pthread -lrt -lm
For example, to build the provided TEST.c script,
    make all
    gcc -std=c99 -o TEST TEST.c -L. -lmy_pthread -lm -lrt
