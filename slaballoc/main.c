#include <stdio.h>
#include <assert.h>
#include "slab_api.h"

struct test{
	int one;
	int two;
	struct test *next;
};

struct test_large{
	int one;
	int two;
	char buf[516];
};

//bigger than a page
struct test_huge{
	int one;
	int two;
	char buf[4096];
};

int
main(){
  //      slab_cache = kmem_cache_create("slab_cache", sizeof(struct kmem_slab), 0);	
	
//	assert(bufctl_cache && slab_cache);
	printf("\nTesting large caches:\n");	
	//testing large cache
	struct kmem_cache * cachelarge = kmem_cache_create("large", sizeof(struct test_large), 0);
	struct test_large * tl = kmem_cache_alloc(cachelarge, 0);	
	struct test_large * tl1 = kmem_cache_alloc(cachelarge, 0);	
	tl1->one = 2;	
	struct test_large * tl2 = kmem_cache_alloc(cachelarge, 0);	
	tl2->one = 3;	
	struct test_large * tl3 = kmem_cache_alloc(cachelarge, 0);	
	tl3->one = 4;	
	printf("cachelarge->one:%d\n", tl1->one);	
	printf("cachelarge->one:%d\n", tl2->one);	
	printf("cachelarge->one:%d\n", tl3->one);	

	printf("Test to force cache to grow more objects:\n");
	int i = 0;
	for(i = 0; i < 30; i ++){
		tl3 = kmem_cache_alloc(cachelarge, 0);
	}
	tl3->one = 30;
	printf("cachelarge->one:%d\n", tl3->one);	


	printf("\n\nTesting small caches:\n");	
	//testing small cache create:
	struct kmem_cache * cache = kmem_cache_create("test", sizeof(struct test), 0);
	assert(cache);
	
	//first slab allocate+first object get
	struct test * testbuf= (struct test *)kmem_cache_alloc(cache, 0);	
	assert(testbuf);
	testbuf->one = 1;
	testbuf->two = 2;
	printf("test->one:%d\n", testbuf->one);	

	//second object get from initial slab 
	struct test * testbuf2= (struct test *)kmem_cache_alloc(cache, 0);	
	assert(testbuf2);
	testbuf2->one = 8;
	testbuf2->two = 9;
	printf("test2->one:%d\n", testbuf2->one);
	printf("test1->one:%d\n", testbuf->one);
	
	printf("\n\nTesting caches for objects > page:\n");	
	//testing large cache
	struct kmem_cache * cachehuge = kmem_cache_create("huge", sizeof(struct test_huge), 0);
	struct test_huge * th = kmem_cache_alloc(cachehuge, 0);	
	struct test_huge * th1 = kmem_cache_alloc(cachehuge, 0);	
	th1->one = 500;	
	struct test_huge * th2 = kmem_cache_alloc(cachehuge, 0);	
	th2->one = 600;	
	struct test_huge * th3 = kmem_cache_alloc(cachehuge, 0);	
	th3->one = 700;	
	printf("cachelarge->one:%d\n", th1->one);	
	printf("cachelarge->one:%d\n", th2->one);	
	printf("cachelarge->one:%d\n", th3->one);	


	printf("\n\nTesting kmem_cache_free:\n");	
	kmem_cache_free(cachelarge, (void *)th2);
	struct test_large * th4 = kmem_cache_alloc(cachelarge, 0);	
	th2->one = 1000;	
	printf("attempting to print free'd object, should be 0 now: %d\n", th4->one);
	th4->one = 300;	
	printf("now printing object allocated on freed cache: th4->one: %d\n", th4->one);

	return 0;
}



















