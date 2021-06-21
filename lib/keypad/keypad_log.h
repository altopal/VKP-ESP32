#pragma once

#define DEBUG 0
#define INFO 1
#define WARN 1
#define ERROR 1

#define debug(fmt, ...) \
        do { if (DEBUG) fprintf(stdout, "D : " fmt, __VA_ARGS__); } while (0)
#define info(fmt, ...) \
        do { if (INFO) fprintf(stdout, "I : " fmt, __VA_ARGS__); } while (0)
#define warn(fmt, ...) \
        do { if (WARN) fprintf(stdout, "W : " fmt, __VA_ARGS__); } while (0)
#define error(fmt, ...) \
        do { if (ERROR) fprintf(stdout, "E : " fmt, __VA_ARGS__); } while (0)
