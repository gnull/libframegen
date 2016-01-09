#include <librfc2544/rfc2544.h>

#define ERR(str, ...) fprintf(stderr, "[ ERR] %s: %s: %d: "  str "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

int parse_argv(int argc, char **argv, rfc2544_settings_t *hdr);
