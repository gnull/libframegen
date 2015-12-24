#include <stdio.h>

extern char *whoami;

#define PRNT(level, str, ...) fprintf(stderr, "%6s: [%5s] " __FILE__ ": "  str "\n", whoami, level, ##__VA_ARGS__)

#define INFO(...) PRNT("INFO", ##__VA_ARGS__)
#define  ERR(...) PRNT("ERR", ##__VA_ARGS__)

