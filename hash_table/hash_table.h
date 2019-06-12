#ifndef __HASH_TABLE_H__
#define __HASH_TABLE_H__

#include <stdint.h>
#include <stdio.h>

#define HASH_BUCKET_MAX (1024)
#define HASH_BUCKET_CAPACITY_MAX (256)
#define HASHTABLE_DEBUG 0
#define TRUE  1
#define FALSE 0

#if HASHTABLE_DEBUG == 1
#define DEBUG(format, ...) printf("[%s] [%d] : "format"\n", __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

struct hash_bucket {
    int                capacity;  /* 桶的容量 */
    void               *hkey;     /* hashtable的key */
    void               *hdata;    /* hashtable的data */
    struct hash_bucket *prev;
    struct hash_bucket *next;
    struct hash_bucket *tail;
};

struct hash {
    uint32_t            (*hash_key)(void *);
    int                 (*hash_cmp)(const void *, const void*, int len);
    struct hash_bucket* bucket[HASH_BUCKET_MAX];
};

uint32_t string_hash_key(void* str);
int string_hash_cmp(const void* src, const void* dst, int len);

void hash_create();
void *hash_lookup(void *key);
int  hash_set(void *key, void* data);
int  hash_delete(void *key);
void hash_destroy();
void hash_iter_print();

#endif // __HASH_TABLE_H__
