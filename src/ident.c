#include "lib2d.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define L_SIZE 1024

struct link {
    char* data;
    struct link* next;
};

static struct l2d_ident_bank {
    struct link* first;
    struct link* last;
} bank = {.first=NULL, .last=NULL};

static
struct link*
newLink() {
    struct link* link = malloc(sizeof(struct link));
    link->data = malloc(sizeof(char)*L_SIZE);
    memset(link->data, '\0', sizeof(char)*L_SIZE);
    link->next = NULL;
    if (bank.last) {
        bank.last->next = link;
    } else {
        bank.first = link;
    }
    bank.last = link;
    return link;
}

static
char*
findSpaceInLink(size_t length) {
    length += 1; // for null terminator
    // TODO this should be an 'input verification' type of assert, (that also
    // runs in release builds.)
    assert(length < L_SIZE);
    if (bank.last == NULL) {
        return newLink()->data;
    }
    char* lastPossible = bank.last->data + L_SIZE - length - 1;
    for (char* c=bank.last->data; c<lastPossible; c++) {
        if (c[0] == '\0' && c[1] == '\0') {
            return &c[1];
        }
    }
    return newLink()->data;
}

l2d_ident
l2d_ident_from_str(const char* str) {
    if (strlen(str) == 0)
        return 0;
    if (strlen(str) >= L_SIZE) {
        assert(0);
        return 0;
    }
    // First, check if it already is a pointer into the l2d_identifier string data.
    // This will happen if we're passed back a const char* that came from
    // l2d_ident_as_char.
    for (struct link* link = bank.first; link != NULL; link = link->next) {
        if ((str >= link->data && str < link->data + L_SIZE)) {
            // It's in the range, but make sure that it's the *start* of an
            // l2d_identifier, not halfway through or something.
            if (str == link->data || *(str-1) == '\0') {
                return (l2d_ident)str;
            }
        }
    }

    // Now, search the contents:
    for (struct link* link = bank.first; link != NULL; link = link->next) {
        for (const char* c = link->data;
                c[0] != '\0' && c < link->data+L_SIZE;
                c += strlen(c)+1) {
            if (strcmp(c, str) == 0) {
                return (l2d_ident)c;
            }
        }
    }

    // We didn't find it, so append it.

    char* c = findSpaceInLink(strlen(str));
    strcpy(c, str);
    return (l2d_ident)c;
}

l2d_ident
l2d_ident_from_strn(const char* str, int len) {
    assert(len < 255);
    char s[256];
    strncpy(s, str, len);
    s[len] = '\0';
    return l2d_ident_from_str(s);
}

const char*
l2d_ident_as_char(l2d_ident i) {
    return (const char*)i;
}
