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

#define MINIMUM_BLOCK_SIZE (2 * DSIZE) // 최소 블록 크기 (16바이트)

#define ALLOCATED 1 // 할당된 상태
#define FREE      0 // 가용 상태

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

    if (prev_alloc == ALLOCATED && next_alloc == ALLOCATED) {       /* Case 1 */
        return bp;
    } else if (prev_alloc == ALLOCATED && next_alloc == FREE) {     /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, FREE));
        PUT(FTRP(bp), PACK(size, FREE));
    } else if (prev_alloc == FREE && next_alloc == ALLOCATED) {     /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, FREE));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, FREE));
        bp = PREV_BLKP(bp);
    } else {                                                        /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, FREE));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, FREE));
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
    PUT(HDRP(bp), PACK(size, FREE));           /* Free block header */
    PUT(FTRP(bp), PACK(size, FREE));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, ALLOCATED));   /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);                                        /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, ALLOCATED));       /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, ALLOCATED));       /* Prologue footer */
    PUT(HDRP(NEXT_BLKP(heap_listp)), PACK(0, ALLOCATED));      /* Epilogue header */
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    return 0;
}


static void *find_fit(size_t required_size) {
    /* First-Fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (required_size <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}

static void place(void *bp, size_t required_size) {
    size_t free_size = GET_SIZE(HDRP(bp));
    size_t remainder_size = free_size - required_size;

    if (remainder_size >= MINIMUM_BLOCK_SIZE) {  // 남는 공간이 분할할 만큼 크다면 분할
        // 먼저 요청한만큼 메모리 할당하기
        PUT(HDRP(bp), PACK(required_size, ALLOCATED)); // 헤더에 크기, 할당됨(1) 기록
        PUT(FTRP(bp), PACK(required_size, ALLOCATED)); // 푸터에도 기록

        // 남는 부분을 새로운 가용 블록으로 만들기
        bp = NEXT_BLKP(bp); // 남는 공간의 시작점으로 이동
        PUT(HDRP(bp), PACK(remainder_size, FREE));    // 남는 공간의 헤더 설정
        PUT(FTRP(bp), PACK(remainder_size, FREE));    // 남는 공간의 푸터 설정
        coalesce(bp); // 새로 생긴 가용 블록을 인접 블록과 병합
    } else {
        PUT(HDRP(bp), PACK(free_size, ALLOCATED));    // 헤더에 원래 크기, 할당됨 상태 기록
        PUT(FTRP(bp), PACK(free_size, ALLOCATED));    // 푸터에도 동일하게 기록
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t required_size;      // 실제 할당할 블록의 크기 (헤더/푸터, 정렬 포함)
    size_t extension_size;     // 적합한 블록이 없을 때 힙을 확장할 크기
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE) {
        required_size = MINIMUM_BLOCK_SIZE;
    } else {
        required_size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(required_size)) != NULL) {
        place(bp, required_size);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extension_size = MAX(required_size, CHUNKSIZE);
    if ((bp = extend_heap(extension_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, required_size);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, FREE));
    PUT(FTRP(bp), PACK(size, FREE));
    coalesce(bp);
}


/*
 * mm_realloc - 메모리 블록의 크기를 재조정하는 함수
 * 
 * @param ptr: 크기를 조절할 메모리 블록의 포인터
 * @param size: 재할당할 새로운 크기 (바이트 단위)
 * @return: 재할당된 메모리 블록의 포인터
 */
void *mm_realloc(void *ptr, size_t size)
{
    // 엣지 케이스 1: ptr이 NULL이면 malloc(size)와 동일
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // 엣지 케이스 2: size가 0이면 free(ptr)와 동일
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t required_size; // 정렬 및 오버헤드를 포함한 새 블록 크기

    // mm_malloc과 동일한 로직으로 필요한 블록 크기(asize)를 계산 (★★★★★ 핵심 수정 사항)
    if (size <= DSIZE) {
        required_size = MINIMUM_BLOCK_SIZE;
    } else {
        required_size = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    }

    // [최적화 1] 요청한 크기가 기존 블록보다 작거나 같은 경우
    if (required_size <= old_size) {
        // 남는 공간이 분할할 만큼 크다면 분할
        if ((old_size - required_size) >= MINIMUM_BLOCK_SIZE) {
            PUT(HDRP(ptr), PACK(required_size, ALLOCATED));
            PUT(FTRP(ptr), PACK(required_size, ALLOCATED));
            void *remainder_bp = NEXT_BLKP(ptr);
            PUT(HDRP(remainder_bp), PACK(old_size - required_size, FREE));
            PUT(FTRP(remainder_bp), PACK(old_size - required_size, FREE));
            coalesce(remainder_bp); // 분할된 가용 블록을 주변과 병합
        }
        // 분할할 수 없으면 그냥 기존 블록 그대로 사용
        return ptr;
    }
    // [최적화 2] 요청한 크기가 더 큰 경우, 다음 블록을 확인
    else {
        void *next_bp = NEXT_BLKP(ptr);
        size_t next_alloc = GET_ALLOC(HDRP(next_bp));
        size_t next_size = GET_SIZE(HDRP(next_bp));
        size_t total_size = old_size + next_size;

        // 다음 블록이 가용 상태이고, 합친 크기가 충분한 경우
        if (next_alloc == FREE && total_size >= required_size) {
            PUT(HDRP(ptr), PACK(total_size, ALLOCATED));
            PUT(FTRP(ptr), PACK(total_size, ALLOCATED));
            return ptr;
        }
        // [최후의 수단] 새로 할당받고 데이터 복사
        void *newptr = mm_malloc(size);
        if (newptr == NULL) return NULL;
        memcpy(newptr, ptr, old_size - DSIZE); // 기존 payload 만큼만 복사
        mm_free(ptr);
        return newptr;
    }
}