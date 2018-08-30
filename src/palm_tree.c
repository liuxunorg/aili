/**
 *    author:     UncP
 *    date:    2018-08-22
 *    license:    BSD-3
**/

#include <stdlib.h>
#include <math.h>

#include "palm_tree.h"

palm_tree* new_palm_tree()
{
  palm_tree *pt = (palm_tree *)malloc(sizeof(palm_tree));
  pt->root = new_node(Leaf, 0); // set root as leaf to avoid some adjustment later
  return pt;
}

void free_palm_tree(palm_tree *pt)
{
  // TODO

}

void palm_tree_execute(palm_tree *pt, batch *b, worker *w)
{
  // calculate [beg, end) in a batch that current thread needs to process
  // it's possible that a worker has no key to process
  uint32_t part = (uint32_t)ceilf((float)b->keys / w->total);
  uint32_t beg = w->id * part;
  uint32_t end = beg + part > b->keys ? b->keys;

  uint32_t level = root->level;

  worker_clear(w);

  // for each key, we descend to leaf node, and store each key's descending path
  for (uint32_t i = beg; i < end; ++i) {
    uint32_t  op;
    void    *key;
    uint32_t len;
    void    *val;
    // get basic info of this kv
    assert(batch_read_at(b, i, &op, &key, &len, &val));

    path* p = worker_get_new_path(w);
    path_set_kv_id(p, i);

    // loop until we reach level 0 node, push all the node to `p` along the way
    node *cur = root;
    while (level--) {
      node *pre = cur;
      cur = node_descend(cur, key, len);
      assert(cur);
      path_push_node(p, pre);
    }

    path_push_node(p, cur);
  }

  // TODO: point-to-point synchronization
  // wait until all the threads collected the path information
  barrier_wait(w->barrier);

  // try to find overlap nodes in previoud worker and next worker,
  // if there is a previous worker owns the same leaf node in current worker,
  // it will be processed by previous worker
  worker_resolve_hazards(w);

  // now we can process all the paths that belong to this worker

  char *fence_key;
  uint32_t fence_len;
  node *pn = 0; // previous node
  node *to_process; // node actually to process the key
  node *next = 0; // node next to `to_process`

  path_iter iter;
  path *cp;
  init_path_iter(&iter, w);

  // iterate all the path and write or read the key in the leaf node
  while ((cp = next_path(&iter))) {
    node *cn = path_get_leaf_node(cp);
    uint32_t  op;
    void    *key;
    uint32_t len;
    void    *val;
    assert(batch_read_at(b, path_get_kv_id(cp), &op, &key, &len, &val));

    if (cn == pn) {
      if (next) {
        to_process = compare_key(key, len, fence_key, fence_len) < 0 ? to_process : next;
      } else {
        // `to_process` is the latest node, continue to use it
      }
    } else {
      to_process = cn;
      next = 0; // previous split has no influence on current key
    }

    if (op == Write) {
      switch (node_insert(to_process, key, len, val)) {
        case 1:  // key insert succeed, we set value to 1
          set_val(val, 1);
          break;
        case 0:  // key already insert, we set value to 0
          set_val(val, 0);
          break;
        case -1: // node does not have enough space, needs to split
          node *new = new_node(to_process->type, to_process->level);
          // record fence key for later promotion
          fence *f = worker_get_new_fence(w);
          f->ptr = new;
          f->id = path_get_kv_id(cp);
          node_split(to_process, new, fence.key, &fence.len);
          fence_key = fence.key;
          fence_len = fence.len;

          // compare current key with fence key to determine which node to insert
          if (compare_key(key, len, fence_key, fence_len) < 0) {
            assert(node_insert(to_process, key, len, val) == 1);
            next = new;
          } else {
            assert(node_insert(new, key, len, val) == 1);
            to_process = new;
            next = 0;
          }
          break;
        default:
          assert(0);
      }
    } else { // Read
      set_val(val, node_search(to_process, key, len));
    }

    pn = cn; // record previous node
  }

  // we have successfully processed all the modifications to the leaf nodes
  // now we need to fix the split

}