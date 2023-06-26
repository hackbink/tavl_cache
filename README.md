# Threaded AVL tree for block device caching application

## Introduction
Block devices need memory buffer to handle the speed gap between storage media and the host interfaces. The memory buffer is also used as buffer cache, either to retain the data for future read transfer or temporarily save dirty data to commit to storage media later.

As the memory buffer size increases, block devices need to handle more number of data fragments, often referred as cache segments. For block devices, it is common for each cache segment to contain multiple blocks of data. As each cache segment may contain many blocks of data, it would quickly become inefficient to use conventional binary tree to track all blocks in a cache segment.

This project is an example of using a balanced binary tree, more specifically AVL tree, in conjunction with an LBA (Logical Block Address) ordered list. This list is referred as Thread, thus resulting the name, Threaded AVL. The goal of this implementation is to manage the coherency of data using the Thread, while minimizing the search latency by using the AVL tree.

In addition, this projects includes multiple lists that are ordered either time domain or spatial domain. Each list contains a specific type of cache segments.

Please note that while the name and the purpose might be same, this implementation is slightly different than conventional threaded binary tree as explained below.

https://en.wikipedia.org/wiki/Threaded_binary_tree

In the above conventional threaded binary tree, a node may not point to the immediate prior/next node. This implies additional latency when searching for the node with higher/lower LBA.

## Conventional AVL tree

AVL tree guarantees that the tree is balanced and the difference in depth between the shortest path and the longest path from any node (including the root) to the bottom is no bigger than one.

In other words, the worst case search latency is bound by the log of the total number of nodes in the tree.

To maintain the tree balanced, each insert operation to a sub-tree is followed by a potential rebalancing of the sub-tree.

Each node contains a key. When searching an AVL tree for a node with a matching key, the result is either success or failure. Success is represented by returning a valid pointer to the node with matching key. An invalid pointer (i.e. NULL) indicates a failure - there was no node with such key.

### Conventional AVL Tree
![avl_tree](./images/avl_tree.png)

## Threaded AVL tree

To support searching and inserting a node with a range of blocks instead of a single block, each node contains an LBA key and a number of blocks. And each and every node in the tree is a part of LBA ordered list - Thread.

To be used for caching application, the tree needs to be coherent - there shouldn't be any range overlap between any nodes.

Note that this project does not include the coherency management as the operation is heavily depedent on the features provided by storage controllers. This will be commented in the last section.

### TAVL Tree
![tavl_tree](./images/tavl_tree.png)

A slightly different presentation of the Threaded AVL tree is below.

Hopefully, the figure below conveys the point that the actual cache scan is done using the thread and the AVL tree is only there to provide a quick way to locate the node in the thread to start the cache scan.

### TAVL Tree with visually separated Thread
![tavl_tree2](./images/tavl_tree2.png)

When searching a Threaded AVL tree for a node with a matching key, the result is not a clear success or failure. The search returns a valid pointer to the node that has a key which is equal to or smaller than the LBA to be searched. An invalid pointer (i.e. NULL) indicates that there is no node with such key.

The reason for returning the node with a key that is equal to or smaller than the LBA is that such node is the one with the lowest key that may have an overlap with LBA range searched.

The examples in the figure below show why TAVL search needs to return a node with a key that is equal to or smaller than the LBA being searched. Depending on the range, such node may or may not overlap with the LBA range. Traversing the thread to the right will pass all nodes that overlap with the LBA range being searched.

### Examples of TAVL search
![left_node](./images/left_node.png)

## Overall construction

TAVL tree allows all cache segments to be sorted in spatial domain. As there is a limited number of cache segments, cache segments need to be tracked in time domain too.

After initialization, all cache segments are pushed to the free list. As long as the free list is not empty, allocating a new cache segment is done by popping the head of the free list.

The free list will eventually become empty and the oldest cache segment needs to be recycled. This is done by tracking nodes with the LRU list. The head of the LRU list contains the node that is the oldest of all nodes in the LRU list. If the free list is empty, the node at the head of the LRU list is invalidated, popped and used.

The dirty list contains cache segments for write data. A cache segment in the dirty list cannot be moved to other lists till the write data is written to the media. Once the write data is written, the node can be moved to the LRU list or the free list. Cache segments in the dirty list do not have to be ordered based on the time. Block devices may apply reodering schemes like elevator reordering or three-dimensional reordering. Certain reordering scheme may require more than one list to manage cache segments before and after reodering. This project simply uses a linked list and does not include any reordering scheme or any list structure for reordering scheme.

The locked list may contain promoted cache segments - promoted as there were cache hits for those, implying that there might be future cache hits on those. In such case, the locked list acts like the LRU list and whichever oldest cache segment in the locked list gets demoted into the LRU list when necessary. In another case where the locked list contains truly locked cache segments, each cache segment needs to have a reference counter that prevents the cache segment from getting removed from the locked list till the counter decrements to 0. This is typical in a system where cache search and cache update are independently done in separate threads.

### Simplified diagram of the overall construction
![tavl_tree_with_lru_dirty](./images/tavl_tree_with_lru_dirty.png)

### Lists
| List | Type | Domain | Purpose |
| --- | --- | --- | --- |
| Locked | Bidirectional | Time | To track promoted cache segments |
| LRU | Bidirectional | Time | To track the age of each cache segment |
| Dirty | Bidirectional/Circular | Time or LBA | To reorder writes in the most efficient sequence |
| Free | Bidirectional | Time | To keep invalidated and freed cache segments |

## Considerations for application

How exactly this TAVL caching scheme can be used in block devices largely depends on the storage controller.

For example, if a storage controller supports SGL buffer, 
- multiple cache segments with consecutive LBA ranges, can be merged together to form a single buffer to be managed and transferred
- a cache segment with an LBA range that gets partially invalidated by a new LBA range can be shrunken to keep a valid range, instead of getting thrown away

This is why TAVL cache search in this implementation only returns the node to start traversing. Depending on whether SGL buffer is supported, the caller can either invalidate or shrink all cache segments with overlap.

With SGL buffer, TAVL search result for write will be handled as following.
* TAVL search returns a cache segment
* By comparing the LBA ranges, check if the first part of the required LBA range exists in the cache segment
* Traverse to the right cache segment in the thread till the cache segment has an LBA that is outside of the required LBA range
* Invalidate either partial or full range of cache segments that overlap with the write range - do not stop at a gap
* If partially invalidated, keep the cache segment and just free the portion of the buffer. If fully invalidated, move the invalidated cache segment to the free list
* Insert the new cache segment into the TAVL tree and the dirty list

Without SGL buffer, TAVL search result for write will be handled similarly but without freeing a portion of the buffer. 

For write, data coherency is managed by invalidating all cache segments that overlap with the new range.

With SGL buffer, TAVL search result for read will be handled as following.
* TAVL search returns a cache segment
* By comparing the LBA range, check if the first part of the required LBA range exist in the cache segment
* Traverse to the right cache segment in the thread till a cache segment with an LBA that is outside of required LBA range, or till there is a gap
* Merge all sequential cache segments for host transfer
* If there are more blocks to read, start media read operation

For read, data coherency is managed when inserting a cache segment either before or after reading from the media.

NOTE : 
To build the code in *nix, run 'make'. Run the test binary with "./test".
To build the code in Windows with gcc(mingw), run "make". Run the test binary with "main.exe".

The test code in main.c,
- creates 100 cache segments into the free pool,
- assigns a random LBA [0.19999] with random number of blocks [10..29] to each cache segment,
- inserts each cache segment into TAVL tree and LRU list,
- finds & invalidates any cache segments in the TAVL tree with range overlap, to maintain coherency,
- once all cache segments (except the ones invalidated due to an overlap) are in TAVL tree, checks the sanity of the tree,
- for 1,000,000 loops, adds a cache segment with a random LBA [0.19999] with random number of blocks [10..29]
- while looping, removes the least recently used cache segment if there is no free cache segment available,
- checks the sanity of the tree,
- then removes each and every cache segment in the thread,
- checks if both the TAVL tree and the LRU list are empty.

It assumes that any cache segments with overlap need to be invalidated & freed. In other words, it behaves like there is no buffer SGL support.

Example of TAVL tree dump :

l(7)-l(6)-l(5)-l(4)-r(3)-l(2)-l(1)-(3149..3163)(1) : 
From the root, take left-left-left-left-right-left-left to arrive to the node with LBA from 3149 till 3163.

(13156..13166)(8) : The root of the TAVL tree with LBA from 13156 till 13166, with depth of 8.

```
l(7)-l(6)-l(5)-l(4)-l(2)-l(1)-(1490..1503)(1)
l(7)-l(6)-l(5)-l(4)-l(2)-(2177..2199)(2)
l(7)-l(6)-l(5)-l(4)-l(2)-r(1)-(2757..2781)(1)
l(7)-l(6)-l(5)-l(4)-(2998..3025)(4)
l(7)-l(6)-l(5)-l(4)-r(3)-l(2)-l(1)-(3149..3163)(1)
l(7)-l(6)-l(5)-l(4)-r(3)-l(2)-(3232..3260)(2)
l(7)-l(6)-l(5)-l(4)-r(3)-(3996..4020)(3)
l(7)-l(6)-l(5)-l(4)-r(3)-r(2)-l(1)-(4022..4036)(1)
l(7)-l(6)-l(5)-l(4)-r(3)-r(2)-(4351..4364)(2)
l(7)-l(6)-l(5)-(4563..4583)(5)
l(7)-l(6)-l(5)-r(4)-l(2)-l(1)-(4886..4915)(1)
l(7)-l(6)-l(5)-r(4)-l(2)-(5127..5147)(2)
l(7)-l(6)-l(5)-r(4)-l(2)-r(1)-(5234..5244)(1)
l(7)-l(6)-l(5)-r(4)-(5416..5439)(4)
l(7)-l(6)-l(5)-r(4)-r(3)-l(1)-(5610..5632)(1)
l(7)-l(6)-l(5)-r(4)-r(3)-(5884..5904)(3)
l(7)-l(6)-l(5)-r(4)-r(3)-r(2)-l(1)-(5924..5952)(1)
l(7)-l(6)-l(5)-r(4)-r(3)-r(2)-(5965..5979)(2)
l(7)-l(6)-l(5)-r(4)-r(3)-r(2)-r(1)-(5994..6013)(1)
l(7)-l(6)-(6032..6044)(6)
l(7)-l(6)-r(5)-l(3)-l(2)-(6116..6130)(2)
l(7)-l(6)-r(5)-l(3)-l(2)-r(1)-(6160..6174)(1)
l(7)-l(6)-r(5)-l(3)-(6196..6215)(3)
l(7)-l(6)-r(5)-l(3)-r(2)-(6221..6233)(2)
l(7)-l(6)-r(5)-l(3)-r(2)-r(1)-(6255..6279)(1)
l(7)-l(6)-r(5)-(6279..6290)(5)
l(7)-l(6)-r(5)-r(4)-l(2)-(6348..6363)(2)
l(7)-l(6)-r(5)-r(4)-l(2)-r(1)-(6534..6562)(1)
l(7)-l(6)-r(5)-r(4)-(6611..6629)(4)
l(7)-l(6)-r(5)-r(4)-r(3)-l(2)-l(1)-(7171..7181)(1)
l(7)-l(6)-r(5)-r(4)-r(3)-l(2)-(8449..8461)(2)
l(7)-l(6)-r(5)-r(4)-r(3)-(8519..8536)(3)
l(7)-l(6)-r(5)-r(4)-r(3)-r(2)-(8590..8605)(2)
l(7)-l(6)-r(5)-r(4)-r(3)-r(2)-r(1)-(8865..8887)(1)
l(7)-(9003..9028)(7)
l(7)-r(6)-l(4)-l(3)-l(1)-(9280..9308)(1)
l(7)-r(6)-l(4)-l(3)-(9334..9351)(3)
l(7)-r(6)-l(4)-l(3)-r(2)-l(1)-(9354..9380)(1)
l(7)-r(6)-l(4)-l(3)-r(2)-(9424..9442)(2)
l(7)-r(6)-l(4)-l(3)-r(2)-r(1)-(9758..9772)(1)
l(7)-r(6)-l(4)-(9944..9963)(4)
l(7)-r(6)-l(4)-r(2)-l(1)-(10025..10044)(1)
l(7)-r(6)-l(4)-r(2)-(10098..10111)(2)
l(7)-r(6)-l(4)-r(2)-r(1)-(10125..10148)(1)
l(7)-r(6)-(11059..11070)(6)
l(7)-r(6)-r(5)-l(4)-l(2)-l(1)-(11251..11275)(1)
l(7)-r(6)-r(5)-l(4)-l(2)-(11284..11311)(2)
l(7)-r(6)-r(5)-l(4)-l(2)-r(1)-(11390..11418)(1)
l(7)-r(6)-r(5)-l(4)-(11816..11833)(4)
l(7)-r(6)-r(5)-l(4)-r(3)-l(2)-(11928..11941)(2)
l(7)-r(6)-r(5)-l(4)-r(3)-l(2)-r(1)-(11980..11994)(1)
l(7)-r(6)-r(5)-l(4)-r(3)-(12003..12015)(3)
l(7)-r(6)-r(5)-l(4)-r(3)-r(2)-l(1)-(12046..12056)(1)
l(7)-r(6)-r(5)-l(4)-r(3)-r(2)-(12056..12071)(2)
l(7)-r(6)-r(5)-l(4)-r(3)-r(2)-r(1)-(12240..12259)(1)
l(7)-r(6)-r(5)-(12304..12328)(5)
l(7)-r(6)-r(5)-r(3)-l(1)-(12490..12519)(1)
l(7)-r(6)-r(5)-r(3)-(12738..12764)(3)
l(7)-r(6)-r(5)-r(3)-r(2)-(12900..12914)(2)
l(7)-r(6)-r(5)-r(3)-r(2)-r(1)-(13100..13121)(1)
(13156..13166)(8)
r(6)-l(5)-l(4)-l(3)-l(2)-(13242..13261)(2)
r(6)-l(5)-l(4)-l(3)-l(2)-r(1)-(13391..13420)(1)
r(6)-l(5)-l(4)-l(3)-(13449..13459)(3)
r(6)-l(5)-l(4)-l(3)-r(1)-(13478..13496)(1)
r(6)-l(5)-l(4)-(13545..13572)(4)
r(6)-l(5)-l(4)-r(3)-l(2)-l(1)-(13603..13620)(1)
r(6)-l(5)-l(4)-r(3)-l(2)-(13661..13683)(2)
r(6)-l(5)-l(4)-r(3)-l(2)-r(1)-(13916..13927)(1)
r(6)-l(5)-l(4)-r(3)-(13959..13969)(3)
r(6)-l(5)-l(4)-r(3)-r(2)-l(1)-(13979..14007)(1)
r(6)-l(5)-l(4)-r(3)-r(2)-(14379..14389)(2)
r(6)-l(5)-l(4)-r(3)-r(2)-r(1)-(14498..14508)(1)
r(6)-l(5)-(14594..14608)(5)
r(6)-l(5)-r(4)-l(3)-l(1)-(14615..14637)(1)
r(6)-l(5)-r(4)-l(3)-(14754..14769)(3)
r(6)-l(5)-r(4)-l(3)-r(2)-(14776..14796)(2)
r(6)-l(5)-r(4)-l(3)-r(2)-r(1)-(14883..14899)(1)
r(6)-l(5)-r(4)-(14951..14964)(4)
r(6)-l(5)-r(4)-r(2)-l(1)-(14974..14984)(1)
r(6)-l(5)-r(4)-r(2)-(15002..15020)(2)
r(6)-l(5)-r(4)-r(2)-r(1)-(15030..15054)(1)
r(6)-(15109..15126)(6)
r(6)-r(5)-l(3)-l(1)-(15156..15175)(1)
r(6)-r(5)-l(3)-(15260..15289)(3)
r(6)-r(5)-l(3)-r(2)-l(1)-(15332..15359)(1)
r(6)-r(5)-l(3)-r(2)-(15512..15541)(2)
r(6)-r(5)-(15650..15668)(5)
r(6)-r(5)-r(4)-l(3)-l(2)-l(1)-(15709..15727)(1)
r(6)-r(5)-r(4)-l(3)-l(2)-(17474..17498)(2)
r(6)-r(5)-r(4)-l(3)-(17621..17650)(3)
r(6)-r(5)-r(4)-l(3)-r(2)-l(1)-(18406..18422)(1)
r(6)-r(5)-r(4)-l(3)-r(2)-(18692..18711)(2)
r(6)-r(5)-r(4)-l(3)-r(2)-r(1)-(19092..19120)(1)
r(6)-r(5)-r(4)-(19507..19532)(4)
r(6)-r(5)-r(4)-r(3)-l(2)-(19599..19620)(2)
r(6)-r(5)-r(4)-r(3)-l(2)-r(1)-(19759..19786)(1)
r(6)-r(5)-r(4)-r(3)-(19843..19858)(3)
r(6)-r(5)-r(4)-r(3)-r(2)-l(1)-(19871..19894)(1)
r(6)-r(5)-r(4)-r(3)-r(2)-(19967..19988)(2)
```
