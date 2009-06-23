/*
 * Copyright (c) 2009 Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, California 95054, U.S.A. All rights reserved.
 * 
 * U.S. Government Rights - Commercial software. Government users are
 * subject to the Sun Microsystems, Inc. standard license agreement and
 * applicable provisions of the FAR and its supplements.
 * 
 * Use is subject to license terms.
 * 
 * This distribution may include materials developed by third parties.
 * 
 * Parts of the product may be derived from Berkeley BSD systems,
 * licensed from the University of California. UNIX is a registered
 * trademark in the U.S.  and in other countries, exclusively licensed
 * through X/Open Company, Ltd.
 * 
 * Sun, Sun Microsystems, the Sun logo and Java are trademarks or
 * registered trademarks of Sun Microsystems, Inc. in the U.S. and other
 * countries.
 * 
 * This product is covered and controlled by U.S. Export Control laws and
 * may be subject to the export or import laws in other
 * countries. Nuclear, missile, chemical biological weapons or nuclear
 * maritime end uses or end users, whether direct or indirect, are
 * strictly prohibited. Export or reexport to countries subject to
 * U.S. embargo or to entities identified on U.S. export exclusion lists,
 * including, but not limited to, the denied persons and specially
 * designated nationals lists is strictly prohibited.
 * 
 */
/*
  Author: Grzegorz Milos, Sun Microsystems, Inc., summer intern 2007.
 */
#include <os.h>
#include <sched.h>
#include <spinlock.h>
#include <time.h>
#include <xmalloc.h>

extern u32 rand_int(void);
extern void seed(u32 s);

#define NUM_THREADS     10
#define NUM_OBJECTS     1000
#define MAX_OBJ_SIZE    0x1FF

#define PRINTK(_f, _a...) \
    printk("[%s] " _f, current->name, ## _a)

struct object
{
    int size;
    void *pointer;
    int allocated;
    int read;
};
static struct object objects[NUM_THREADS][NUM_OBJECTS];
static int remaining_threads_count;
static DEFINE_SPINLOCK(thread_count_lock);


static void verify_object(struct object *object, int thread_id)
{
    u32 id = ((unsigned long)object - (unsigned long)objects[thread_id]) 
                                                        / sizeof(struct object);
    u8 *test_value = (u8 *)object->pointer, *pointer;
    int i;

    if(!object->allocated)
        return;
    object->read = 1; 
    for(i=0; i<object->size; i++)
        if(test_value[i] != (id & 0xFF))
            goto fail;
    return;
    
fail:                
    PRINTK("Failed verification for object id=%d, size=%d, i=%d,"
           " got=%x, expected=%x\n",
            id, object->size, i, test_value[i], id & 0xFF);
 
    pointer = (u8 *)objects[thread_id][id].pointer;

    PRINTK("Object %d, size: %d, pointer=%lx\n",
               id, objects[thread_id][id].size, 
               objects[thread_id][id].pointer);
    printk("[%s] ", current->name);
    for(i=0; i<objects[thread_id][id].size; i++)
         printk("%2x", pointer[i]);
    printk("\n");

  
    ok_exit();
}


static void allocate_object(u32 id, int thread_id)
{
    /* If object already allocated, don't allocate again */
    if(objects[thread_id][id].allocated)
        return;
    /* +1 protects against 0 allocation */
    objects[thread_id][id].size = (rand_int() & MAX_OBJ_SIZE) + 1;
    objects[thread_id][id].pointer = malloc(objects[thread_id][id].size);
    objects[thread_id][id].read = 0;
    objects[thread_id][id].allocated = 1;
    memset(objects[thread_id][id].pointer, id & 0xFF, objects[thread_id][id].size);
    if(id % (NUM_OBJECTS / 20) == 0)
        PRINTK("Allocated object size=%d, pointer=%p.\n",
                objects[thread_id][id].size, objects[thread_id][id].pointer);
}

static void free_object(struct object *object, int thread_id)
{
    
    if(!object->allocated)
        return;
    free(object->pointer);
    object->size = 0;
    object->read = 0;
    object->allocated = 0;
    object->pointer = 0;
}

static void USED mem_allocator(void *p)
{
    u32 count = 1;
    u32 thread_id = (u32)(u64)p;

    PRINTK("Threaded memory allocator tester #%d started.\n", thread_id);
    memset(objects[thread_id], 0, sizeof(struct object) * NUM_OBJECTS);
    for(count=0; count < NUM_OBJECTS; count++)
    {
        allocate_object(count, thread_id);
        /* Randomly read an object off */
        if(rand_int() & 1)
        {
            u32 to_read = count & rand_int();

            verify_object(&objects[thread_id][to_read], thread_id);
        }
        /* Randomly free an object */
        if(rand_int() & 1)
        {
            u32 to_free = count & rand_int();

            free_object(&objects[thread_id][to_free], thread_id);
        }
    }
    
    PRINTK("Destroying remaining objects.\n"); 
    for(count = 0; count < NUM_OBJECTS; count++)
    {
        if(objects[thread_id][count].allocated)
        {
            verify_object(&objects[thread_id][count], thread_id);
            free_object(&objects[thread_id][count], thread_id);
        }
    } 
   
    spin_lock(&thread_count_lock); 
    remaining_threads_count--;
    if(remaining_threads_count == 0)
    {
        spin_unlock(&thread_count_lock); 
        PRINTK("ALL SUCCESSFUL\n"); 
        ok_exit();
    }
    
    spin_unlock(&thread_count_lock); 
    PRINTK("Success.\n");
}

int guk_app_main(start_info_t *si)
{
    int i;
    char thread_name[256];
    
    printk("Private appmain.\n");
    seed((u32)NOW());    
    remaining_threads_count = NUM_THREADS; 
    for(i=0; i<NUM_THREADS; i++)
    {
        sprintf(thread_name, "mem_allocator_%d", i);
        create_thread(strdup(thread_name), mem_allocator, UKERNEL_FLAG, (void *)(u64)i);
    }

    return 0;
}