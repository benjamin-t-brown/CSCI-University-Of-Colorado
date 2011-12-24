/* 
 * mm.c -  Simple allocator based on implicit free lists, 
 *         first fit placement, and boundary tag coalescing. 
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
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
  /* Team name */
  "bebr5104, byki8392",
  /* First member's full name */
  "Benjamin Brown",
  /* First member's email address */
  "",
  /* Second member's full name (leave blank if none) */
  "Byung Kim",
  /* Second member's email address (leave blank if none) */
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

// Given a block ptr bp, the the next aspect
static inline char* NEXT(void *bp ){
  return ((char *)(bp)+WSIZE);
}

/////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//
void *heap_listp=NULL;   /* The global variable is a pointer to the first block */
void *header=NULL;       /* The header of the double linked list */
void *tail;              /* The tail of the double linked list */
int j=0;                 /* The global condition sign */

//
// function prototypes for internal helper routines
//
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void *place(void *bp,size_t asize);
static void printblock(void *bp); 
static void checkblock(void *bp);

//
// mm_init - Initialize the memory manager 
//
int mm_init(void)
{
    /* initialize all the global variables*/
    header=NULL;              
    tail=NULL;               /* set all the pointers to NULL*/
    j=0;                     /* set all the integers to 0 */
   
    /* initialize a start space */
    if((heap_listp=mem_sbrk(4*WSIZE))==NULL)
        return -1;          /* system call failed, return -1*/
    
    PUT(heap_listp,0);      /* alignment padding */
    
    PUT(heap_listp+WSIZE,PACK(OVERHEAD,1));   /* prologue header */ 
    PUT(heap_listp+DSIZE,PACK(OVERHEAD,1));   /* prologue footer */ 
    PUT(heap_listp+WSIZE+DSIZE,PACK(0,1));    /* epilogue header */

    heap_listp += DSIZE; /* point the list_pointer past the header */  

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(1<<4)==NULL)
        return -1;         /* If this fails, return -1 */
    
    return 0;	           /* You're good to go Jr. */
}

//
// extend_heap - Extend heap with free block and return its block pointer
//
static void *extend_heap(size_t words)
{
    void* bp;           /* The return pointer to the new block */
    size_t size;        /* The apporiate size of the new block */

    /* Allocate an even number of words to maintain alignment */
    size=(words%2) ? (words+1)*WSIZE:words*WSIZE;

    /* Get a memory space by system call */
    if((int)(bp=mem_sbrk(size))==NULL)  
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);       
}

//
// Practice problem 9.8
//
// find_fit - Find a fit for a block with asize bytes 
//
static void *find_fit(size_t asize)
{
    void *bp;      /* The return block pointer */

    if(header==NULL)    /* If the block list header is null ,return null */
    	return NULL;    
   
    if(j%2==1)    /* search from the beginning */
    {                             
        for(bp=header;bp!=NULL;bp=(void *)(GET(NEXT(bp))))	    
	    	if(asize<=GET_SIZE(HDRP(bp)))    	/* Find a fit one */	    
				return bp;    
    }	                          
    else         /* search from the end of the list */
    {
        if(tail==NULL)        /*If tail is null ,return null */
            return NULL;                    
        
		/* Search a fit block from the tail */
		for(bp=tail;bp!=NULL;bp=(void *)(GET(bp)))      
			if(asize<=GET_SIZE(HDRP(bp)))    	/* Find a fit one */	    
				return bp;    	              
    }                                                                  
    return NULL;         
}

// 
// mm_free - Free a block 
//
void mm_free(void *ptr)
{
    /* Get the size of the free block */
    size_t size=GET_SIZE(HDRP(ptr));

    /* Set the header/footer to free */
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);             /* coalesce the block to join with others */
}

//
// coalesce - boundary tag coalescing. Return ptr to coalesced block
//
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));   /* Whether the pre block allocated*/
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));   /* Whether the next block allocated*/
    size_t size       = GET_SIZE(HDRP(bp));    /* Get size of the current one */

    void *temp1;      /* Two temporary variables */
    void *temp2;
      
    /* Addresses in a double linked list */
    /* Case 1: Prev Block and Next Block are allocated [a][x][a], add
     * the current bp to the list and return it. */
    if(prev_alloc && next_alloc)             
    {
    	/* If the list header is null, we initialize bp to the header */
     	if(header==NULL)               
    	{
	       PUT(bp,(int)NULL);
	       PUT(NEXT(bp),(int)NULL);
	       tail=header=bp;
    	}
    	/* The normal case */
		else                         
		{
			/* We order memory in our list by where it is in physical
			 * memory, so if the bp is less than the header, we move
			 * the header back to the bp, and the next item on the list
			 * is what the header previiously pointed to */
			if(bp<header)             
			{
				PUT(bp,(int)NULL);
				PUT(NEXT(bp),(int)header);
				PUT(header,(int)bp);
				header=bp;
			}
			
			/* We know that the bp isn't the header or the footer so 
			 * we determine the placement of bp in our list by where it
			 * is in physical memory. Go forward through the 
			 * double-linked list and find where the block pointer is 
			 * in regoards to the other blocks, then insert it */
			for(temp1=header;temp1!=NULL;temp1=(void *)(GET(NEXT(temp1))))
			{
				temp2=(void *)(GET(NEXT(temp1)));
				if(temp2!=NULL)       
				{
					if(bp>temp1 &&bp<temp2) /* bp is between them */
					{
						PUT(NEXT(temp1),(int)(bp));   /* inserts bp into the list*/
						PUT(bp,(int)temp1);
						
						PUT(NEXT(bp),(int)temp2);    
						PUT(temp2,(int)bp);

						return bp;                   
					}
				}
				/* We have gone through the whole list at this point
				 * and the bp is greater than everything, so we place
				 * it at the tail of the list*/
				else
				{
					/* Making absolute certain that the bp is at the tail*/
					if(bp>temp1)        
					{
						PUT(NEXT(temp1),(int)bp);     /* add bp to the list */
						PUT(bp,(int)temp1);
						PUT(NEXT(bp),(int)NULL);
						tail=bp;                      /* set bp to be tail */
					}
				}
			}        
		}
        return bp;       /* return bp */
    }
    /* Case 2: Prev Block allocated, but Next Block free [a][x][f]
     * We know that the next block is already in the list, so we
     * combine it with the current block */
    else if(prev_alloc && !next_alloc)             
    {
    	void *nextBp=NEXT_BLKP(bp);             /* The next block */
     
        size += GET_SIZE(HDRP(nextBp));         /* Add the size to it */
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
	
        temp1=(void *)(GET(nextBp));           /* The next pointer back */
        temp2=(void *)(GET(NEXT(nextBp)));     /* The next pointer forward*/
      	
        PUT(nextBp,(int)NULL);                 /* set the next */
        PUT(NEXT(nextBp),(int)NULL);           /* set the next in the list */
	
		/* If both next blocks are NULL, our whole list is null */
		if(temp1==NULL && temp2==NULL)        
		{ 
			PUT(bp,(int)NULL);                 /* set the list*/ 
			PUT(NEXT(bp),(int)NULL);
			tail=header=bp;                    /* set the header and tail both to be bp*/
			
		}
		/* If our next-next block is null, we are at the tail */
		else if(temp1!=NULL && temp2==NULL) 
		{
			PUT(NEXT(temp1),(int)bp);          /* Set the list*/
			PUT(bp,(int)temp1);
			PUT(NEXT(bp),(int)NULL);
			tail=bp;                           /* Set bp to the tail */
		}
		/* If our next block is null, we are at the header */
		else if(temp1==NULL && temp2!=NULL)   
		{	 
			PUT(NEXT(bp),(int)temp2);          /* Set the list */
			PUT(temp2,(int)bp);
			PUT(bp,(int)NULL);
			header=bp;                         /* Set bp to the header */	    
		}
		/* If we are in the middle of our list then add bp into it */
		else                                 
		{	   
			PUT(NEXT(temp1),(int)bp);         /* Set the list */
			PUT(bp,(int)temp1);           
			PUT(temp2,(int)bp);               
			PUT(NEXT(bp),(int)temp2);	  	    
		}	      
		return bp;
	}
	/* Case  3 the previous block is free [f][x][a]
	 * All we have to do here is change the size of the previous block
	 * to accomodate for the block coelesced into it. */	
    else if(!prev_alloc && next_alloc)         
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); /* add the size of the pre to the total size*/
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));

        return PREV_BLKP(bp);                  /* return the prev block pointer */        
    }
    /* Case 4: both blocks are free [f][x][f] 
     * Here we know we can't be at the header so we just accomodate
     * for the case that we are at the tail.  Change the size of the
     * first block to accomodate for the sizes of the next two 
     * blocks then, see if we are at the tail or not.*/
    else                                      
    {       
		void *pre=PREV_BLKP(bp);               /* Set the list */
		void *next=NEXT_BLKP(bp);
		void *next1=(void *)(GET(NEXT(next)));
		PUT(next,(int)NULL);
		PUT(NEXT(next),(int)NULL);             /* add it into the list*/

		/* Get the total size with the neighbour ones */
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));

		/* Reset the header/footer with the new size */
		PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
		PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
	   
		if(next1==NULL)                /* if we are at the tail */
		{
			PUT(NEXT(pre),(int)NULL);
			tail=pre;                  /* set the tail*/
		}
		else                           /* otherwise */
		{
			PUT(NEXT(pre),(int)next1);
			PUT(next1,(int)pre);       /* add to the list */	    
		}

	return pre;                      /* return the pre block pointer */        
    }                    
}

//
// mm_malloc - Allocate a block with at least size bytes of payload 
//
void *mm_malloc(size_t size)
{
    size_t asize;               /* adjusted block size */
    size_t extendsize;          /* amount to extend heap if no fit */
    void  *bp;                  /* The return pointer */
    j++;                        /* increase j */
    
    
    if(size<=0)                 /* if size smaller than 0,just return */
        return NULL;

    if(size <= DSIZE)           /* If the size is smaller than two word */
        asize = DSIZE + OVERHEAD;
    else                        /* larger than two words */
        asize = DSIZE *((size +(OVERHEAD) + (DSIZE - 1))/DSIZE);

    if((bp = find_fit(asize)) !=NULL)  /* find the apporiate one and place it */        
        return place(bp,asize);

    extendsize = MAX(asize,CHUNKSIZE); /* doesnot find the fit one ,and extend the size */

    if((bp = extend_heap(extendsize/WSIZE)) == NULL)  /* Extending the heap failed */
        return NULL;

    return  place(bp,asize);    /* return the place pointer */
}

//
// Practice problem 9.9
//
// place - Place block of asize bytes at start of free block bp 
//         and split if remainder would be at least minimum block size
//
static void *place(void *bp,size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    
    void *pre=(void *)(GET(bp));
    void *next=(void *)(GET(NEXT(bp)));

    /* Split the block */
    if((csize - asize) >= (DSIZE + OVERHEAD))
    {
    	/* Case 1: if we have malloc'd an odd number of times we
    	 * have to make sure that the split block is added to the 
    	 * list of free blocks*/
		if(j%2==1)			 
		{
			PUT(bp,(int)NULL);
			PUT(NEXT(bp),(int)NULL);
			 
			PUT(HDRP(bp),PACK(asize,1));
			PUT(FTRP(bp),PACK(asize,1));

			void *result=bp;
			bp=NEXT_BLKP(bp);
			PUT(HDRP(bp),PACK(csize - asize ,0));
			PUT(FTRP(bp),PACK(csize - asize ,0));
		
			/* If the pre and next are both null, we need to initialize the list */
			if(pre==NULL && next==NULL)	
			{
				PUT(bp,(int)NULL);
				PUT(NEXT(bp),(int)NULL);
				tail=header=bp;
			
			}
			/* If the pre is not null and next is null we are at the tail*/
			else if(pre!=NULL && next==NULL) 
			{		
				PUT(NEXT(pre),(int)bp);   /* Set the list */
				PUT(bp,(int)pre);
				PUT(NEXT(bp),(int)NULL);
				tail=bp;			    
			}
			/* If the pre is null and next is not null we are at the header */
			else if(pre==NULL && next!=NULL) 
			{		
				PUT(bp,(int)NULL);		/* Set the list */
				PUT(NEXT(bp),(int)next);
				PUT(next,(int)bp);
				header=bp;		
			}
			/* Both are not null */
			else				 
			{		
				PUT(NEXT(pre),(int)bp);	 /* Set the list */
				PUT(bp,(int)pre);		
				PUT(NEXT(bp),(int)next);
				PUT(next,(int)bp);		
			}	    
			return result;
		}
		/* Case 2 */
		else			    
		{
			PUT(HDRP(bp),PACK(csize-asize,0));
			PUT(FTRP(bp),PACK(csize-asize,0));
			bp=NEXT_BLKP(bp);	    
			PUT(HDRP(bp),PACK(asize ,1));
			PUT(FTRP(bp),PACK(asize ,1));	                
		}                       
	}
	else		 /* The left is not big enough for a new block */
	{        
		PUT(HDRP(bp),PACK(csize,1));	/* Set the block mark*/
		PUT(FTRP(bp),PACK(csize,1));
	
		/* If the pre and next are both null we have a NULL list*/
		if(pre==NULL && next==NULL)		
		{
			tail=header=NULL;	    
		}
		/* If the pre is null and next is not null we are at the header*/
		else if(pre==NULL && next!=NULL)	
		{
			PUT(next, (int)NULL);
			header=next;	    
		}
		/* If the pre is not null and next is null we are at the footer*/
		else if(pre!=NULL && next==NULL)	
		{	    
			PUT(NEXT(pre),(int)NULL);
			tail=pre;
		}
		/* The pre and next are both not null */		
		else					
		{
			PUT(NEXT(pre),(int)next); /* Point them to each other */
			PUT(next,(int)pre);
		}
	}                      
    return bp;					/* return the block pointer */    
}

//
// mm_realloc -- implemented for you
//
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr=ptr;      /* The old pointer pointing to the old block */
    void *newptr;          /* The new pointer pointing to the new block */
    size_t oldsize;        /* The size of the old block */
    size_t newsize;        /* The size of the new block */
    void *pre=PREV_BLKP(ptr);   /* The previous block of the old one */
    void *next=NEXT_BLKP(ptr);  /* The next block of the old one */
    size_t next_size;      /* The size of the next block */
    size_t pre_size;       /* The size of the previous block */

    size_t prev_alloc=GET_ALLOC(FTRP(pre));   /* The sign of the previous block */
    size_t next_alloc=GET_ALLOC(HDRP(next));  /* The sign of the next block */

    if(size < 0)           /* If the size smaller than 0,return null */
        return NULL;

    if(ptr == NULL)        /* If the pointer is null ,malloc a new block */
       return  mm_malloc(size);     
    else if(size == 0)     /* If size is 0,free the old one */
    {
        mm_free(ptr);
        return NULL;
    }
    else                   /* The normal case */ 
    {
        oldsize = GET_SIZE(HDRP(oldptr));   /* Get the old size */
        size = ((size+ (DSIZE - 1))/DSIZE)*DSIZE;     /* align the new size */
	
        newsize =size + DSIZE;     /* add the header and footer size */

        if(oldsize>=newsize)       /* If oldsize bigger than new size ,just return */
            return oldptr;
        else
        {
            if(prev_alloc && next_alloc)  /* If the prev and next blocks are both allocated */
            {
                newptr=mm_malloc(size);   /* Malloc a new bigger block */
                memcpy(newptr,oldptr,oldsize-DSIZE);  /* copy the data to the new place */
                mm_free(oldptr);          /* free the old block */
		
                return newptr;            /* return the new pointer */
                
            }
            else if(!prev_alloc && next_alloc)  /* If prev is free and the next is allocated */
            {
                void *pre1=(void *)(GET(pre));   /* Get the previous block */        
                void *next1=(void *)(GET(NEXT(pre))); /* Get the next pointer */             
                pre_size = GET_SIZE(HDRP(pre));    /* Get the previous block size */
                
                if(pre_size+oldsize>=newsize)    /* If the sum is big enough */
                {                  
                    PUT(HDRP(pre),PACK(pre_size+oldsize,1));  /* Set the header and footer */
                    PUT(FTRP(pre),PACK(pre_size+oldsize,1));
		    
					char *temp1=(char *)pre;
					char *temp2=(char *)oldptr;
					int ii;
					for(ii=0;ii<oldsize;ii++)   /* copy the data to the new block */
					{
						*temp1=*temp2;
						 temp1++;
						 temp2++;
					}

		    		 /* If the pre in the list is null and next is null too */
                    if(pre1==NULL && next1==NULL)
                        tail=header=NULL;    
				    /* If the pre in the list is not null but the next is null */
                    else if(pre1!=NULL && next1==NULL)
                    {
                        PUT(NEXT(pre1),(int)NULL); /* Set the next of the prev */
                        tail=pre1;              /* Set the tail */
                    }
		  			/* If the pre in the list is null but the next is not null */
                    else if(pre1==NULL && next1!=NULL)
                    {
                        PUT(next1,(int)NULL);   /* Set the next */
                        header=next1;           /* Set the header */
                    }
		    		/* If the pre in the list is not null and so is the next */
                    else if(pre1!=NULL && next1!=NULL)
                    {
                        PUT(NEXT(pre1),(int)next1);  /* set the next of the pre */
                        PUT(next1,(int)pre1);        /* set the pre of the next*/
                    }
                  	return pre;  
                }
                else  /* The total size is not big enough */
                {
					newptr=mm_malloc(size);     /* malloc a new block big enough */
					memcpy(newptr,oldptr,oldsize-DSIZE);  /* copy the data */
					mm_free(oldptr);            /* free the old block */
					return newptr;              /* return the new block */
                }
            }
            else if(prev_alloc && !next_alloc) /* The prevoius is allocated but the next block is not allocated */
            {
                void *pre2=(void *)(GET(next));   /* The next of the block */
                void *next2=(void *)(GET(NEXT(next))); /*The next of the next of the block*/ 
                
                next_size=GET_SIZE(HDRP(next));   /* Get the size of the next */
		
				/* If the total size is big enough */
                if(next_size+oldsize>=newsize)
                {
		   			/* Set the block sign */
                    PUT(HDRP(oldptr),PACK(next_size+oldsize,1));
                    PUT(FTRP(oldptr),PACK(next_size+oldsize,1));
		    		/* If the pre is null and next is null too */
                    if(pre2==NULL && next2==NULL)
                        tail=header=NULL;
		    		/* If the pre in the list is not null but the next is null */
                    else if(pre2!=NULL && next2==NULL)
                    {
                        PUT(NEXT(pre2),(int)NULL);
                        tail=pre2;
                    }
		    		/* If the pre in the list is null but the next is not null */
                    else if(pre2==NULL && next2!=NULL)
                    {
                        PUT(next2,(int)NULL);
                        header=next2;
                    }
                    /* If the pre in the list is null and the next is null */
                    else if(pre2!=NULL && next2!=NULL)
                    {
                        PUT(NEXT(pre2),(int)next2);
                        PUT(next2,(int)pre2);
                    }
                    return oldptr;    /* return the old block pointer */
                }
                else
                {
					newptr=mm_malloc(size);   /* malloc a new block */
					memcpy(newptr,oldptr,oldsize-DSIZE); /* copy the data to the aim */
					mm_free(oldptr);          /* free the old block */
					return newptr;            /* return the new block pointer */
                }
            }
            else
            {
                pre_size=GET_SIZE(HDRP(pre));  /* the size of the pre block */
                next_size=GET_SIZE(HDRP(next));/* the size of the next block */
		
                void *pre1=(void *)(GET(pre)); /* the pre pointer */
                void *next2=(void *)(GET(NEXT(next)));  /* the next pointer */

                if((oldsize+pre_size)>=newsize) /* If the total size is enough */
                {
                    PUT(HDRP(pre),PACK(pre_size+oldsize,1)); /* Set the sign */
                    PUT(FTRP(pre),PACK(pre_size+oldsize,1));
		    
					char *temp1=(char *)pre;
					char *temp2=(char *)oldptr;
					int ii;
					for(ii=0;ii<oldsize;ii++)   /* copy the data to the new block */
					{
						*temp1=*temp2;
						temp1++;
						temp2++;
					}

					/* The pre in the list is null */
					if(pre1==NULL)
					 {
						 PUT(next,(int)NULL);        /* set the header */
						 header=next;
					 }
					else   /* The pre is not null */
					 {
						 PUT(NEXT(pre1),(int)next); /* set the next */
						 PUT(next,(int)pre1);
					 }
							 
					return pre;       /* return the pre pointer */
                }                  
				/* The sum of the current and next is enough */
                else if((oldsize+next_size)>=newsize)
                { 
		    		/* Set the sign */
                    PUT(HDRP(oldptr),PACK(next_size+oldsize,1));
                    PUT(FTRP(oldptr),PACK(next_size+oldsize,1));
                    if(next2==NULL)   /* If next is null */
                    {
                        PUT(NEXT(pre),(int)NULL); /* set the next */
                        tail=pre;                 /* set the tail */
                    }else
                    {
                        PUT(NEXT(pre),(int)next2); /* set the next of the pre */
                        PUT(next2,(int)pre);
                    }
                    
                    return oldptr;   /* return the old block pointer */
                }
                
                /* The total size of the three blocks are big enough */
                else  if(pre_size+oldsize+next_size>=newsize)
                {
		    		/* Set the sign */
                    PUT(HDRP(pre),PACK(pre_size+oldsize+next_size,1));
                    PUT(FTRP(pre),PACK(pre_size+oldsize+next_size,1));

					char *temp1=(char *)pre;
					char *temp2=(char *)oldptr;
					int ii;
					
					for(ii=0;ii<oldsize;ii++)   /* Copy the data to the new block */
					{
						*temp1=*temp2;
						temp1++;
						temp2++;
					}
		    
		    /* The pre in the list is null and the next is null too */
                    if(pre1==NULL && next2==NULL)    
                        tail=header=NULL; 
		    /* The pre in the list is null and the next is not null */
                    else if(pre1==NULL && next2!=NULL)
                    {
                        PUT(next2,(int)NULL);   /* Set the next of the header */
                        header=next2;           /* Set the header */
                    }
		    /* The pre in the list is not null and the next is null */
                    else if(pre1!=NULL && next2==NULL)
                    {
                        PUT(NEXT(pre1),(int)NULL); /* set the next*/
                        tail=pre1;                 /* set the tail */
                    }
		    /* The pre in the list is not null and the next is not null too */
                    else
                    {
                        PUT(NEXT(pre1),(int)next2);  /* set the next of the pre */
                        PUT(next2,(int)pre1);        /* set the pre of the next */
                    }
                    return pre;                      /* return the pre */
                }
                else      /* All the total size is not big enough */
                { 
					newptr=mm_malloc(size);   /* malloc a new block */
					memcpy(newptr,oldptr,oldsize-DSIZE); /* copy the data */
					mm_free(oldptr);          /* free the old block */
					return newptr;            /* return the new block pointer */
                }   
            }   
        }   
    }
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

  printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp, 
	 hsize, (halloc ? 'a' : 'f'), 
	 fsize, (falloc ? 'a' : 'f')); 
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
