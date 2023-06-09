#ifndef __TAVL_H
#define __TAVL_H

#include <stdbool.h>

//-----------------------------------------------------------
// Macros
//-----------------------------------------------------------
#define MAX(x,y) (((x) >= (y)) ? (x) : (y))
#define MIN(x,y) (((x) >= (y)) ? (y) : (x))

//-----------------------------------------------------------
// Structure definitions
//-----------------------------------------------------------
typedef struct segment {
    // Previous and Next pointer used for Locked/LRU/Dirty/Free list
    struct segment  *prev;
    struct segment  *next;
    void            *pNode;
    unsigned        key;
    unsigned        numberOfBlocks;
} segment_t;

typedef struct tavl_node {
    // Left and right pointer used for tree
    struct tavl_node  *left;
    struct tavl_node  *right;
    // Lower and Higher pointer used for thread list (sorted in LBA)
    struct tavl_node  *lower;
    struct tavl_node  *higher;
    segment_t       *pSeg;
    unsigned        height;
} tavl_node_t;

typedef struct segList {
    segment_t   head;
    segment_t   tail;
} segList_t;

typedef struct tavl {
    tavl_node_t *root;
    tavl_node_t lowest;
    tavl_node_t highest;
    int         active_nodes;
} tavl_t;

typedef struct cManagement {
	tavl_t		tavl;
    segList_t   locked;
    segList_t   lru;
    segList_t   dirty;
    segList_t   free;
} cManagement_t;

//-----------------------------------------------------------
// Global variables
//-----------------------------------------------------------
extern  segment_t       *pSegmentPool;
extern	tavl_node_t     *pNodePool;
extern  cManagement_t   cacheMgmt;


//-----------------------------------------------------------
// Functions
//-----------------------------------------------------------
/**
 *  @brief  Initializes the given segment with a clean state
 *  @param  segment_t *pSeg - the segment to be initialized
 *  @return None
 */
extern void initSegment(segment_t *pSeg);

/**
 *  @brief  Initializes the given node with a clean state
 *  @param  tavl_node_t *pNode - the node to be initialized
 *  @return None
 */
extern	void initNode(tavl_node_t *pNode);

/**
 *  @brief  Inserts the given segment into the tail of the given list - Locked, LRU, Dirty or Free
 *  @param  segment_t *pSeg - the segment to be inserted, segList_t *pList - the destination list
 *  @return None
 */
extern void pushToTail(segment_t *pSeg, segList_t *pList);

/**
 *  @brief  Removes the given segment from any list - Locked, LRU, Dirty or Free
 *          Note that the function does not need to know which list the segment is removed from
 *  @param  segment_t *pSeg - the segment to be removed
 *  @return None
 */
extern void removeFromList(segment_t *pSeg);

/**
 *  @brief  Pops a segment from the head of the given list - Locked, LRU, Dirty or Free
 *  @param  segList_t *pList - the list
 *  @return The segment that got just popped
 */
extern segment_t *popFromHead(segList_t *pList);

/**
 *  @brief  Removes the given node from the LBA ordered thread.
 *  @param  tavl_node_t *pNode - the node to be removed
 *  @return None
 */
extern void removeFromThread(tavl_node_t *pNode);

/**
 *  @brief  Inserts the given node before the target.
 *  @param  tavl_node_t *pNode - the node to be inserted, tavl_node_t *pTarget - target node
 *  @return None
 */
extern void insertBefore(tavl_node_t *pNode, tavl_node_t *pTarget);

/**
 *  @brief  Inserts the given node after the target.
 *  @param  tavl_node_t *pNode - the node to be inserted, tavl_node_t *pTarget - target node
 *  @return None
 */
extern void insertAfter(tavl_node_t *pNode, tavl_node_t *pTarget);

/**
 *  @brief  Returns the heigh of the given node
 *  @param  tavl_node_t *head - a node in the AVL tree, or NULL
 *  @return unsigned height of the node
 */
extern unsigned avlHeight(tavl_node_t *head);

/**
 *  @brief  Rotates the sub-tree to right (clockwise)
 *  @param  tavl_node_t *head - a node in the AVL tree - cannot be NULL
 *  @return root of the rotated sub-tree
 */
extern tavl_node_t *rightRotation(tavl_node_t *head);

/**
 *  @brief  Rotates the sub-tree to left (counter clockwise)
 *  @param  tavl_node_t *head - a node in the AVL tree - cannot be NULL
 *  @return root of the rotated sub-tree
 */
extern tavl_node_t *leftRotation(tavl_node_t *head);

/**
 *  @brief  Inserts the given node into the given AVL tree
 *  @param  tavl_node_t *head - a node in the AVL tree, or NULL
 *          tavl_node_t *x - a node to be inserted
 *  @return root of the new tree
 */
extern tavl_node_t *insertNode(tavl_node_t *head, tavl_node_t *x);

/**
 *  @brief  Removes the given segment from the given AVL tree
 *          Note that the entity being removed is segment, not a node.
 *          This is because remove operation may swap the content of the node
 *          to be removed with another node that has a key just higher.
 *  @param  tavl_node_t *head - a node in the AVL tree, or NULL
 *          segment_t *x - a segment to be removed
 *  @return root of the new tree
 */
extern tavl_node_t *removeNode(tavl_node_t *head, segment_t *x);

/**
 *  @brief  Searches the given AVL tree for the given key
 *  @param  tavl_node_t *head - a node in the AVL tree, or NULL
 *          unsigned key - a key to be searched
 *  @return The node that contains the key, or NULL
 */
extern tavl_node_t *searchAvl(tavl_node_t *head, unsigned key);

/**
 *  @brief  Searches the given TAVL tree for the given LBA
 *  @param  tavl_node_t *head - a node in the AVL tree, or NULL
 *          unsigned lba - an LBA to be searched
 *  @return The node that contains a key that is equal or smaller than the given LBA, or NULL
 */
extern tavl_node_t *searchTavl(tavl_node_t *head, unsigned lba);

/**
 *  @brief  Inserts the given node into the given TAVL tree.
 *          In other words,
 *          1. inserts the given node into AVL tree
 *          2. inserts the given node into the Thread
 *  @param  tavl_t *pTavl - pointer to the tavl structure
 *          tavl_node_t *x - pointer to the node to be inserted
 *  @return New root of the tree
 */
extern tavl_node_t *insertToTavl(tavl_t *pTavl, tavl_node_t *x);

// Remove a node from AVL tree, thread and list the push to free list.
// Specified list can be Locked/LRU/Dirty.
// Returns the new root.
/**
 *  @brief  Remove a segment_t from cache management TAVL tree then push to the free list.
 *  @param  segment_t *x - segment to be removed
 *  @return None
 */
extern	void freeNode(segment_t *x);

/**
 *  @brief  Searches the given TAVL tree for the given LBA and dump the path
 *  @param  tavl_node_t *head - a node in the AVL tree, or NULL
 *          unsigned lba - an LBA to be searched
 *  @return The node that contains a key that is equal or smaller than the given LBA, or NULL
 */
extern tavl_node_t *dumpPathToKey(tavl_node_t *head, unsigned lba);

/**
 *  @brief  Sanity check for TAVL tree
 *  @param  tavl_t *pTavl - pointer to the tavl structure
 *  @return None
 */
extern void tavlSanityCheck(tavl_t *pTavl);

/**
 *  @brief  Checks the height of all nodes under the given node
 *  @param  tavl_node_t *head - a node in the AVL tree, or NULL
 *  @return bool - true if heights are correct
 */
extern bool tavlHeightCheck(tavl_node_t *head);

/**
 *  @brief  Initializes the whole cache management structure - cacheMgmt, 
 *  @param  int maxNode - number of nodes
 *  @return None
 */
extern	void initCache(int maxNode);

#endif // __TAVL_H
