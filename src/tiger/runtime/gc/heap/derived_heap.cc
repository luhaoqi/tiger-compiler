#include "derived_heap.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stack>
#include <stdio.h>

namespace gc {
// TODO(lab7): You need to implement those interfaces inherited from TigerHeap
// correctly according to your design.

void DerivedHeap::Initialize(uint64_t size) {
  heap = new uint8_t[size];
  mark = new bool[size];
  heap_size = size;
  // 初始化free_list链表
  node = new NODE[size];
  free_list = 0;
  // prev:-1 next:heap_size 分别表示null
  node[0] = NODE(-1, heap_size, heap_size);
}

DerivedHeap::~DerivedHeap() {
  delete heap;
  delete mark;
  delete node;
}

char *DerivedHeap::Allocate(uint64_t size) {
  for (int i = free_list; i < heap_size; i = node[i].next) {
    int now = node[i].size;
    if (now > size) {
      // 分裂出小节点
      node[i + size] = NODE(node[i].prev, node[i].next, now - size);
      int *pre_nxt;
      pre_nxt = node[i].prev == -1 ? (&free_list) : (&node[node[i].prev].next);
      *pre_nxt = i + size;

      if (node[i].next != heap_size)
        node[node[i].next].prev = i + size;

      node[i] = NODE(-1, -1, -1);
      printf("node_size=%d alloc_size=%d i=%d addr=%p\n", now, (int)size, i,
             (char *)(&heap[i]));
      return (char *)(&heap[i]);
    } else if (now == size) {
      // 完全删除
      printf("%d %d %d %d \n", i, node[i].prev, node[i].next, node[i].size);
      int *pre_nxt;
      pre_nxt = node[i].prev == -1 ? (&free_list) : (&node[node[i].prev].next);
      *pre_nxt = node[i].next;

      if (node[i].next != heap_size)
        node[node[i].next].prev = node[i].prev;
      printf("node_size=%d alloc_size=%d i=%d addr=%p\n", now, (int)size, i,
             (char *)(&heap[i]));
      return (char *)(&heap[i]);
    } else {
      // 大小不够
      continue;
    }
  }
  printf("heap full!");
  return nullptr;
}

uint64_t DerivedHeap::Used() const {
  int free_tot = 0;
  for (int i = free_list; i < heap_size; i = node[i].next) {
    free_tot += node[i].size;
  }
  return heap_size - free_tot;
}

uint64_t DerivedHeap::MaxFree() const {
  uint64_t max_block = 0;
  for (int i = free_list; i < heap_size; i = node[i].next) {
    max_block = max_block > node[i].size ? max_block : node[i].size;
  }
  return max_block;
}

void DerivedHeap::GC() {



}

} // namespace gc
