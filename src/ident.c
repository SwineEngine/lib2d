#include "lib2d.h"

#include "siphash.h"
#include "stretchy_buffer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define EXPORTED __attribute__((visibility("default")))


struct reverse_entry {
    l2d_ident ident;
    const char* str;
};

static struct reverse_entry* reverse = NULL;

EXPORTED
l2d_ident
l2d_ident_from_str(const char* str) {
    if (!str || strlen(str) == 0)
        return 0;
    if (strlen(str) >= 1024) {
        assert(false);
        return 0;
    }

    unsigned char key[16] = "l2d_ident_hash__";
    l2d_ident ident = siphash(key, (const unsigned char*)str, strlen(str));

    sbforeachp(struct reverse_entry*e, reverse) {
        if (e->ident == ident) {
            return ident;
        }
    }
    struct reverse_entry* e = sbadd(reverse, 1);
    e->ident = ident;
    char* str_copy = malloc(strlen(str)+1);
    strcpy(str_copy, str);
    e->str = str_copy;
    return ident;
}

EXPORTED
l2d_ident
l2d_ident_from_strn(const char* str, int len) {
    assert(len < 255);
    char s[256];
    strncpy(s, str, len);
    s[len] = '\0';
    return l2d_ident_from_str(s);
}

EXPORTED
const char*
l2d_ident_as_char(l2d_ident ident) {
    sbforeachp(struct reverse_entry*e, reverse) {
        if (e->ident == ident) {
            return e->str;
        }
    }
    return NULL;
}
