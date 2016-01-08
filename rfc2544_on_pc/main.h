#define ifeq(a, b) if (!strcmp(a, b))

#define ERR(str, ...) fprintf(stderr, "[ ERR] %s: %s: %d: "  str "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

extern FILE *conf_file;

int parse_argv(char **argv, header_cfg_t *hdr);
int parse_conf(rfc2544_settings_t *set);
