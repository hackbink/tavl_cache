#include <stdio.h>
#ifdef __linux__
#include <execinfo.h>
#include <signal.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>
#include "tavl.h"

//-----------------------------------------------------------
// Macros
//-----------------------------------------------------------
#define NUM_OF_SEGMENTS (100)
#define TEST_LOOP       (1000000)

//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------


//-----------------------------------------------------------
// Functions
//-----------------------------------------------------------
#ifdef __linux__
void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d\n", sig);
  backtrace_symbols_fd(array, size, 2);
  exit(1);
}
#endif

void main(void) {
    time_t t;
    unsigned i;
    segment_t *tSeg, *cSeg, *nextSeg;
    tavl_node_t *cNode,*higherNode, *nextNode;
    unsigned currentLba, currentNB;

#ifdef __linux__
	signal(SIGSEGV, handler);
#endif

    // srand(time(NULL)) initializes the random seed with the current time.
    srand((unsigned)time(&t));

    // Test TAVL tree insertion and removal operation, with coherency management.
    // - Initialize the cache
    // - Get a segment from free pool
    // - Insert all NUM_OF_SEGMENTS segments into the tree, each with random key (0..1999) and number of block of (10..29)
    // - Invalidate(free) any segments that overlap with the current range
    // - Since the segment got inserted to TAVL tree, insert it to LRU too.
    // - Scan the Thread and make sure all segments are ordered and there is no overlap
    // - Traverse the Thread and remove each & every segment from TAVL and the list. Segment gets returned to free pool.
    // - Confirm that AVL tree, Thread and LRU are empty

    printf("Testing TAVL tree insertion and removal operation, with coherency management\n");
    initCache(NUM_OF_SEGMENTS);
    // Insert NUM_OF_SEGMENTS segments into the TAVL tree.
    for (i = 0; i < NUM_OF_SEGMENTS; i++) {
        tSeg=popFromHead(&cacheMgmt.free);
        tSeg->key = rand() % 20000;
        tSeg->numberOfBlocks = 10+(rand()%20);
        cNode=searchTavl(cacheMgmt.tavl.root, tSeg->key);
        if (NULL!=cNode) {
            if (&cacheMgmt.tavl.lowest!=cNode) {
                // printf("searchTavl(%d) returned cNode:%p with LBA range [%d..%d]\n", tSeg->key, cNode, cNode->pSeg->key, (cNode->pSeg->key+cNode->pSeg->numberOfBlocks));
                if ((cNode->pSeg->key+cNode->pSeg->numberOfBlocks)>tSeg->key) {
                    printf("%dth LBA range [%d..%d] hits with [%d..%d]. Invalidating...\n", i, tSeg->key, (tSeg->key+tSeg->numberOfBlocks), cNode->pSeg->key, (cNode->pSeg->key+cNode->pSeg->numberOfBlocks));
                    // Invalidate the existing cache segment before inserting the new one.
                    freeNode(cNode->pSeg);
                }
            }
        }
        printf("%dth LBA range [%d..%d] will be inserted\n", i, tSeg->key, (tSeg->key+tSeg->numberOfBlocks));
        cacheMgmt.tavl.root = insertToTavl(&cacheMgmt.tavl, (tavl_node_t *)(tSeg->pNode));
        pushToTail(tSeg, &cacheMgmt.lru);

        // Manage coherency by invalidating any segment that overlaps with the new one.
        higherNode=((tavl_node_t *)(tSeg->pNode))->higher;
        cSeg=higherNode->pSeg;
        while (&cacheMgmt.tavl.highest!=higherNode) {
            // Check if cNode is outside of the new one's range. If yes, stop.
            if (cSeg->key>=(tSeg->key+tSeg->numberOfBlocks)) {
                break;
            }
            // Invalidate this segment as it overlapped.
            printf("Invalidating LBA range %p [%d..%d] as it overlaps with new one - [%d..%d]\n", cSeg, cSeg->key, (cSeg->key+cSeg->numberOfBlocks), tSeg->key, (tSeg->key+tSeg->numberOfBlocks));
            freeNode(cSeg);
            higherNode=((tavl_node_t *)(tSeg->pNode))->higher;
            cSeg=higherNode->pSeg;
        }
    }

#if 1
    // Check the sanity of the TAVL tree
    tavlSanityCheck(&cacheMgmt.tavl);
    assert(tavlHeightCheck(cacheMgmt.tavl.root));
#else
    // Scan the Thread and make sure all segments are ordered
    // Fetch the first segment in the Thread, one that is pointed by cacheMgmt.tavl.lowest.higher.
    cNode=cacheMgmt.tavl.lowest.higher;
    currentLba=0;
    currentNB=0;
    i=0;
    while (cNode!=&cacheMgmt.tavl.highest) {
        // Make sure this segment has an LBA that is equal or bigger than previous LBA + number of blocks
        assert(cNode->pSeg->key>=currentLba+currentNB);
        currentLba=cNode->pSeg->key;
        (void)dumpPathToKey(cacheMgmt.tavl.root, currentLba);
        i++;
        currentNB=cNode->pSeg->numberOfBlocks;
        cNode=cNode->higher;
    }
#endif

    // Random delete and add loop
    for (i = 0; i < TEST_LOOP; i++) {
        // Remove a random node from the TAVL tree, but only if there is none left in the free pool.
        while (NULL==(tSeg=popFromHead(&cacheMgmt.free))) {
            do {
                currentLba=(rand() % 20000);
                cNode=searchTavl(cacheMgmt.tavl.root, currentLba);
            } while (NULL==cNode);

            if (&cacheMgmt.tavl.lowest==cNode) {
                cNode=cNode->higher;
            }
            cSeg=cNode->pSeg;
            printf("A randomly picked node with LBA range [%d..%d] will be removed\n", cSeg->key, (cSeg->key+cSeg->numberOfBlocks));
            freeNode(cSeg);
        }

        initSegment(tSeg);
	    initNode(tSeg->pNode);
        tSeg->key = rand() % 20000;
        tSeg->numberOfBlocks = 10+(rand()%20);
        cNode=searchTavl(cacheMgmt.tavl.root, tSeg->key);
        if (NULL!=cNode) {
            if (&cacheMgmt.tavl.lowest!=cNode) {
                // printf("searchTavl(%d) returned cNode:%p with LBA range [%d..%d]\n", tSeg->key, cNode, cNode->pSeg->key, (cNode->pSeg->key+cNode->pSeg->numberOfBlocks));
                if ((cNode->pSeg->key+cNode->pSeg->numberOfBlocks)>tSeg->key) {
                    printf("%dth LBA range [%d..%d] hits with [%d..%d]. Invalidating...\n", i, tSeg->key, (tSeg->key+tSeg->numberOfBlocks), cNode->pSeg->key, (cNode->pSeg->key+cNode->pSeg->numberOfBlocks));
                    // Invalidate the existing cache segment before inserting the new one.
                    freeNode(cNode->pSeg);
                }
            }
        }

        cacheMgmt.tavl.root = insertToTavl(&cacheMgmt.tavl, (tavl_node_t *)(tSeg->pNode));
        pushToTail(tSeg, &cacheMgmt.lru);
        printf("%dth LBA range [%d..%d] has been inserted\n", i, tSeg->key, (tSeg->key+tSeg->numberOfBlocks));

        // Manage coherency by invalidating any segment that overlaps with the new one.
        higherNode=((tavl_node_t *)(tSeg->pNode))->higher;
        cSeg=higherNode->pSeg;
        while (&cacheMgmt.tavl.highest!=higherNode) {
            // Check if cNode is outside of the new one's range. If yes, stop.
            if (cSeg->key>=(tSeg->key+tSeg->numberOfBlocks)) {
                break;
            }
            // Invalidate this segment as it overlapped.
            printf("Invalidating LBA range %p [%d..%d] as it overlaps with new one - [%d..%d]\n", cSeg, cSeg->key, (cSeg->key+cSeg->numberOfBlocks), tSeg->key, (tSeg->key+tSeg->numberOfBlocks));
            freeNode(cSeg);
            higherNode=((tavl_node_t *)(tSeg->pNode))->higher;
            cSeg=higherNode->pSeg;
        }
        printf("Random delete/insert test %dth completed. active_nodes:%d\n", i, cacheMgmt.tavl.active_nodes);
    }

    // Check the sanity of the TAVL tree
    tavlSanityCheck(&cacheMgmt.tavl);
    assert(tavlHeightCheck(cacheMgmt.tavl.root));

    // Traverse the Thread and remove each & every node from TAVL and the list. Node gets returned to free pool.
    // Fetch the first segment in the Thread, one that is pointed by cacheMgmt.tavl.lowest.higher.
    printf("Removing all nodes in the Thread\n");
    cNode=cacheMgmt.tavl.lowest.higher;
    while (cNode!=&cacheMgmt.tavl.highest) {
        // Remove this node
        nextNode=cNode->higher;
        freeNode(cNode->pSeg);
        cNode=nextNode;
    }

    // Traverse the LRU and dump any remaining segments.
    printf("Dumping any segments in LRU, there should be none left\n");
    tSeg=cacheMgmt.lru.head.next;
    i=0;
    while (tSeg!=&cacheMgmt.lru.tail) {
        printf("%dth seg %p in the LRU, LBA range [%d..%d]\n", i, tSeg, tSeg->key, tSeg->key+tSeg->numberOfBlocks);
        // Remove this node
        tSeg=tSeg->next;
        i++;
    }

    // Confirm that AVL tree, Thread and LRU are empty
    printf("Checking the tree is empty\n");
    assert(NULL==cacheMgmt.tavl.root);
    printf("Checking the thread is empty\n");
    assert(cacheMgmt.tavl.lowest.higher==&cacheMgmt.tavl.highest);
    assert(cacheMgmt.tavl.highest.lower==&cacheMgmt.tavl.lowest);
    printf("Checking the LRU is empty\n");
    assert(cacheMgmt.lru.head.next==&cacheMgmt.lru.tail);
    assert(cacheMgmt.lru.tail.prev==&cacheMgmt.lru.head);
    printf("Test successful\n");
}
