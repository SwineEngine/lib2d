#ifndef STRETCHY_BUFFER_H
#define STRETCHY_BUFFER_H

#include <assert.h>
#include <string.h> // for memmove

// stretchy buffer // init: NULL // free: sbfree() // push_back: sbpush() // size: sbcount() //
#define sbfree(a)         ((a) ? free(stb__sbraw(a)),0 : 0)
#define sbpush(a,v)       (stb__sbmaybegrow(a,1), (a)[stb__sbn(a)++] = (v))
#define sbcount(a)        ((a) ? stb__sbn(a) : 0)
#define sbadd(a,n)        (stb__sbmaybegrow(a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define sblast(a)         ((a)[stb__sbn(a)-1])
#define sbremove(a,i,n)   (memmove((a)+(i), (a)+(i)+(n), (stb__sbn(a)-((i)+(n)))*sizeof(*(a))), stb__sbn(a)-=(n))
#define sbconcat(a, b) (memcpy(sbadd((a), sbcount(b)), (b), sbcount(b)*sizeof(*(b))))
#define sbempty(a) ((a) ? stb__sbn(a)=0 : 0)

// foreach, with a pointer into the array
#define sbforeachp(p, a) \
    for (unsigned int stb__counter=0; stb__counter<sbcount((a)); stb__counter++) \
        for (p = &(a)[stb__counter]; /* inner loop is executed once, so we can initialize a variable */\
                !(stb__counter & (1<<31)) || ((stb__counter &= ~(1<<31))&&false); /* check and restore */ \
                stb__counter |= (1<<31) /* signal that we've executed */)
// foreach, copying the values of the array
#define sbforeachv(v, a) \
    for (unsigned int stb__counter=0; stb__counter<sbcount((a)); stb__counter++) \
        for (v = (a)[stb__counter]; /* inner loop is executed once, so we can initialize a variable */\
                !(stb__counter & (1<<31)) || ((stb__counter &= ~(1<<31))&&false); /* check and restore */ \
                stb__counter |= (1<<31) /* signal that we've executed */)

#include <stdlib.h>
#define stb__sbraw(a) ((int *) (a) - 2)
#define stb__sbm(a)   stb__sbraw(a)[0]
#define stb__sbn(a)   stb__sbraw(a)[1]

#define stb__sbneedgrow(a,n)  ((a)==0 || stb__sbn(a)+n >= stb__sbm(a))
#define stb__sbmaybegrow(a,n) (stb__sbneedgrow(a,(n)) ? stb__sbgrow(a,n) : (void)0)
#define stb__sbgrow(a,n)  stb__sbgrowf((void **) &(a), (n), sizeof(*(a)))

static void stb__sbgrowf(void **arr, int increment, int itemsize)
{
   int m = *arr ? 2*stb__sbm(*arr)+increment : increment+1;
   void *p = realloc(*arr ? stb__sbraw(*arr) : 0, itemsize * m + sizeof(int)*2);
   assert(p);
   if (p) {
      if (!*arr) ((int *) p)[1] = 0;
      *arr = (void *) ((int *) p + 2);
      stb__sbm(*arr) = m;
   }
}

#endif
