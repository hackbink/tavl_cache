#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>
#include "tavl.h"

//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------
segment_t segment[NUM_OF_SEGMENTS];
tavl_node_t node[NUM_OF_SEGMENTS];
cManagement_t cacheMgmt;

void main(void) {
    time_t t;
    unsigned i;
    segment_t *tSeg, *cSeg, *nextSeg;
    tavl_node_t *cNode;
    unsigned currentLba, currentNB;

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
    initCache();
    // Insert NUM_OF_SEGMENTS segments into the TAVL tree.
    for (i = 0; i < NUM_OF_SEGMENTS; i++) {
        tSeg=popFromHead(&cacheMgmt.free);
        tSeg->key = rand() % 2000;
        tSeg->numberOfBlocks = 10+(rand()%20);
        cNode=searchTavl(cacheMgmt.root, tSeg->key);
        if (NULL!=cNode) {
            if (&cacheMgmt.lowest!=cNode->pSeg) {
                printf("searchTavl(%d) returned cNode:%p with LBA range [%d..%d]\n", tSeg->key, cNode, cNode->pSeg->key, (cNode->pSeg->key+cNode->pSeg->numberOfBlocks));
                if ((cNode->pSeg->key+cNode->pSeg->numberOfBlocks)>tSeg->key) {
                    printf("%dth LBA range [%d..%d] hits with [%d..%d]. Invalidating...\n", i, tSeg->key, (tSeg->key+tSeg->numberOfBlocks), cNode->pSeg->key, (cNode->pSeg->key+cNode->pSeg->numberOfBlocks));
                    // Invalidate the existing cache segment before inserting the new one.
                    cacheMgmt.root=freeNode(cacheMgmt.root, cNode->pSeg);
                }
            }
        }
        printf("%dth LBA range [%d..%d] will be inserted\n", i, tSeg->key, (tSeg->key+tSeg->numberOfBlocks));
        cacheMgmt.root = insertToTavl(cacheMgmt.root, (tavl_node_t *)(tSeg->pNode));
        pushToTail(tSeg, &cacheMgmt.lru);

        // Manage coherency by invalidating any segment that overlaps with the new one.
        cSeg=tSeg->higher;
        while (&cacheMgmt.highest!=cSeg) {
            // Check if cNode is outside of the new one's range. If yes, stop.
            if (cSeg->key>=(tSeg->key+tSeg->numberOfBlocks)) {
                break;
            }
            // Invalidate this segment as it overlapped.
            printf("Invalidating LBA range %p [%d..%d] as it overlaps with new one - [%d..%d]\n", cSeg, cSeg->key, (cSeg->key+cSeg->numberOfBlocks), tSeg->key, (tSeg->key+tSeg->numberOfBlocks));
            cacheMgmt.root=freeNode(cacheMgmt.root, cSeg);
            cSeg=tSeg->higher;
        }
    }
    // Scan the Thread and make sure all segments are ordered
    // Fetch the first segment in the Thread, one that is pointed by cacheMgmt.lowest.higher.
    tSeg=cacheMgmt.lowest.higher;
    currentLba=0;
    currentNB=0;
    i=0;
    while (tSeg!=&cacheMgmt.highest) {
        // Make sure this segment has an LBA that is equal or bigger than previous LBA + number of blocks
        assert(tSeg->key>=currentLba+currentNB);
        currentLba=tSeg->key;
        (void)dumpPathToKey(cacheMgmt.root, currentLba);
        i++;
        currentNB=tSeg->numberOfBlocks;
        tSeg=tSeg->higher;
    }

    // Traverse the Thread and remove each & every node from TAVL and the list. Node gets returned to free pool.
    // Fetch the first segment in the Thread, one that is pointed by cacheMgmt.lowest.higher.
    printf("Removing all nodes in the Thread\n");
    tSeg=cacheMgmt.lowest.higher;
    while (tSeg!=&cacheMgmt.highest) {
        // Remove this node
        nextSeg=tSeg->higher;
        cacheMgmt.root=freeNode(cacheMgmt.root, tSeg);
        tSeg=nextSeg;
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
    assert(NULL==cacheMgmt.root);
    printf("Checking the thread is empty\n");
    assert(cacheMgmt.lowest.higher==&cacheMgmt.highest);
    assert(cacheMgmt.highest.lower==&cacheMgmt.lowest);
    printf("Checking the LRU is empty\n");
    assert(cacheMgmt.lru.head.next==&cacheMgmt.lru.tail);
    assert(cacheMgmt.lru.tail.prev==&cacheMgmt.lru.head);
    printf("Test successful\n");
}
