#include "runtime.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *scenario = "baseline";
    int code;

    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            ldtl_runtime_print_help(stdout);
            return 0;
        }
        scenario = argv[1];
    }

    code = ldtl_runtime_run_scenario(scenario, stdout);
    if (code != 0) {
        fprintf(stderr, "unknown or failed scenario: %s\n", scenario);
    }
    return code == 0 ? 0 : 1;
}
