/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic Constant and macros */
#define WSIZE       4           // 1워드 크기       = 4바이트
#define DSIZE       8           // 더블 워드 크기    = 8바이트
#define CHUNKSIZE   (1 << 12)   // mem_sbrk로 힙을 늘릴 용 (4096 = 한 번에 4KB씩)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated a bit into a word */
#define PACK(size, alloc) ((size) | (alloc))            // 블록의 크기, 할당 정보를 1워드로 합쳐준다.

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))          // 주소 p에 있는 1워드 값 읽기
#define PUT(p, val)     (*(unsigned int *)(p) = (val))  // 주소 p에 있는 1워드 값에 val을 대입한다.

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)                 // 헤더(주소 p)에서 크기 정보만 얻는다.
#define GET_ALLOC(p)    (GET(p) & 0x1)                  // 헤더(주소 p)에서 할당 여부만 얻는다.

/* Given block ptr bp, compute the address of its header and footer */
// bp = 페이로드의 시작점을 가리킨다.
#define HDRP(bp)        ((char *)(bp) - WSIZE)                          // bp 블록의 헤더 주소 계산
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)     // bp 블록의 푸터 주소 계산

/* Given block ptr bp, compute address of next and previous blocks */
// 현재 블록의 다음 블록 포인터 계산 :
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
// 현재 블록의 이전 블록 포인터 계산
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Global variables */
static char *heap_listp;

/*
 * coalesce :
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {             /* Case 1 */
        return bp;
    } else if (prev_alloc && !next_alloc) {     /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {     /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {                                    /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * extend_heap : 새 가용 블록으로 힙 확장하기
 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    /* Initialize a free block header / footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // 초기 설정을 위해 4워드(=16바이트) 크기의 메모리 공간을 확보해달라고 요청
    // mem_sbrk 성공 : heap_listp에 16바이트 공간의 시작 주소가 저장된다.
    // mem_sbrk 실패 : -1을 반환한다. (약속된 에러 값)
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);                                /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));       /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));       /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));           /* Epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    return 0;
}


static void *find_fit(size_t asize) {
    /* First-Fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // ----- original code -----
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize;
    size_t extendsize;
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE) {
        asize = 2*DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}


/*
 * mm_realloc - 메모리 블록의 크기를 재조정하는 함수
 * 
 * @param ptr: 크기를 조절할 메모리 블록의 포인터
 * @param size: 재할당할 새로운 크기 (바이트 단위)
 * @return: 재할당된 메모리 블록의 포인터
 * 
 * - ptr이 NULL인 경우: mm_malloc(size)와 동일하게 동작
 * - size가 0인 경우: mm_free(ptr)와 동일하게 동작하고 NULL 반환
 * - 그 외의 경우:
 *   1) 기존 메모리 블록의 크기를 새로운 size로 조절
 *   2) 새 크기가 기존보다 작다면 데이터 일부가 잘릴 수 있음
 *   3) 새 크기가 기존보다 크다면 추가 공간의 초기값은 정의되지 않음
 *   4) 재할당 후 기존 데이터는 새 위치로 복사됨
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    void *oldptr = ptr;           // 이전 집 주소
    void *newptr;                 // 이사갈 새 집 주소 (아직 없음)
    size_t old_size, copy_size;   // 옮길 이삿짐 크기

    newptr = mm_malloc(size); // 새로 이사갈 집 구하기
    if (newptr == NULL) {
        return NULL;          // 새 집을 못 구하면 이사 실패
    }

    // 이삿짐 크기 확인하기
    old_size = GET_SIZE(HDRP(oldptr));
    copy_size = old_size - DSIZE;       // Payload Size

    // 짐을 얼마나 옮길지 결정하기
    // 새 집이 더 작으면 짐을 다 못 넣으니까 새 집 크기만큼만 옮기기
    if (size < copy_size) {
        copy_size = size;
    }

    // 실제로 이사하기
    memcpy(newptr, oldptr, copy_size);

    // 이전 집은 계약 해지하기
    mm_free(oldptr);

    // 새 집 주소 알려주기
    return newptr;
}