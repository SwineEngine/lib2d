#ifndef __LIB2D_TEMPLATE__
#define __LIB2D_TEMPLATE__

struct template_var {
    const char* key;
    const char* value;
};

char*
replace_vars(struct template_var* vars, const char* source, const char* prefix);

#endif
