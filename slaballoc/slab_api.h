//initial slab api header
#define PAGE_SIZE 4096
#define SMALL_CACHE_SIZE (4096/8)
#include <stdio.h>
#include <stdlib.h>

struct kmem_cache{
	char* name;
	int size;
	//just made a simple circlular doubly linked list, not partitioned
	struct kmem_slab * slab;
};

struct kmem_slab{
	struct kmem_slab *next, *prev;
	
	struct kmem_bufctl *bufctl_fl;

	int refcnt;
};

struct kmem_bufctl{
	struct kmem_bufctl * next, *prev;
	//back pointer to slab
	struct kmem_slab * slab;
	
	//pointer to buf
	void* buf;
};

struct kmem_cache *kmem_cache_create(char * name, size_t size, int align);

void * kmem_cache_alloc(struct kmem_cache *cp, int flags);

void * kmem_smallcache_alloc(struct kmem_cache *cp, int flags);

void kmem_cache_free(struct kmem_cache *cp, void *buf);

void kmem_cache_destroy(struct kmem_cache *cp);

void kmem_cache_grow(struct kmem_cache *cp, size_t size);

void kmem_small_cache_grow(struct kmem_cache *cp);


struct kmem_cache *bufctl_cache;
struct kmem_cache *slab_cache;

