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

#define PROLOGUE_BLOCK_SIZE DSIZE      // 프롤로그 블록 크기 (8바이트)
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

// 가용 리스트의 시작점을 가리키는 포인터
static char *free_listp;

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
 * extend_heap - 힙을 확장하여 새로운 가용 블록을 생성하는 함수
 * 
 * @param words: 확장할 크기 (워드 단위)
 * @return:      새로 생성된 가용 블록의 포인터, 실패 시 NULL
 * 
 * 동작 방식:
 * 1. 더블 워드 정렬 요구사항을 만족하도록 크기를 조정
 * 2. mem_sbrk를 호출하여 힙을 확장
 * 3. 새 공간을 가용 블록으로 초기화 (헤더, 푸터 설정)
 * 4. 새 에필로그 블록을 배치
 * 5. 이전 블록과의 통합 가능성 확인
 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // 8바이트 정렬을 위해, 요청된 워드 개수를 짝수로 맞춰준다.
    size_t num_words = words;
    if (num_words % 2 != 0) {
        num_words++;
    }
    size = num_words * WSIZE;

    // OS로부터 size만큼의 메모리를 추가로 받아오기
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // 새로 받은 공간을 하나의 큰 가용 블록으로 만들기
    PUT(HDRP(bp), PACK(size, FREE));                /* 가용 블록의 헤더 */
    PUT(FTRP(bp), PACK(size, FREE));                /* 가용 블록의 푸터 */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, ALLOCATED));   /* 새 에필로그 헤더 설치 */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}



/*
 * mm_init - 메모리 시스템을 초기화한다.
 *           초기 힙 영역을 생성하고 프롤로그와 에필로그 블록을 설정한다.
 * 
 * 반환값: 성공하면 0, 실패하면 -1
 */
int mm_init(void) {
    // 초기 힙 공간 4워드(16바이트)를 OS로부터 할당받는다.
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1) {
        return -1;
    }
    PUT(heap_listp, 0);                                                   /* 정렬을 위한 패딩 워드 */
    PUT(heap_listp + (1 * WSIZE), PACK(PROLOGUE_BLOCK_SIZE, ALLOCATED));  /* 프롤로그 블록의 헤더 */
    PUT(heap_listp + (2 * WSIZE), PACK(PROLOGUE_BLOCK_SIZE, ALLOCATED));  /* 프롤로그 블록의 푸터 */
    PUT(HDRP(NEXT_BLKP(heap_listp)), PACK(0, ALLOCATED));                 /* 에필로그 블록의 헤더 */
    heap_listp += (2 * WSIZE);                                            /* 프롤로그 블록의 payload를 가리키도록 포인터 이동 */

    // CHUNKSIZE만큼 힙을 확장하여 초기 가용 블록을 생성한다.
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    return 0;
}



/*
 * find_fit - 요청된 크기를 수용할 수 있는 가용 블록을 찾는 함수
 * 
 * @param required_size: 할당하고자 하는 메모리 크기 (헤더와 푸터 포함)
 * @return:              적합한 가용 블록의 포인터, 없으면 NULL
 * 
 * First-fit 방식을 사용:
 * 1. 힙의 처음부터 순차적으로 블록들을 검색
 * 2. 가용 상태이고 요청 크기보다 큰 첫 번째 블록을 찾으면 반환
 * 3. 적합한 블록을 찾지 못하면 NULL 반환
 */
static void *find_fit(size_t required_size) {
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (required_size <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}


/*
 * place - 가용 블록에 요청된 크기의 메모리를 할당하는 함수
 * 
 * @param bp:            할당할 가용 블록의 포인터
 * @param required_size: 할당하고자 하는 메모리 크기 (헤더와 푸터 포함)
 * 
 * 동작 방식:
 * 1. 가용 블록의 크기가 요청 크기보다 충분히 크면 (최소 블록 크기 이상 차이)
 *    - 블록을 분할하여 앞부분은 할당하고 뒷부분은 가용 블록으로 분리
 * 2. 가용 블록의 크기가 요청 크기와 비슷하면
 *    - 블록 전체를 할당 상태로 변경
 */
static void place(void *bp, size_t required_size) {
    size_t free_size = GET_SIZE(HDRP(bp));
    size_t remainder_size = free_size - required_size;

    void *prev_free_bp = (void *)GET(bp);           // 이전 가용 블록의 주소 읽기
    void *next_free_bp = (void *)GET(bp + WSIZE);   // 다음 가용 블록의 주소 읽기

    // edge case 1: 제거된 블록이 가용 리스트의 첫 번째 블록이었다면?
    if (prev_free_bp == NULL && next_free_bp != NULL) {
        free_listp = next_free_bp;

        // 두 번째 블록의 PRED 를 NULL로 해야 한다.
        PUT(next_free_bp, 0);
    }
    // edge case 2: 제거된 블록이 가용 리스트의 가장 마지막 블록이었다면?
    else if (prev_free_bp != NULL && next_free_bp == NULL) {
        // 삭제 블록의 '이전 블록'의(=끝에서 두 번째 블록) SUCC을 NULL로 해야 한다.
        PUT(prev_free_bp + WSIZE, 0);
    }
    // edge case 3: 제거된 블록이 가용 리스트의 유일한 블록이었다면?
    else if (prev_free_bp == NULL && next_free_bp == NULL) {
        free_listp = 0;   // 가용 리스트의 시작 주소를 초기화한다.
    }
    else {
        PUT(prev_free_bp + WSIZE, next_free_bp);  // 앞 블록의 SUCC가 '뒷 블록'을 가리키도록 수정
        PUT(next_free_bp, prev_free_bp);          // 뒷 블록의 PRED가 '앞 블록'을 가리키도록 수정
    }

    if (remainder_size >= MINIMUM_BLOCK_SIZE) {  // 남는 공간이 분할할 만큼 크다면 분할
        // 먼저 요청한만큼 메모리 할당하기
        PUT(HDRP(bp), PACK(required_size, ALLOCATED)); // 헤더에 크기, 할당됨(1) 기록
        PUT(FTRP(bp), PACK(required_size, ALLOCATED)); // 푸터에도 기록

        // 남는 부분을 새로운 가용 블록으로 만들기
        bp = NEXT_BLKP(bp);                           // 남는 공간의 시작점으로 이동
        PUT(HDRP(bp), PACK(remainder_size, FREE));    // 남는 공간의 헤더 설정
        PUT(FTRP(bp), PACK(remainder_size, FREE));    // 남는 공간의 푸터 설정
        coalesce(bp);                                 // 새로 생긴 가용 블록을 인접 블록과 병합

        // 새로운 가용 블록이 가용 리스트의 가장 첫 번째 블록이 되어야 한다.
        PUT(bp, 0);                   // 새 가용 블록의 PRED -> NULL이다. (가용 리스트의 첫 번째 블록이니까.)
        PUT(bp + WSIZE, free_listp);  // 새 가용 블록의 SUCC -> 기존 가용 리스트의 첫 번째 블록을 가리키던 free_listp이다.

        // 기존 가용 블록의 첫 번째 블록은 free_listp가 가리키고 있다.
        // edge case: 만약 가용 리스트가 비어 있는 상태였다면?
        if (free_listp != NULL) {
            PUT(free_listp, bp);     // Old Head의 PRED는 새 가용 블록을 가리키게 한다.
        }
        free_listp = bp;             // 가용 리스트의 새로운 HEAD를 bp로 설정하기

    } else {
        PUT(HDRP(bp), PACK(free_size, ALLOCATED));    // 헤더에 원래 크기, 할당됨 상태 기록
        PUT(FTRP(bp), PACK(free_size, ALLOCATED));    // 푸터에도 동일하게 기록
    }
}


/*
 * mm_malloc - 요청된 크기의 메모리를 할당하는 함수
 * 
 * @param size: 할당하고자 하는 메모리의 크기 (바이트 단위)
 * @return:     할당된 메모리 블록의 포인터, 실패 시 NULL
 * 
 * 동작 방식:
 * 1. 요청 크기가 0이면 NULL 반환
 * 2. 실제 필요한 블록 크기 계산 (헤더/푸터, 정렬 요구사항 포함)
 * 3. 가용 리스트에서 적합한 블록 검색
 * 4. 적합한 블록이 없으면 힙을 확장하여 새 블록 할당
 */
void *mm_malloc(size_t size) {
    size_t required_size;       // 실제 할당할 블록의 크기 (헤더/푸터, 정렬 포함)
    size_t extension_size;      // 적합한 블록이 없을 때 힙을 확장할 크기
    char *bp;

    /* 크기가 0인 요청은 무시 */
    if (size == 0) {
        return NULL;
    }

    /* 오버헤드와 정렬 요구사항을 포함한 블록 크기 조정 */
    if (size <= DSIZE) {
        required_size = MINIMUM_BLOCK_SIZE;
    } else {
        required_size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* 가용 리스트에서 적합한 블록 검색 */
    if ((bp = find_fit(required_size)) != NULL) {
        place(bp, required_size);
        return bp;
    }

    /* 적합한 블록을 찾지 못했으므로, 힙을 확장하고 블록 배치 */
    extension_size = MAX(required_size, CHUNKSIZE);
    if ((bp = extend_heap(extension_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, required_size);
    return bp;
}


/*
 * mm_free - 할당된 메모리 블록을 해제하는 함수
 * 
 * @param bp: 해제할 메모리 블록의 포인터
 * 
 * 동작 방식:
 * 1. 해제할 블록의 크기 정보를 헤더에서 읽어옴
 * 2. 블록의 헤더와 푸터를 가용 상태로 변경
 * 3. 인접한 가용 블록들과 병합 (coalesce 함수 호출)
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