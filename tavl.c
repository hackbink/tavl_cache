#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include "tavl.h"

//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------
segment_t       *pSegmentPool;
tavl_node_t     *pNodePool;
cManagement_t   cacheMgmt;

//-----------------------------------------------------------
// Functions
//-----------------------------------------------------------
void initSegment(segment_t *pSeg) {
    pSeg->prev = NULL;
    pSeg->next = NULL;
    pSeg->key = 0;
    pSeg->numberOfBlocks = 0;
}

void initNode(tavl_node_t *pNode) {
    pNode->left = NULL;
    pNode->right = NULL;
    pNode->lower = NULL;
    pNode->higher = NULL;
    pNode->height = 1;
}

void pushToTail(segment_t *pSeg, segList_t *pList) {
    segment_t *pPrev = pList->tail.prev;
    pPrev->next=pSeg;
    pSeg->prev=pPrev;
    pList->tail.prev=pSeg;
    pSeg->next=&(pList->tail);
}

void removeFromList(segment_t *pSeg) {
    segment_t *pPrev = pSeg->prev;
    segment_t *pNext = pSeg->next;
    pPrev->next=pNext;
    pNext->prev=pPrev;
    pSeg->prev=NULL;
    pSeg->next=NULL;
}

segment_t *popFromHead(segList_t *pList) {
    segment_t *pSeg=pList->head.next;
    if (&pList->tail==pSeg) {
        return NULL;
    }
    removeFromList(pSeg);
    return pSeg;
}

void removeFromThread(tavl_node_t *pNode) {
    tavl_node_t *pLower = pNode->lower;
    tavl_node_t *pHigher = pNode->higher;
    pLower->higher=pHigher;
    pHigher->lower=pLower;
    pNode->lower=NULL;
    pNode->higher=NULL;
}

void insertBefore(tavl_node_t *pNode, tavl_node_t *pTarget) {
    tavl_node_t *pLower = pTarget->lower;
    pLower->higher=pNode;
    pTarget->lower=pNode;
    pNode->lower=pLower;
    pNode->higher=pTarget;
}

void insertAfter(tavl_node_t *pNode, tavl_node_t *pTarget) {
    tavl_node_t *pHigher = pTarget->higher;
    pHigher->lower=pNode;
    pTarget->higher=pNode;
    pNode->lower=pTarget;
    pNode->higher=pHigher;
}

unsigned avlHeight(tavl_node_t *head) {
    if (NULL == head) {
        return 0;
    }
    return head->height;
}

tavl_node_t *rightRotation(tavl_node_t *head) {
	assert(NULL!=head);
	assert(NULL!=head->left);
    tavl_node_t *newHead = head->left;
	assert(NULL!=newHead);
    head->left = newHead->right;
    newHead->right = head;
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    newHead->height = 1 + MAX(avlHeight(newHead->left), avlHeight(newHead->right));
    return newHead;
}

tavl_node_t *leftRotation(tavl_node_t *head) {
	assert(NULL!=head);
	assert(NULL!=head->right);
    tavl_node_t *newHead = head->right;
	assert(NULL!=newHead);
    head->right = newHead->left;
    newHead->left = head;
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    newHead->height = 1 + MAX(avlHeight(newHead->left), avlHeight(newHead->right));
    return newHead;
}

tavl_node_t *insertNode(tavl_node_t *head, tavl_node_t *x) {
    if (NULL == head) {
        return x;
    }
    if (x->pSeg->key < head->pSeg->key) {
        head->left = insertNode(head->left, x);
    } else if (x->pSeg->key > head->pSeg->key) {
        head->right = insertNode(head->right, x);
    }
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    int bal = avlHeight(head->left) - avlHeight(head->right);
    if (bal > 1) {
        if (x->pSeg->key < head->left->pSeg->key) {
            return rightRotation(head);
        } else {
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    } else if (bal < -1) {
        if (x->pSeg->key > head->right->pSeg->key) {
            return leftRotation(head);
        } else {
            head->right = rightRotation(head->right);
            return leftRotation(head);
        }
    }

    return head;
}

tavl_node_t *removeNode(tavl_node_t *head, segment_t *x) {
    if (NULL == head) {
        return NULL;
    }
    if (x->key < head->pSeg->key) {
        // if the node belongs to the left, traverse through left.
        head->left = removeNode(head->left, x);
    } else if (x->key > head->pSeg->key) {
        // if the node belongs to the right, traverse through right.
        head->right = removeNode(head->right, x);
    } else {
        // if the node is the tree, copy the key of the node that is just bigger than the key being removed & remove the node from the right
        tavl_node_t *r = head->right;
        if (NULL == head->right) {
            tavl_node_t *l = head->left;
            // Remove from the thread.
            removeFromThread(head);
            head = l;
        } else if (NULL == head->left) {
            // Remove from the thread.
            removeFromThread(head);
            head = r;
        } else {
            // Instead of traversing the tree, use the thread to find the right next one.
            r = (tavl_node_t *)(head->higher);

            // Swap the segment between head and r.
            // The segment pointed by r will be preserved in the thread.
            // The segment pointed by head will be removed from the thread when r node gets removed later.
            segment_t *pHeadSeg = head->pSeg;
            segment_t *pRSeg = r->pSeg;
            head->pSeg = pRSeg;
            r->pSeg = pHeadSeg;
            pHeadSeg->pNode = (void *)r;
            pRSeg->pNode = (void *)head;

            head->right = removeNode(head->right, r->pSeg);
        }
    }
    // unless the tree is empty, check the balance and rebalance the tree before traversing back
    if (NULL == head) {
        return NULL;
    }
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    int bal = avlHeight(head->left) - avlHeight(head->right);
    if (bal > 1) {
        if (avlHeight(head->left->left) >= avlHeight(head->left->right)) {
            return rightRotation(head);
        } else {
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    } else if (bal < -1 ) {
        if (avlHeight(head->right->right) >= avlHeight(head->right->left)) {
            return leftRotation(head);
        } else {
            head->right = rightRotation(head->right);
            return leftRotation(head);
        }
    }
    return head;
}

tavl_node_t *searchAvl(tavl_node_t *head, unsigned key) {
    if (NULL == head) {
        return NULL;
    }
    unsigned k = head->pSeg->key;
    if (key == k) {
        return head;
    }
    if (k > key) {
        return searchAvl(head->left, key);
    }
    if (k < key) {
        return searchAvl(head->right, key);
    }
}

tavl_node_t *searchTavl(tavl_node_t *head, unsigned lba) {
    if (NULL == head) {
        return NULL;
    }
    unsigned k = head->pSeg->key;
    if (lba == k) {
        return head;
    }
    if (k > lba) {
        if (NULL==head->left) {
            return (tavl_node_t *)(head->lower);
        }
        return searchTavl(head->left, lba);
    }
    if (k < lba) {
        if (NULL==head->right) {
            return head;
        }
        return searchTavl(head->right, lba);
    }
}

/**
 *  @brief  Inserts the given node into the given TAVL tree that is NOT empty.
 *          In other words,
 *          1. inserts the given node into AVL tree
 *          2. inserts the given node into the Thread
 *  @param  tavl_node_t *head - root of the tree, 
 *          tavl_node_t *x - pointer to the node to be inserted
 *  @return New root of the tree
 */
tavl_node_t *_insertToTavl(tavl_node_t *head, tavl_node_t *x) {
	assert(NULL!=x);
	assert(NULL!=x->pSeg);
	assert(NULL!=head);
	assert(NULL!=head->pSeg);
    if (x->pSeg->key < head->pSeg->key) {
        if (NULL==head->left) {
            insertBefore(x, head);
            head->left = x;
        } else {
            head->left = _insertToTavl(head->left, x);
        }
    } else if (x->pSeg->key > head->pSeg->key) {
        if (NULL==head->right) {
            insertAfter(x, head);
            head->right = x;
        } else {
            head->right = _insertToTavl(head->right, x);
        }
    }
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    int bal = avlHeight(head->left) - avlHeight(head->right);
    if (bal > 1) {
        if (x->pSeg->key < head->left->pSeg->key) {
            return rightRotation(head);
        } else {
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    } else if (bal < -1) {
        if (x->pSeg->key > head->right->pSeg->key) {
            return leftRotation(head);
        } else {
            head->right = rightRotation(head->right);
            return leftRotation(head);
        }
    }
    return head;
}

tavl_node_t *insertToTavl(tavl_t *pTavl, tavl_node_t *x) {
	assert(NULL!=pTavl);
	assert(NULL!=x);
    pTavl->active_nodes++;
    if (NULL == pTavl->root) {
        pTavl->lowest.higher=x;
        x->lower=&pTavl->lowest;
        pTavl->highest.lower=x;
        x->higher=&pTavl->highest;
        return x;
    } else {
        return _insertToTavl(pTavl->root, x);
    }
}

void freeNode(segment_t *x) {
    removeFromList(x);
    pushToTail(x, &cacheMgmt.free);

    // Remove the node from TAVL tree & return the new root
    cacheMgmt.tavl.active_nodes--;
    cacheMgmt.tavl.root=removeNode(cacheMgmt.tavl.root, x);
}

tavl_node_t *dumpPathToKey(tavl_node_t *head, unsigned lba) {
    if (NULL == head) {
        printf("Unknown Key\n");
        return NULL;
    }
    unsigned k = head->pSeg->key;
    if (lba == k) {
        printf("(%d..%d)(%d)\n", lba, lba+head->pSeg->numberOfBlocks,head->height);
        return head;
    }
    if (k > lba) {
        if (NULL==head->left) {
            printf("Unknown Key\n");
            return (tavl_node_t *)(head->lower);
        }
        printf("l(%d)-",head->left->height);
        return dumpPathToKey(head->left, lba);
    }
    if (k < lba) {
        if (NULL==head->right) {
            printf("Unknown Key\n");
            return head;
        }
        printf("r(%d)-",head->right->height);
        return dumpPathToKey(head->right, lba);
    }
}

void tavlSanityCheck(tavl_t *pTavl) {
	tavl_node_t *cNode,*searchedNode;
	segment_t 	*tSeg;
    unsigned currentLba, currentNB;
    unsigned i;

    cNode=pTavl->lowest.higher;
	assert(NULL!=cNode);
    currentLba=0;
    currentNB=0;
    i=0;
    // Traverse through the thread and check each node
    while (cNode!=&pTavl->highest) {
		assert(NULL!=cNode);
        // Make sure this segment has an LBA that is equal or bigger than previous LBA + number of blocks
        assert(cNode->pSeg->key>=currentLba+currentNB);
        // Check if the node is linked with a segment
		tSeg=cNode->pSeg;
		assert(tSeg->pNode==(void *)cNode);
        currentLba=cNode->pSeg->key;
        (void)dumpPathToKey(pTavl->root, currentLba);
        // The node in the thread should exist in the tree too
		searchedNode=searchAvl(pTavl->root, currentLba);
		if (searchedNode==NULL) {
			printf("tavlSanityCheck() could not find the LBA %d.\n", currentLba);
			assert(searchedNode!=NULL);
		}
        i++;
        currentNB=cNode->pSeg->numberOfBlocks;
        cNode=cNode->higher;
    }
    // Check if the active_nodes matches with the number of nodes traversed.
	assert(pTavl->active_nodes==i);
}

bool tavlHeightCheck(tavl_node_t *head) {
    if (NULL == head) {
        return true;
    }
	if ((NULL==head->left) && (NULL==head->right)) {
		if (head->height != 1) {
			printf("tavlHeightCheck(%p) with key:%d. height:%d should have been 1\n", head, head->pSeg->key, head->height);
			return false;
		}
		return true;
	}
    if (head->height != 1 + MAX(avlHeight(head->left), avlHeight(head->right))) {
		printf("tavlHeightCheck(%p) height:%d, key:%d, left height:%d, right height:%d\n", head, head->height, head->pSeg->key, avlHeight(head->left), avlHeight(head->right));
		assert(head->height == 1 + MAX(avlHeight(head->left), avlHeight(head->right)));
	}

    int bal = avlHeight(head->left) - avlHeight(head->right);
    if ((bal > 1)||(bal<-1)) {
		printf("tavlHeightCheck(%p) height:%d, key:%d, left height:%d, right height:%d\n", head, head->height, head->pSeg->key, avlHeight(head->left), avlHeight(head->right));
		assert((bal <= 1)&&(bal>=-1));
	}
	if (head->left) {
		if (false==tavlHeightCheck(head->left)) {
			return false;
		}
	}
	if (head->right) {
		if (false==tavlHeightCheck(head->right)) {
			return false;
		}
	}
	return true;
}

void initCache(int maxNode) {
    unsigned i;

    // Initialize cache management data structure
    // 1. Initialize cacheMgmt.
    cacheMgmt.tavl.root = NULL;
    cacheMgmt.tavl.active_nodes = 0;
    initNode(&cacheMgmt.tavl.lowest);
    initNode(&cacheMgmt.tavl.highest);
    cacheMgmt.tavl.lowest.higher=&cacheMgmt.tavl.highest;
    cacheMgmt.tavl.highest.lower=&cacheMgmt.tavl.lowest;
    initSegment(&cacheMgmt.locked.head);
    initSegment(&cacheMgmt.locked.tail);
    cacheMgmt.locked.head.next=&cacheMgmt.locked.tail;
    cacheMgmt.locked.tail.prev=&cacheMgmt.locked.head;
    initSegment(&cacheMgmt.lru.head);
    initSegment(&cacheMgmt.lru.tail);
    cacheMgmt.lru.head.next=&cacheMgmt.lru.tail;
    cacheMgmt.lru.tail.prev=&cacheMgmt.lru.head;
    initSegment(&cacheMgmt.dirty.head);
    initSegment(&cacheMgmt.dirty.tail);
    cacheMgmt.dirty.head.next=&cacheMgmt.dirty.tail;
    cacheMgmt.dirty.tail.prev=&cacheMgmt.dirty.head;
    initSegment(&cacheMgmt.free.head);
    initSegment(&cacheMgmt.free.tail);
    cacheMgmt.free.head.next=&cacheMgmt.free.tail;
    cacheMgmt.free.tail.prev=&cacheMgmt.free.head;

    // 2. Initialize each segment and push into cacheMgmt.free.
	pSegmentPool=malloc(maxNode*sizeof(segment_t));
	pNodePool=malloc(maxNode*sizeof(tavl_node_t));
	assert(NULL!=pSegmentPool);
	assert(NULL!=pNodePool);
    for (i = 0; i < maxNode; i++) {
        initSegment(&pSegmentPool[i]);
        initNode(&pNodePool[i]);
        pNodePool[i].pSeg=&pSegmentPool[i];
        pSegmentPool[i].pNode=(void *)&pNodePool[i];
        pushToTail(&pSegmentPool[i], &cacheMgmt.free);
    }
}
