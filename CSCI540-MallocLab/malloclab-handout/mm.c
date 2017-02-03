/* 
 * mm-implicit.c -  Simple allocator based on implicit free lists, 
 *                  first fit placement, and boundary tag coalescing. 
 *
 * Each block has header and footer of the form:
 * 
 *      31                     3  2  1  0 
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      ----------------------------------- 
 * 
 * where s are the meaningful size bits and a/f is set 
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap  
 *  -----------------------------------------------------------------   
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
	/* Team name */
	"malloc_priyanka",
	/* First member's full name */
	"Priyanka Pravinkumar Chordia",
	/* First member's email address */
	"pchordia@mail.csuchico.edu",
	/* (leave blank) */
	"",
	/* (leave blank) */
	""
};

/////////////////////////////////////////////////////////////////////////////
// Constants and macros
//
// These correspond to the material in Figure 9.43 of the text
// The macros have been turned into C++ inline functions to
// make debugging code easier.
//
/////////////////////////////////////////////////////////////////////////////
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

static inline int MAX(int x, int y) {
	return x > y ? x : y;
}

//
// Pack a size and allocated bit into a word
// We mask of the "alloc" field to insure only
// the lower bit is used
//
static inline size_t PACK(size_t size, int alloc) {
	return ((size) | (alloc & 0x1));
}

//
// Read and write a word at address p
//
static inline size_t GET(void *p) { return  *(size_t *)p; }
static inline void PUT( void *p, size_t val)
{
	*((size_t *)p) = val;
}

//
// Read the size and allocated fields from address p
//
static inline size_t GET_SIZE( void *p )  { 
	return GET(p) & ~0x7;
}

static inline int GET_ALLOC( void *p  ) {
	return GET(p) & 0x1;
}

//
// Given block ptr bp, compute address of its header and footer
//
static inline void *HDRP(void *bp) {

	return ( (char *)bp) - WSIZE;
}
static inline void *FTRP(void *bp) {
	return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

//
// Given block ptr bp, compute address of next and previous blocks
//
static inline void *NEXT_BLKP(void *bp) {
	return  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));
}

static inline void* PREV_BLKP(void *bp){
	return  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
}

/////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//

static char *heap_listp;  /* pointer to first block */  
static void *root;

//
// function prototypes for internal helper routines
//
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);

//
// mm_init - Initialize the memory manager 
//
int mm_init(void) 
{
	heap_listp = mem_sbrk(4*WSIZE);
	if(heap_listp != (void*)-1)
	{
		PUT(heap_listp, 0);
		PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
		PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
		PUT(heap_listp + (3*WSIZE), PACK(0, 1));
		heap_listp += (2*WSIZE);
		root = heap_listp;
		extend_heap((CHUNKSIZE/WSIZE)+12);
	}
	else
	{
		return -1;
	}
	return 0;
}


//
// extend_heap - Extend heap with free block and return its block pointer
//
static void *extend_heap(size_t words) 
{
	if(words%2 == 1)
	{
		words++;
	}
	char* base_pointer = mem_sbrk(words * WSIZE);
	size_t size = PACK((words * WSIZE), 0);
	if((int)base_pointer == -1)
	{
		return NULL;
	}
	PUT(HDRP(base_pointer), size);
	PUT(FTRP(base_pointer), size);
	PUT(HDRP(NEXT_BLKP(base_pointer)), PACK(0, 1));
	return coalesce(base_pointer);
}


//
// Practice problem 9.8
//
// find_fit - Find a fit for a block with asize bytes 
//
static void *find_fit(size_t asize)
{
	void* initial = root;
  size_t header, size;
  char alloc;
	for(; GET_SIZE(HDRP(root)) > 0; root = NEXT_BLKP(root))
	{
    header = GET(HDRP(root));
	  size = header & ~7;
		alloc = header & 1;
		if( size >= asize && !alloc)
			return root;
	}
	root = heap_listp;
	for(; root < initial; root = NEXT_BLKP(root))
	{
    header = GET(HDRP(root));
		size = header & ~7;
		alloc = header & 1;
		if( size >= asize && !alloc)
			return root;
	}
	return NULL; /* no fit */
}

// 
// mm_free - Free a block 
//
void mm_free(void *bp)
{
	if(bp == NULL)
		return;
	PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
	PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
	coalesce(bp);
}

//
// coalesce - boundary tag coalescing. Return ptr to coalesced block
//
static void *coalesce(void *bp) 
{
	int prev_b = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	int next_b = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

	size_t size = GET_SIZE(HDRP(bp));
	size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
	size_t prev_size = GET_SIZE(FTRP(PREV_BLKP(bp)));

	if(prev_b && next_b)
    return bp;
	else if(!prev_b && next_b)
	{
    size = size + prev_size;
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
  else if(prev_b && !next_b)
  {
    size = size + next_size;
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    PUT(HDRP(bp), PACK(size, 0));
  }
	else
	{
    size = size + next_size + prev_size;
		size_t new_size = PACK(size, 0);
		PUT(FTRP(NEXT_BLKP(bp)), new_size);
		PUT(HDRP(PREV_BLKP(bp)), new_size);
		bp = PREV_BLKP(bp);
	}
	if(bp < root && root < NEXT_BLKP(bp))
	{
		root = bp;
	}
	return bp;
}

//
// mm_malloc - Allocate a block with at least size bytes of payload 
//
void *mm_malloc(size_t size) 
{
	size_t asize, esize;      
	void *base_pointer, *hsize;

	if (size <= 0)
		return (NULL);

	if (size >= DSIZE)
    asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
	else
    asize = 2 * DSIZE;

	if ((base_pointer = find_fit(asize)) == NULL) 
		esize = MAX(asize, CHUNKSIZE);
	else
  {
    place(base_pointer, asize);
    return (base_pointer);
  }

	hsize = extend_heap(esize / WSIZE);

	if ((base_pointer = hsize) != NULL)  
	 place(base_pointer, asize);
  else
    return NULL;

	return base_pointer;
} 

//
//
// Practice problem 9.9
//
// place - Place block of asize bytes at start of free block bp 
//         and split if remainder would be at least minimum block size
//
static void place(void *bp, size_t asize)
{
	int size = GET_SIZE(HDRP(bp));

  if(size < asize + 2*DSIZE)
	{
		PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
	}
	else
	{
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(size - asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size - asize, 0));	
	}
}


//
// mm_realloc -- implemented for you
//
void *mm_realloc(void *ptr, size_t size)
{
	void *newp;
	size_t copySize;

	newp = mm_malloc(size);
	if (newp == NULL) {
		printf("ERROR: mm_malloc failed in mm_realloc\n");
		exit(1);
	}
	copySize = GET_SIZE(HDRP(ptr));
	if (size < copySize) {
		copySize = size;
	}
	memcpy(newp, ptr, copySize);
	mm_free(ptr);
	return newp;
}

//
// mm_checkheap - Check the heap for consistency 
//
void mm_checkheap(int verbose) 
{
	//
	// This provided implementation assumes you're using the structure
	// of the sample solution in the text. If not, omit this code
	// and provide your own mm_checkheap
	//
	void *bp = heap_listp;

	if (verbose) {
		printf("Heap (%p):\n", heap_listp);
	}

	if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
		printf("Bad prologue header\n");
	}
	checkblock(heap_listp);

	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (verbose)  {
			printblock(bp);
		}
		checkblock(bp);
	}

	if (verbose) {
		printblock(bp);
	}

	if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
		printf("Bad epilogue header\n");
	}
}

static void printblock(void *bp) 
{
	size_t hsize, halloc, fsize, falloc;

	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));  
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));  

	if (hsize == 0) {
		printf("%p: EOL\n", bp);
		return;
	}

	printf("%p: header: [%d:%c] footer: [%d:%c]\n",
			bp, 
			(int) hsize, (halloc ? 'a' : 'f'), 
			(int) fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
	if ((size_t)bp % 8) {
		printf("Error: %p is not doubleword aligned\n", bp);
	}
	if (GET(HDRP(bp)) != GET(FTRP(bp))) {
		printf("Error: header does not match footer\n");
	}
}
