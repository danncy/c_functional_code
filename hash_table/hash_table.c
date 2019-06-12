#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"
#include <pthread.h>

static __thread struct hash *g_htable = NULL;

#define bucket_kv_free(bucket) \
    free(bucket->hkey);        \
    free(bucket->hdata);       \
    bucket->hkey = NULL;       \
    bucket->hdata = NULL;

#define  bucket_free(bucket) \
    bucket_kv_free(bucket)   \
    free(bucket);            \
    bucket = NULL;

uint32_t string_hash_key(void* str) {
    char *tmp = (char *)str;
    uint32_t key = 0;
    while(*tmp)
        key = (key * 33) ^ (uint32_t)(tmp++);

    DEBUG("string key : %u, index : %d", key, key%HASH_BUCKET_MAX);

    return key%HASH_BUCKET_MAX;
}

int string_hash_cmp(const void* src, const void* dst, int len) {
    if (!src || !dst) {
        DEBUG("src addr: %p, dst addr: %p", src, dst);
        return -1;
    }
    return strncmp((char *)src, (char *)dst, len);
}

void hash_create() {
    if (g_htable) {
        DEBUG("the default hashtable is already created");
        return;
    }

    g_htable = (struct hash *)malloc(sizeof(struct hash));
    if (!g_htable) {
        DEBUG("memory alloc failed.");
        return;
    }

    memset(g_htable, 0, sizeof(struct hash));

    g_htable->hash_key = string_hash_key;
    g_htable->hash_cmp = string_hash_cmp;

    return;
}

static void bucket_delete(struct hash_bucket** ptr) {
    struct hash_bucket *bucket = *ptr;
    struct hash_bucket *tmp;

    while(bucket) {
        tmp = bucket;
        bucket = bucket->next;
        bucket_free(tmp);
    }
}

void hash_destroy() {
    if (g_htable) {
        for(int i=0; i<HASH_BUCKET_MAX; i++) {
            if (g_htable->bucket[i]) {
                bucket_delete(&g_htable->bucket[i]);
            }
        }

        free(g_htable);
        g_htable = NULL;
    }
    return;
}

#define  lru_bucket_move(bucket,  head)   \
    bucket->next = head;                  \
    bucket->prev = NULL;                  \
    bucket->capacity = head->capacity;    \
    head->capacity = 0;                   \
                                          \
    head->prev = bucket;                  \
    head->tail = NULL;

void *hash_lookup(void *key) {
    if (!key || !g_htable) {
        DEBUG("input para is NULL\n");
        return NULL;
    }

    uint32_t index = g_htable->hash_key(key);
    struct hash_bucket* head = g_htable->bucket[index];
    struct hash_bucket* bucket = head;

    while(bucket) {
        if (0 == g_htable->hash_cmp(key, bucket->hkey, strlen((char*)key))) {
            // �����ͷ���ڵ�ƥ�䣬��ֱ�ӷ�������
            if (bucket == head) {
                return bucket->hdata;
            } else if (head != bucket && bucket != head->tail) {
                // ���bucket�ڵ�ǰhashͰ���м�λ�ã��ͽ����ƶ���ͷ������ָ��ǰ��tail
                bucket->prev->next = bucket->next;
                bucket->next->prev = bucket->prev;

                bucket->tail = head->tail;

                lru_bucket_move(bucket, head);
            } else if (bucket == head->tail && head->capacity>1) {
                // ����������β������β�����Լ�����һ���ڵ�
                bucket->prev->next = NULL;
                bucket->tail = bucket->prev;

                lru_bucket_move(bucket, head);
            }
            g_htable->bucket[index] = bucket;
            return bucket->hdata;
        }
        bucket = bucket->next;
    }
    return NULL;
}

int hash_set(void *key, void* data) {
    if (!key || !data || !g_htable) {
        DEBUG("input para is NULL\n");
        return FALSE;
    }

    uint32_t index = g_htable->hash_key(key);
    struct hash_bucket* head = g_htable->bucket[index];

    // ��ʼ����һ��hashͰ
    if (!head) {
        head = (struct hash_bucket*)malloc(sizeof(struct hash_bucket));
        if (!head) {
            DEBUG("no memory for more hash_bucket\n");
            return FALSE;
        }

        memset(head, 0, sizeof(*head));
        head->capacity++;

        head->hkey  = strdup((char *)key);
        head->hdata = strdup((char *)data);
        head->tail  = head;
        g_htable->bucket[index] = head;
        return TRUE;
    }

    // ����Ѿ�����hashͰ����Ƚϵ�ǰ��key�Ƿ��Ѿ����ڣ��������޸Ķ�Ӧ������
    struct hash_bucket *tmp = head;
    while(tmp) {
        if (0 == g_htable->hash_cmp(key, tmp->hkey, strlen((char*)key))) {
            free(tmp->hdata);
            tmp->hdata = strdup((char*)data);
            return TRUE;
        }
        tmp = tmp->next;
    }

    // �����Ӧ��key�����ڣ��򽫵�ǰkey�����ݷ�hashͰ��ͷ���������ȵ����ݣ��´β�ѯ��������
    int capacity = head->capacity;
    struct hash_bucket *new_bucket =
        (struct hash_bucket *)malloc(sizeof(struct hash_bucket));
    if (!new_bucket) {
        DEBUG("no memory for more hash_bucket\n");
        return FALSE;
    } else {
        memset(new_bucket, 0, sizeof(*new_bucket));
    }

    // �����ǰhashͰ�ĳ�ͻ�������Ѿ��ﵽ���ֵ����β�������ݶ���
    // �򻯴������ﲻ��ʱ�����ڵļ��㣬����β���Ľڵ�˵��lookup�Ĵ�����
    if (capacity >= HASH_BUCKET_CAPACITY_MAX) {
        struct hash_bucket *tail = head->tail;
        head->tail = tail->prev;

        tail->prev->next = NULL;
        bucket_free(tail);
        capacity--;
    }

    head->prev = new_bucket;
    new_bucket->next = head;
    new_bucket->capacity = capacity + 1;
    new_bucket->tail = head->tail;
    head->tail = NULL;

    new_bucket->hkey  = strdup((char *)key);
    new_bucket->hdata = strdup((char *)data);

    g_htable->bucket[index] = new_bucket;

    return TRUE;
}

int hash_delete(void *key) {
    if (!key || !g_htable) {
        DEBUG("input para is NULL\n");
        return FALSE;
    }

    uint32_t index = g_htable->hash_key(key);
    struct hash_bucket* head = g_htable->bucket[index];
    struct hash_bucket* bkt = head;

    while(bkt) {
        if (0 == g_htable->hash_cmp(key, bkt->hkey, strlen((char*)key))) {
            if (head != bkt && bkt != head->tail) {
                bkt->prev->next = bkt->next;
                bkt->next->prev = bkt->prev;

            } else if (bkt == head->tail && head->capacity>1) {
                bkt->prev->next = NULL;
                bkt->tail = bkt->prev;

            } else {
                if (bkt->next) {
                    bkt->next->tail = bkt->tail;
                    bkt->next->capacity = bkt->capacity;
                    bkt->next->prev = NULL;
                    g_htable->bucket[index] = bkt->next;
                } else {
                    g_htable->bucket[index] = NULL;
                }
            }

            bucket_free(bkt);
            if (g_htable->bucket[index]) {
                g_htable->bucket[index]->capacity--;
            }

            return TRUE;
        }
        bkt = bkt->next;
    }
    return FALSE;
}

static void bucket_print(struct hash_bucket** ptr) {
    struct hash_bucket *bkt = *ptr;
    while(bkt) {
        printf("key=[%s],data=[%s], thread-id: %d\n", (char*)bkt->hkey, (char*)bkt->hdata, pthread_self());
        bkt = bkt->next;
    }
}

void hash_iter_print() {
    if (g_htable) {
        for(int i=0; i<HASH_BUCKET_MAX; i++) {
            if (g_htable->bucket[i]) {
                bucket_print(&g_htable->bucket[i]);
            }
        }
    }
}

