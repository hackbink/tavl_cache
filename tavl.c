#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "tavl.h"

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
    pNode->height = 0;
}

void pushToTail(segment_t *pSeg, segList_t *pList) {
    segment_t *pPrev = pList->tail.prev;
    if (NULL==pPrev) {
        pList->head.next=pSeg;
        pSeg->prev=&(pList->head);
    } else {
        pPrev->next=pSeg;
        pSeg->prev=pPrev;
    }
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
    if (NULL==pSeg) {
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

void initCache(void) {
    unsigned i;
    // Initialize cache management data structure
    // 1. Initialize cacheMgmt.
    cacheMgmt.root = NULL;
    cacheMgmt.active_nodes = 0;
    initNode(&cacheMgmt.lowest);
    initNode(&cacheMgmt.highest);
    cacheMgmt.lowest.higher=&cacheMgmt.highest;
    cacheMgmt.highest.lower=&cacheMgmt.lowest;
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
    for (i = 0; i < NUM_OF_SEGMENTS; i++) {
        initSegment(&segment[i]);
        initNode(&node[i]);
        node[i].pSeg=&segment[i];
        segment[i].pNode=(void *)&node[i];
        pushToTail(&segment[i], &cacheMgmt.free);
    }
}

unsigned avlHeight(tavl_node_t *head) {
    if (NULL == head) {
        return 0;
    }
    return head->height;
}

tavl_node_t *rightRotation(tavl_node_t *head) {
    tavl_node_t *newHead = head->left;
    head->left = newHead->right;
    newHead->right = head;
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    newHead->height = 1 + MAX(avlHeight(newHead->left), avlHeight(newHead->right));
    return newHead;
}

tavl_node_t *leftRotation(tavl_node_t *head) {
    tavl_node_t *newHead = head->right;
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
        if (avlHeight(head->left) >= avlHeight(head->right)) {
            return rightRotation(head);
        } else {
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    } else if (bal < -1 ) {
        if (avlHeight(head->right) >= avlHeight(head->left)) {
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

tavl_node_t *insertToTavl(tavl_node_t *head, tavl_node_t *x) {
    if (NULL == head) {
        cacheMgmt.lowest.higher=x;
        x->lower=&cacheMgmt.lowest;
        cacheMgmt.highest.lower=x;
        x->higher=&cacheMgmt.highest;
        cacheMgmt.active_nodes++;
        return x;
    }
    if (x->pSeg->key < head->pSeg->key) {
        if (NULL==head->left) {
            insertBefore(x, head);
            head->left = x;
            cacheMgmt.active_nodes++;
        } else {
            head->left = insertToTavl(head->left, x);
        }
    } else if (x->pSeg->key > head->pSeg->key) {
        if (NULL==head->right) {
            insertAfter(x, head);
            head->right = x;
            cacheMgmt.active_nodes++;
        } else {
            head->right = insertToTavl(head->right, x);
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

tavl_node_t *freeNode(tavl_node_t *root, segment_t *x) {
    removeFromList(x);
    pushToTail(x, &cacheMgmt.free);

    // Remove the node from TAVL tree & return the new root
    cacheMgmt.active_nodes--;
    return removeNode(root, x);
}

tavl_node_t *dumpPathToKey(tavl_node_t *head, unsigned lba) {
    if (NULL == head) {
        printf("Unknown Key\n");
        return NULL;
    }
    unsigned k = head->pSeg->key;
    if (lba == k) {
        printf("(%d..%d)\n", lba, lba+head->pSeg->numberOfBlocks);
        return head;
    }
    if (k > lba) {
        if (NULL==head->left) {
            printf("Unknown Key\n");
            return (tavl_node_t *)(head->lower);
        }
        printf("l-");
        return dumpPathToKey(head->left, lba);
    }
    if (k < lba) {
        if (NULL==head->right) {
            printf("Unknown Key\n");
            return head;
        }
        printf("r-");
        return dumpPathToKey(head->right, lba);
    }
}
