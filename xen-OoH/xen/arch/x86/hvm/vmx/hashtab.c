#include <xen/lib.h>
#include <xen/xmalloc.h>
#include <xen/errno.h>

#include <asm/hvm/vmx/hashtab.h>
#define SIZE 25

struct hashtab *hashtab_create(u32 (*hash_value)(struct hashtab *h,
						 const void *key),
            int (*keycmp)(struct hashtab *h, const void *key1,
			  const void *key2), u32 size)
{
    struct hashtab *p = xzalloc(struct hashtab);

    if ( p == NULL )
        return p;

    p->size = size;
    p->hash_value = hash_value;
    p->keycmp = keycmp;
    p->htable = xzalloc_array(struct hashtab_node *, size);
    if ( p->htable == NULL )
    {
        xfree(p);
        return NULL;
    }

    return p;
}

int hashtab_insert(struct hashtab *h, void *key, void *datum)
{
    u32 hvalue;
    struct hashtab_node *prev, *cur, *newnode;

    if ( !h || h->nel == HASHTAB_MAX_NODES )
        return -EINVAL;

    hvalue = h->hash_value(h, key);
    prev = NULL;
    cur = h->htable[hvalue];
    while ( cur && h->keycmp(h, key, cur->key) > 0 )
    {
        prev = cur;
        cur = cur->next;
    }

    if ( cur && (h->keycmp(h, key, cur->key) == 0) )
        return -EEXIST;

    newnode = xzalloc(struct hashtab_node);
    if ( newnode == NULL )
        return -ENOMEM;
    newnode->key = key;
    newnode->datum = datum;
    if ( prev )
    {
        newnode->next = prev->next;
        prev->next = newnode;
    }
    else
    {
        newnode->next = h->htable[hvalue];
        h->htable[hvalue] = newnode;
    }

    h->nel++;
    return 0;
}

void *hashtab_search(struct hashtab *h, const void *key)
{
    u32 hvalue;
    struct hashtab_node *cur;

    if ( !h )
        return NULL;

    hvalue = h->hash_value(h, key);
    cur = h->htable[hvalue];
    while ( cur != NULL && h->keycmp(h, key, cur->key) > 0 )
        cur = cur->next;

    if ( cur == NULL || (h->keycmp(h, key, cur->key) != 0) )
        return NULL;

    return cur->datum;
}

void hashtab_destroy(struct hashtab *h)
{
    u32 i;
    struct hashtab_node *cur, *temp;

    if ( !h )
        return;

    for ( i = 0; i < h->size; i++ )
    {
        cur = h->htable[i];
        while ( cur != NULL )
        {
            temp = cur;
            cur = cur->next;
            xfree(temp);
        }
        h->htable[i] = NULL;
    }

    xfree(h->htable);
    h->htable = NULL;

    xfree(h);
}

int hashtab_map(struct hashtab *h,
        int (*apply)(void *k, void *d, void *args),
        void *args)
{
    u32 i;
    int ret;
    struct hashtab_node *cur;

    if ( !h )
        return 0;

    for ( i = 0; i < h->size; i++ )
    {
        cur = h->htable[i];
        while ( cur != NULL )
        {
            ret = apply(cur->key, cur->datum, args);
            if ( ret )
                return ret;
            cur = cur->next;
        }
    }
    return 0;
}


void hashtab_stat(struct hashtab *h, struct hashtab_info *info)
{
    u32 i, chain_len, slots_used, max_chain_len;
    struct hashtab_node *cur;

    slots_used = 0;
    max_chain_len = 0;
    for ( slots_used = max_chain_len = i = 0; i < h->size; i++ )
    {
        cur = h->htable[i];
        if ( cur )
        {
            slots_used++;
            chain_len = 0;
            while ( cur )
            {
                chain_len++;
                cur = cur->next;
            }

            if ( chain_len > max_chain_len )
                max_chain_len = chain_len;
        }
    }

    info->slots_used = slots_used;
    info->max_chain_len = max_chain_len;
}

unsigned int vtf_hash(struct hashtab *h, const void *key)
{
    unsigned int size = h->size, keyp = (unsigned int)(uintptr_t)key;
    return keyp%size;
}

int vtf_cmp(struct hashtab *h, const void *key1, const void *key2)
{
    return (key1 == key2);
}

struct hashtab *vtf_hashmap_init(void)
{
    return hashtab_create(vtf_hash, vtf_cmp, SIZE);
}
/*
 */
