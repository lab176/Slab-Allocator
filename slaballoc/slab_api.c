#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "slab_api.h"

struct kmem_cache*
kmem_cache_create(char* name, size_t size, int align){
	if(bufctl_cache == NULL){
		//clearly this is a hack that deserves a slap to the face, but I need a way to garuntee the global bufctl_cache
		//is only allocated once, regardless of how many caches are created and without the help of the client.
		//so, in the interest of my time... this is what we got. if this was in the kernel we'd have to devise a non-recursive route.
		//I set it equal to one just so it won't recurse till infinity. 
		bufctl_cache = (void *)1;
		bufctl_cache = kmem_cache_create("bufctl", sizeof(struct kmem_bufctl), 0);		
	}
	
	struct kmem_cache * cache = (struct kmem_cache *)malloc(sizeof(struct kmem_cache));
	assert(cache);	
	cache->name = name;
	cache->size = size;
	cache->slab = NULL;	

	//alocate initial slab
	if(size < SMALL_CACHE_SIZE){
		kmem_small_cache_grow(cache);
		return cache;
	}
	//normal cache create
	kmem_cache_grow(cache, size);
	return cache;
}

void
kmem_small_cache_grow(struct kmem_cache *cp){
	//alloc a page of memory, this will serve as the slab
	void* slab = malloc(PAGE_SIZE);
	assert(slab);

	/* Initialize slab struct at end of memory region
	 *  -This is gross yes, but need to do pointer math to allocate the proper spacing for the two structs
	 *  -In the paper they said they use no bufctls for small caches, but I am going to use just one at the end so that 
	 *          I have a pointer do math to retrieve the "objects". I'd be surprised if the one extra dereference hurts performance. 
	 */
	struct kmem_slab * info = (struct kmem_slab *)( slab + (PAGE_SIZE-(sizeof(struct kmem_slab) + sizeof(struct kmem_bufctl))));
	info->refcnt = 0;

	/* Initialize the only bufctl on slab, not going to use bufctl_cache becasue its the only one*/
	info->bufctl_fl = (struct kmem_bufctl *)(slab + (PAGE_SIZE-sizeof(struct kmem_bufctl)));
	info->bufctl_fl->next = NULL;
	info->bufctl_fl->prev = NULL;
	
	//point to beginning of slab memory
	info->bufctl_fl->buf = slab;
	
	//if this is first slab allocated	
	if(cp->slab == NULL){
		info->next = info;
		info->prev = info;
		 	
		cp->slab = info;
		return;
	}

	/*add slab to the head of the circular linked list of slab*/
	info->next = cp->slab;
	info->prev = cp->slab->prev;		
	cp->slab->prev->next = info;
	cp->slab->prev = info;
	cp->slab = info;
}

//add new slab to cache
void
kmem_cache_grow(struct kmem_cache* cp, size_t size){
	assert(cp);

	//alloc new slab
	struct kmem_slab *slab = (struct kmem_slab *)malloc(sizeof(struct kmem_slab));
	assert(slab);
	slab->refcnt = 0;
	
	/*add slab to the head of the circular linked list of slab*/
	if(cp->slab == NULL){
		slab->next = slab;
		slab->prev = slab;
		cp->slab = slab;
	}else{
		slab->next = cp->slab;
		slab->prev = cp->slab->prev;		
		cp->slab->prev->next = slab;
		cp->slab->prev = slab;
		cp->slab = slab;
	}
	

	/*
	 *
	 *The paper uses a "self scaling hash table" to map bufctl's to their corresponding bufs.
	 * in the interest of time and pending research deadlines ;) just gonna go ahead and make a pointer to a contiguous mem region.
	 * 
	 *
	 * */
	void *buf = malloc(PAGE_SIZE);
	assert(buf);
	
	/*
	 * This relies on the user to have already created a global cache of bufctl objects. Obviously that breaks the whole api abstraction
	 * but it's the easiest way. I initialize the global caches in my main.c
	*/
	if(bufctl_cache == NULL){
	       	printf("please initialize a global cache of bufctl structs.\n"); 
	}
	struct kmem_bufctl *temp = (struct kmem_bufctl*)kmem_cache_alloc(bufctl_cache, 0);
	temp->slab = slab;
	temp->buf = buf;
	cp->slab->bufctl_fl = temp;
	temp->prev = NULL;
	
	int count = size;	
	int i;	
	/*lets just pick 30 as the magic number for number of objects to initialize, based on nothing scientific at all*/
	for(i = 0; i < 30; i++){
		struct kmem_bufctl *newobj = (struct kmem_bufctl*)kmem_cache_alloc(bufctl_cache, 0);
		newobj->slab = slab;
		
		//if we ran out of space on this page, malloc another:
		if(count > PAGE_SIZE){
			buf = malloc(PAGE_SIZE);
			count = 0;
			newobj->buf = buf+count;
			continue;
		}

		//increment buffer so it lies on same contiguous page
		newobj->buf = buf+count;
		count += size;
		temp->next = newobj;
		temp->next->prev = temp;
		temp = temp->next;	
	}
	//set last one in free list to NULL
	temp->next = NULL;		
}

void*
kmem_cache_alloc(struct kmem_cache *cp, int flags){
	assert(flags == 0);
	assert(cp);
	if(cp->size < SMALL_CACHE_SIZE){
		return kmem_smallcache_alloc(cp, flags);
	}
	if(cp->slab->bufctl_fl->next == NULL){
	       	printf("Allocating more objects.\n");
		kmem_cache_grow(cp, cp->size);
		return NULL;
	}
	void *ret = cp->slab->bufctl_fl->buf;
	cp->slab->bufctl_fl = cp->slab->bufctl_fl->next;	
	return ret;	
}

void*
kmem_smallcache_alloc(struct kmem_cache *cp, int flags){
	assert(flags == 0);
	if(cp->slab == NULL){
		printf("kmem_cache is NULL, did you call kmem_cache_create()?");
		return NULL;
	}

	void * ret;

	/*if the slab is empty (all the object are 'taken') then create a new slab, and try again*/
	if((cp->slab->bufctl_fl->buf+cp->size) >= (void *)(cp->slab)){
		printf("need new slab\n");
		kmem_small_cache_grow(cp);
		ret = cp->slab->bufctl_fl->buf;
		cp->slab->refcnt++;
		cp->slab->bufctl_fl->buf += cp->size;
		return ret;
	}	

	/*simple return mem address of the preallocated 'object'*/
	ret = cp->slab->bufctl_fl->buf;
	cp->slab->refcnt++;

	/*increment mem pointer to next "object" in buffer*/
	cp->slab->bufctl_fl->buf += cp->size;

	return ret;
}

void
kmem_cache_free(struct kmem_cache *cp, void * buf){
	//I didn't do the hash table, so this is going to be neither pretty nor quick:
	//To "add" the buf back to the cache, thus 'freeing' it,
	//we are just going to put it at the front of the cache, allowing the next allocation to use it.
	if(cp->size < (PAGE_SIZE/8)){
	       	printf("We don't do small cache frees yet.\n"); 
		return;
	}
	struct kmem_bufctl * temp = cp->slab->bufctl_fl; 
	while(buf != temp->buf){
		if(temp->prev == NULL){
			return;
		}
		temp = temp->prev;
	}

	temp->prev->next = temp->next;
	temp->next->prev = temp->prev;
	temp->prev = cp->slab->bufctl_fl;
	temp->next = cp->slab->bufctl_fl->next;	
	cp->slab->bufctl_fl->next = temp;
}

void
kmem_cache_destroy(struct kmem_cache *cp){
	//out of time.
	free(cp);
}



