#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define try if (!
#define tryend ) return 1

bool run_cmd(const char *cmd) {
    printf("[CMD]: %s\n", cmd);
    try system(cmd) tryend;
}

#define CFLAGS ""

int main(int argc, char **argv) {

    // self rebuild
    try run_cmd("del make.old.exe") tryend;
    try run_cmd("ren make.exe make.old.exe") tryend;
    try run_cmd("gcc make.c -o make"CFLAGS) tryend;

    if (argc < 1) {
        fprintf(stderr, "[ERROR]: argc < 1, something is very wrong");
        exit(1);
    }

    if (argc >= 2 && strcmp(argv[1], "run") == 0) {
        try run_cmd("gcc main.c -o main"CFLAGS" && main") tryend;
    } else if (argc >= 2 && strcmp(argv[1], "reload_libdyn") == 0) {
        try run_cmd("pwsh -c copy ../libdyn/libdyn.h .") tryend;
    } else {
        try run_cmd("gcc main.c -o main"CFLAGS) tryend;
        printf(
            "\n"
            "Usage:\n"
            "- ./make run -> runs main alongside rebuilding.\n"
            "- ./make reload_libdyn -> reloads libdyn.h.\n"
            "\n"
        );
    }

    return 0;
}
