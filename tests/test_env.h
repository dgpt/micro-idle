#ifndef TEST_ENV_H
#define TEST_ENV_H

#ifdef _WIN32
#include <stdlib.h>
static inline int test_set_env(const char *key, const char *value) {
    return _putenv_s(key, value ? value : "");
}
static inline int test_unset_env(const char *key) {
    return _putenv_s(key, "");
}
#else
#include <stdlib.h>
static inline int test_set_env(const char *key, const char *value) {
    return setenv(key, value ? value : "", 1);
}
static inline int test_unset_env(const char *key) {
    return unsetenv(key);
}
#endif

#endif
