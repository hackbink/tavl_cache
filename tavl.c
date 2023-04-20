#include <stdlib.h>
#include <stddef.h>
#include "tavl.h"

//-----------------------------------------------------------
// Functions
//-----------------------------------------------------------
void initSegment(segment_t *pSeg) {
    pSeg->left = NULL;
    pSeg->right = NULL;
    pSeg->lower = NULL;
    pSeg->higher = NULL;
    pSeg->prev = NULL;
    pSeg->next = NULL;
    pSeg->key = 0;
    pSeg->numberOfBlocks = 0;
    pSeg->height = 0;
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

void removeFromThread(segment_t *pSeg) {
    segment_t *pLower = pSeg->lower;
    segment_t *pHigher = pSeg->higher;
    pLower->higher=pHigher;
    pHigher->lower=pLower;
    pSeg->lower=NULL;
    pSeg->higher=NULL;
}

void insertBefore(segment_t *pSeg, segment_t *pTarget) {
    segment_t *pLower = pTarget->lower;
    pLower->higher=pSeg;
    pTarget->lower=pSeg;
    pSeg->lower=pLower;
    pSeg->higher=pTarget;
}

void insertAfter(segment_t *pSeg, segment_t *pTarget) {
    segment_t *pHigher = pTarget->higher;
    pHigher->lower=pSeg;
    pTarget->higher=pSeg;
    pSeg->lower=pTarget;
    pSeg->higher=pHigher;
}

void initCache(void) {
    unsigned i;
    // Initialize cache management data structure
    // 1. Initialize cacheMgmt.
    cacheMgmt.root = NULL;
    cacheMgmt.active_nodes = 0;
    initSegment(&cacheMgmt.lowest);
    initSegment(&cacheMgmt.highest);
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
        initSegment(&node[i]);
        pushToTail(&node[i], &cacheMgmt.free);
    }
}

unsigned avlHeight(segment_t *head) {
    if (NULL == head) {
        return 0;
    }
    return head->height;
}

segment_t *rightRotation(segment_t *head) {
    segment_t *newHead = head->left;
    head->left = newHead->right;
    newHead->right = head;
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    newHead->height = 1 + MAX(avlHeight(newHead->left), avlHeight(newHead->right));
    return newHead;
}

segment_t *leftRotation(segment_t *head) {
    segment_t *newHead = head->right;
    head->right = newHead->left;
    newHead->left = head;
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    newHead->height = 1 + MAX(avlHeight(newHead->left), avlHeight(newHead->right));
    return newHead;
}

segment_t *insertNode(segment_t *head, segment_t *x) {
    if (NULL == head) {
        return x;
    }
    if (x->key < head->key) {
        head->left = insertNode(head->left, x);
    } else if (x->key > head->key) {
        head->right = insertNode(head->right, x);
    }
    head->height = 1 + MAX(avlHeight(head->left), avlHeight(head->right));
    int bal = avlHeight(head->left) - avlHeight(head->right);
    if (bal > 1) {
        if (x->key < head->left->key) {
            return rightRotation(head);
        } else {
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    } else if (bal < -1) {
        if (x->key > head->right->key) {
            return leftRotation(head);
        } else {
            head->right = rightRotation(head->right);
            return leftRotation(head);
        }
    }

    return head;
}

segment_t *removeNode(segment_t *head, segment_t *x) {
    if (NULL == head) {
        return NULL;
    }
    if (x->key < head->key) {
        // if the node belongs to the left, traverse through left.
        head->left = removeNode(head->left, x);
    } else if (x->key > head->key) {
        // if the node belongs to the right, traverse through right.
        head->right = removeNode(head->right, x);
    } else {
        // if the node is the tree, copy the key of the node that is just bigger than the key being removed & remove the node from the right
        segment_t *r = head->right;
        if (NULL == head->right) {
            segment_t *l = head->left;
            // Specific to TAVL
            {
                removeFromThread(head);
                // If head belongs to any list, we didn't take the path that copies key and removes it from any list.
                if (NULL!=head->next) {
                    removeFromList(head);
                }
                pushToTail(head, &cacheMgmt.free);
            }
            head = l;
        } else if (NULL == head->left) {
            // Specific to TAVL
            {
                removeFromThread(head);
                // If head belongs to any list, we didn't take the path that copies key and removes it from any list.
                if (NULL!=head->next) {
                    removeFromList(head);
                }
                pushToTail(head, &cacheMgmt.free);
            }
            head = r;
        } else {
            while (NULL != r->left) {
                r = r->left;
            }
            head->key = r->key;
            // Specific to TAVL
            {
                // We need to copy most other fields, not just key.
                head->numberOfBlocks = r->numberOfBlocks;
                // Since we are going to remove r, r needs to be removed from any list.
                // This can be done either now or when r gets removed from the tree.
                // It is easier to do it now because the head needs to replace the place that r is in the list.
                segment_t *pPrev = head->prev;
                segment_t *pNext = head->next;
                // Remove head from the list first.
                pPrev->next=pNext;
                pNext->prev=pPrev;
                // Then put the head in the place r is.
                pPrev = r->prev;
                pNext = r->next;
                head->next=pNext;
                head->prev=pPrev;
                pPrev->next=head;
                pNext->prev=head;
                r->prev=NULL;
                r->next=NULL;
            }
            head->right = removeNode(head->right, r);
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

segment_t *searchAvl(segment_t *head, unsigned key) {
    if (NULL == head) {
        return NULL;
    }
    unsigned k = head->key;
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

segment_t *searchTavl(segment_t *head, unsigned lba) {
    if (NULL == head) {
        return NULL;
    }
    unsigned k = head->key;
    if (lba == k) {
        return head;
    }
    if (k > lba) {
        if (NULL==head->left) {
            return head->lower;
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

segment_t *insertToTavl(segment_t *head, segment_t *x) {
    if (NULL == head) {
        cacheMgmt.lowest.higher=x;
        x->lower=&cacheMgmt.lowest;
        cacheMgmt.highest.lower=x;
        x->higher=&cacheMgmt.highest;
        cacheMgmt.active_nodes++;
        return x;
    }
    if (x->key < head->key) {
        if (NULL==head->left) {
            insertBefore(x, head);
            head->left = x;
            cacheMgmt.active_nodes++;
        } else {
            head->left = insertToTavl(head->left, x);
        }
    } else if (x->key > head->key) {
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
        if (x->key < head->left->key) {
            return rightRotation(head);
        } else {
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    } else if (bal < -1) {
        if (x->key > head->right->key) {
            return leftRotation(head);
        } else {
            head->right = rightRotation(head->right);
            return leftRotation(head);
        }
    }
    return head;
}

segment_t *freeNode(segment_t *root, segment_t *x) {
    // Remove the node from AVL tree & return the new root
    cacheMgmt.active_nodes--;
    return removeNode(root, x);
}
