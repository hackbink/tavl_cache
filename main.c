#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>
#include "tavl.h"

//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------
segment_t node[NUM_OF_SEGMENTS];
cManagement_t cacheMgmt;

void main(void) {
    time_t t;
    unsigned i;
    segment_t *tNode, *nextNode, *cNode;
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
        tNode=popFromHead(&cacheMgmt.free);
        tNode->key = rand() % 2000;
        tNode->numberOfBlocks = 10+(rand()%20);
        cNode=searchTavl(cacheMgmt.root, tNode->key);
        if ((NULL!=cNode)&&(&cacheMgmt.lowest!=cNode)) {
            printf("searchTavl(%d) returned cNode:%p with LBA range [%d..%d]\n", tNode->key, cNode, cNode->key, (cNode->key+cNode->numberOfBlocks));
            if ((cNode->key+cNode->numberOfBlocks)>tNode->key) {
                printf("%dth LBA range [%d..%d] hits with [%d..%d]. Invalidating...\n", i, tNode->key, (tNode->key+tNode->numberOfBlocks), cNode->key, (cNode->key+cNode->numberOfBlocks));
                // Invalidate the existing cache segment before inserting the new one.
                segment_t *cNodeCopy=cNode;
                cacheMgmt.root=freeNode(cacheMgmt.root, cNode);
            }
        }
        printf("%dth LBA range [%d..%d] will be inserted\n", i, tNode->key, (tNode->key+tNode->numberOfBlocks));
        cacheMgmt.root = insertToTavl(cacheMgmt.root, tNode);
        pushToTail(tNode, &cacheMgmt.lru);

        // Manage coherency by invalidating any segment that overlaps with the new one.
        cNode=tNode->higher;
        while (&cacheMgmt.highest!=cNode) {
            // Check if cNode is outside of the new one's range. If yes, stop.
            if (cNode->key>=(tNode->key+tNode->numberOfBlocks)) {
                break;
            }
            // Invalidate this segment as it overlapped.
            printf("Invalidating LBA range %p [%d..%d] as it overlaps with new one - [%d..%d]\n", cNode, cNode->key, (cNode->key+cNode->numberOfBlocks), tNode->key, (tNode->key+tNode->numberOfBlocks));
            cacheMgmt.root=freeNode(cacheMgmt.root, cNode);
            cNode=tNode->higher;
        }
    }
    // Scan the Thread and make sure all segments are ordered
    // Fetch the first segment in the Thread, one that is pointed by cacheMgmt.lowest.higher.
    tNode=cacheMgmt.lowest.higher;
    currentLba=0;
    currentNB=0;
    i=0;
    while (tNode!=&cacheMgmt.highest) {
        // Make sure this segment has an LBA that is equal or bigger than previous LBA + number of blocks
        assert(tNode->key>=currentLba+currentNB);
        currentLba=tNode->key;
        (void)dumpPathToKey(cacheMgmt.root, currentLba);
        i++;
        currentNB=tNode->numberOfBlocks;
        tNode=tNode->higher;
    }

    // Traverse the Thread and remove each & every node from TAVL and the list. Node gets returned to free pool.
    // Fetch the first segment in the Thread, one that is pointed by cacheMgmt.lowest.higher.
    printf("Removing all nodes in the Thread\n");
    tNode=cacheMgmt.lowest.higher;
    while (tNode!=&cacheMgmt.highest) {
        // Remove this node
        nextNode=tNode->higher;
        cacheMgmt.root=freeNode(cacheMgmt.root, tNode);
        tNode=nextNode;
    }

    // Traverse the LRU and dump any remaining segments.
    printf("Dumping any segments in LRU, there should be none left\n");
    tNode=cacheMgmt.lru.head.next;
    i=0;
    while (tNode!=&cacheMgmt.lru.tail) {
        printf("%dth node %p in the LRU, LBA range [%d..%d]\n", i, tNode, tNode->key, tNode->key+tNode->numberOfBlocks);
        // Remove this node
        tNode=tNode->next;
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
