#include "threads/malloc.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"



/* Magic number for detecting arena corruption. */
#define ARENA_MAGIC 0x9a548eed

void* heap;
struct lock lo;

 struct address{
  char* addr;
  int arenanumber;
 };

struct arena{
  unsigned magic;
  int pagenumber;
  metadata_t* freelist[8];
  struct list_elem elem;
};

metadata_t* find_buddy(metadata_t*);


struct address blocklist[8][1000];
int sizeblock[8];

struct list arenalist;

/**** SIZES FOR THE FREE LIST ****
 * freelist[0] -> 16
 * freelist[1] -> 32
 * freelist[2] -> 64
 * freelist[3] -> 128
 * freelist[4] -> 256
 * freelist[5] -> 512
 * freelist[6] -> 1024
 * freelist[7] -> 2048
 */


void* getblock(struct arena* a, size_t size, int arenanumber){
  /* Get the index of the freelist we need based on the
     needed size */
  int needed = size + sizeof(metadata_t);
  int index = get_index(needed);
  a->pagenumber = arenanumber;

  /* Declares variables for the front of the freelist
     and the next */
  metadata_t *front, *next;

  /* If there's a block of memory of that size in the
     freelist get it and return it */
  
  if (a->freelist[index]) {
    front = a->freelist[index];
    next = front->next;
    
    /* Set to to in_use, remove its next and prev */
    front->in_use = 1; 
    front->next = NULL;
    front->prev = NULL;

    

    /* If there is a next block in the freelis remove 
       its previous and set the front of the list to it
       else there's nothing in the list */
    
    if (next) {
      next->prev = NULL;
      a->freelist[index] = next;
    } else {
      a->freelist[index] = NULL;
    }
   
    (blocklist[index][sizeblock[index]]).arenanumber = arenanumber;
    (blocklist[index][sizeblock[index]]).addr = (char*)front+ sizeof(metadata_t);
    sizeblock[index]++;

    
    return offset_pointer(front, 1);
  }

  int available;
  available = index;
  
  while (available < 8 && !a->freelist[available]) {
    available = available + 1;
  }
  
  /* If we are here that means there's no block of memory
     of our size in the freelist */
  if(available == 8)
      return NULL;

  metadata_t *current, *new;

  /* While the available isn't the index, keep breaking down
     blocks of memory until you have one at the index you want */
  while (available != index) {
    //printf("available : %d , index : %d\n",available,index);
    current = a->freelist[available];
    current->size =  current->size/2; // split in half
    //printf("divding size : %d\n",current->size);
    /* Create a new block at an address that is <size> away
       from current */
    new = (metadata_t *) ((char *) current + current->size);
    new->size = current->size;
    new->in_use = 0;
    /* If there's something else in the linked list move it to
       the front otherwise set it to NULL */
    if (a->freelist[available]->next) {
      a->freelist[available] = a->freelist[available]->next;
      a->freelist[available]->prev = NULL;
    } else {
      a->freelist[available] = NULL;
    }

    
    --available;

    /* Set up the linked lists for the new block. */
    current->next = new;
    new->prev = current;
    a->freelist[available] = current;
    //a->freelist[available]->size = current->size;
    //printf("size malloc %d \n",a->freelist[available]->size);
  }

  /* Get the block we want to return from the freelist */
  metadata_t *ret_meta = a->freelist[index];
  //printf(" ha ha size : %d \n",ret_meta->size);
  (blocklist[index][sizeblock[index]]).arenanumber = arenanumber;
  (blocklist[index][sizeblock[index]]).addr = (char*)ret_meta+ sizeof(metadata_t);
  sizeblock[index]++; 
  /* If there's a next move it up, else set the index to NULL */
  if (a->freelist[index]->next) {
    a->freelist[index] = a->freelist[index]->next;
    a->freelist[index]->prev = NULL;
  } else {
    a->freelist[index] = NULL;
  }

 

  /* Set the values in the return block */
  ret_meta->next = NULL;
  //ret_meta->prev = NULL;
  //ret_meta->size = a->freelist[index]->size;
  ret_meta->in_use = 1;
  //printf("haha size : %d\n",a->freelist[index]->size);
  //print_page();
  
  return offset_pointer(ret_meta, 1);
  
  /* Get the next largest block of memory in the freelist */
  
}

int pagenumber = 0;

void* malloc(size_t size)
{
  printf("%d\n",sizeof(struct list));
  //printf("malloc called\n");
  
  /* Size of memory needed */
  int needed = size + sizeof(metadata_t);
  
  /* if they request more than 2kb return NULL. */
  if (needed > 2048) return NULL; 
  if (size == 0)  return NULL;

  lock_acquire(&lo);

  int png = 0;
  struct list_elem* lelem;
  for(lelem = list_begin(&arenalist) ; lelem != list_end(&arenalist) ; lelem = list_next(lelem)){
    struct arena* a = list_entry(lelem,struct arena,elem);
    png++;
    void* v = getblock(a,size,png);
    if(v != NULL){
      printMemory();
      lock_release(&lo);
      return v;
    }
  }
  /* If the heap doesn't exist initialize it. If it still 
     doesn't exist after initialization it failed so 
     return NULL */
  //printf("here");
  void* t = palloc_get_page(0);

  heap = t;
  
  struct arena* a = t;
  
  a->magic = ARENA_MAGIC;

  a->freelist[7] = (metadata_t *) (t + sizeof(struct arena));

  a->freelist[7]->in_use = 0;

  a->freelist[7]->size = 2048;
  a->freelist[7]->next = NULL;
  a->freelist[7]->prev = NULL; 
  int j;
  
  pagenumber++;
  
  list_push_back(&arenalist,&a->elem);
  
  void *ti = getblock(a,size,pagenumber);
  //printf("%p ",ti);
  //printf("I am calling from malloc\n");
  printMemory();
  lock_release(&lo);
  return ti;
}


void printMemory(){
  int j;
  struct list_elem* lelem;
  struct list_elem* lelem1;
  int pg = 0;
  int i,k,l;
  printf("No. of pages allocated : %d\n",list_size(&arenalist));
  for(i = 0 ; i < 8 ; i++){
    for(j = 0 ; j < sizeblock[i] ; j++){
      for(k = j+1 ; k < sizeblock[i] ; k++){
        if((void*)blocklist[i][k].addr < (void*)blocklist[i][j].addr){
          struct address temp = blocklist[i][k];
          blocklist[i][k] = blocklist[i][j];
          blocklist[i][j] = temp;
        }
      }
    }
  }
  for(lelem = list_begin(&arenalist) ; lelem != list_end(&arenalist) ; lelem = list_next(lelem)){
    struct arena* a = list_entry(lelem,struct arena,elem);
    //printf("%p \n",(void*)a);
    pg++;
    printf("Page %d:\n",pg);
    int sz = 16;
    for(j = 0 ; j < 8 ; j++){
      printf("size %d : ",sz);
      int k;
      metadata_t* front = a->freelist[j];
      while(front != NULL){
        printf("%p ",(void*)front);
        front = front->next;
      }
      sz = sz * 2;
      printf("\n");
    }
    printf("\n");
  }
}
    
  



/* Get the index needed based on the size. Loops until the
   memory that corresponds to the index is larger than needed */
int get_index(size_t needed) {
  int index = 0;
  int memory = 16;
  while (memory < needed) {
    memory *= 2;
    index++;
  }

  return index;
}

void malloc_init(){
  printf("init....\n");
  list_init(&arenalist);
  int i;
  for(i = 0 ; i < 8 ; i++)
    sizeblock[i] = 0;

  lock_init(&lo);
  
}

/* Function to offset the pointer. Takes in a metadata_t *
   and an int offset that determines which direction to go.
   Returns either ptr + sizeof() if offset is 1 else
   returns ptr - sizeof() */
void* offset_pointer(metadata_t* ptr, int offset) {
  char *offset_ptr = (char *) ptr;
  if (offset) return offset_ptr + sizeof(metadata_t);
  else return offset_ptr - sizeof(metadata_t);
}



void free(void* ptr)
{
  if(ptr == NULL){
    return;
  }
   //print_page();
  /* Get the other block of memory and offset it correctly. */
  struct arena *a;
  struct list_elem* lelem;
  for(lelem = list_begin(&arenalist) ; lelem != list_end(&arenalist) ; lelem = list_next(lelem)){
    struct arena* ar = list_entry(lelem,struct arena,elem);
    if((void*)ptr >= (void*)ar && (void*)ptr <= (void*)ar+PGSIZE){
      a = ar;
      break;
    }
  }
  
  if(a == NULL){
    printf("address is invalid\n");
    return;
  }



  metadata_t *block = offset_pointer(ptr, 0);
  int ind = get_index(block->size);

  if(block->in_use == 0)  return;
  
  block->in_use = 0; // no longer in use

  /* Get the buddy! */
  metadata_t *buddy = find_buddy(block);

  //printf("buddy : %p\n",(void*)buddy);


  int index = get_index(block->size);
  int i,j;
  for(i = 0 ; i < sizeblock[index] ; i++){
    if((void*)blocklist[index][i].addr == ptr){
      for(j = i ; j < sizeblock[index]-1 ; j++){
        blocklist[index][j] = blocklist[index][j+1];
      }
    }
  }

  sizeblock[index]--;

  //printf("Buddy : %p\n",(void*)buddy);

  /* While buddy exists and it's not in use, merge it */
  while (buddy && !buddy->in_use) {
    /* Modify the linked list accordingly */
    //rintf("collasce....\n");
    if (buddy->next && buddy->prev) {
      buddy->next->prev = buddy->prev;
      buddy->prev->next = buddy->next;
    } else if (buddy->next) {
      buddy->next->prev = NULL;
      a->freelist[get_index(buddy->size)] = buddy->next;
      buddy->next = NULL;
    } else if (buddy->prev) {
      buddy->prev->next = NULL;
    } else {
      a->freelist[get_index(buddy->size)] = NULL;
    }
    
    /* If the address of buddy is lower than the block, 
       use that one instead */
    if (buddy < block) block = buddy;

    /* Double the size to account for the combination */
    block->size *= 2;

    /* Find the new block's buddy */
    buddy = find_buddy(block);
  }

  index = get_index(block->size);


  /* If there's something at the index position of the 
     freelist, push it to the front, otherwise just place
     it there */
  
  //printf("removeing index : %d , new size : %d\n",index,sizeblock[index]);

  if (a->freelist[index]) {
    metadata_t *front = a->freelist[index];
    front->prev = block;
    block->next = front;
  }
  a->freelist[index] = block;
  if(a->freelist[7]){
    printf("removing page..\n");
    list_remove(&a->elem);
    //printf("%d\n",list_size(&arenalist));
    palloc_free_page(a);
    printMemory();
    
    return;
  }
  
  printMemory();

}
/* Allocates and return A times B bytes initialized to zeroes.
   Returns a null pointer if memory is not available. */
void *
calloc (size_t a, size_t b) 
{
  void *p;
  size_t size;

  /* Calculate block size and make sure it fits in size_t. */
  size = a * b;
  if (size < a || size < b)
    return NULL;

  /* Allocate and zero memory. */
  p = malloc (size);
  if (p != NULL)
    memset (p, 0, size);

  return p;
}



size_t my_block_size(void* block){
  int i,j,k;
  int sz = 16;
  for(i = 0 ; i < 8 ; i++){
    for(j = 0 ; j < sizeblock[i] ; j++){
      if((void*)blocklist[i][j].addr == block){
        return sz;
      }
    }
    sz = sz * 2;
  }
  return 2048;
  
}


/* Attempts to resize OLD_BLOCK to NEW_SIZE bytes, possibly
   moving it in the process.
   If successful, returns the new block; on failure, returns a
   null pointer.
   A call with null OLD_BLOCK is equivalent to malloc(NEW_SIZE).
   A call with zero NEW_SIZE is equivalent to free(OLD_BLOCK). */
void *
realloc (void *old_block, size_t new_size) 
{
  if (new_size == 0) 
    {
      free (old_block);
      return NULL;
    }
  else 
    {
      void *new_block = malloc (new_size);
      if (old_block != NULL && new_block != NULL)
        {
          size_t old_size = my_block_size (old_block);
          size_t min_size = new_size < old_size ? new_size : old_size;
          memcpy (new_block, old_block, min_size);
          free (old_block);
        }
      return new_block;
    }
}
/* Find the buddy for a given block. This is done fairly
   simply. Since the address of the buddy should just be
   size away from the block, either add or subtract that
   depending on which buddy you have. Alternatively, you
   could XOR the address by the size, same thing as:
        ptr +/- (1 << log2(size))
   Then return it. */

metadata_t* find_buddy(metadata_t* ptr){
  unsigned int buddy = (unsigned int) ptr ^ ptr->size;
  metadata_t *b = (metadata_t *) buddy;
  //printf("buddy size: %d %d\n",ptr->size,b->size);
  if (ptr->size == b->size) return b;
  else return NULL;
}



