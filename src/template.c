#include "template.h"
#include <string.h>
#include <stdlib.h>

char*
replace_vars(struct template_var* vars, const char* source, const char* prefix) {
    // figure out the total size needed for the buffer
    size_t len = strlen(source) + strlen(prefix);
    for (size_t i=0; vars[i].key; i++) {
        len += strlen(vars[i].key) + strlen(vars[i].value);
    }

    char* result = malloc(len+1);
    strcpy(result, prefix);
    strcat(result, source);

    for (size_t i=0; vars[i].key; i++) {
        while (1) {
            char* found = strstr(result, vars[i].key);
            if (found) {
                size_t len_key = strlen(vars[i].key);
                size_t len_value = strlen(vars[i].value);
                memmove(found+len_value, found+len_key, strlen(found+len_key)+1);
                memcpy(found, vars[i].value, len_value);
            } else {
                break;
            }
        }
    }

    return result;
}

