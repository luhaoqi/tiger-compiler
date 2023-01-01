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
  // 这里heap和mark都可以8对齐的
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
      //      printf("node_size=%d alloc_size=%d i=%d addr=%p\n", now,
      //      (int)size, i,
      //             (char *)(&heap[i]));
      return (char *)(&heap[i]);
    } else if (now == size) {
      // 完全删除
      //      printf("%d %d %d %d \n", i, node[i].prev, node[i].next,
      //      node[i].size);
      int *pre_nxt;
      pre_nxt = node[i].prev == -1 ? (&free_list) : (&node[node[i].prev].next);
      *pre_nxt = node[i].next;

      if (node[i].next != heap_size)
        node[node[i].next].prev = node[i].prev;
      //      printf("node_size=%d alloc_size=%d i=%d addr=%p\n", now,
      //      (int)size, i,
      //             (char *)(&heap[i]));
      return (char *)(&heap[i]);
    } else {
      // 大小不够
      continue;
    }
  }
  //  printf("heap full!\n");
  fflush(stdout);
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
  //  printf("come in GC!\n");
  uint64_t *sp;
  //  GET_TIGER_STACK(sp);

  uint64_t rbp;
  __asm__("movq %%rbp, %0" : "=r"(rbp));
  sp = ((uint64_t *)((*(uint64_t *)rbp) + sizeof(uint64_t)));

  // 以下是一些测试

  //  uint64_t *tmp;
  //  __asm__("leaq (tmp)(%%rip), %0" : "=r"(tmp));
  //  printf("tmp=%llx\n", *tmp);

  //  char *tmp;
  //  __asm__("leaq (tmp)(%%rip), %0" : "=r"(tmp));
  //  printf("%s %d %d\n", tmp, *(tmp + 7), *(tmp + 8));

  //  uint64_t tmp;
  //  __asm__("leaq tigermain_framesize(0), %0" : "=r"(tmp));
  //  printf("%llu\n",tmp);

  // 现在的sp指向的是GC上一层也就是alloc_record或者array_init的FP的下一个word
  // 也就是FP-8
  // 也就是说sp+8+tigermain_framesize(如果上一层是tigermain的话)就是上一层的FP了
  // sp本身对应的地址里面取出8byte是保存的rip的地址，也就是call的返回地址

  // sp + 1 代表的是加8byte
  // sp+8byte 代表的是metadata的地址 也就是char**
  sp++;
  std::string frame_metadata(*((char **)sp));
  //  printf("%p %s\n", *((char **)sp), frame_metadata.c_str());

  // mark & sweep
  for (int i = 0; i < heap_size; i++)
    mark[i] = false;
  // sp指向调用alloc的函数的底部，比如tigermain的stack_pointer

  // mark
  int length = frame_metadata.length();
  int tiger_main_size = length * 8;
  for (int i = 0; i < length; i++) {
    uint64_t value = *((uint64_t *)sp + length - (i + 1));
    if (frame_metadata[i] == '1') {
      // 可能是pointer
      if (value >= (uint64_t)heap && value < (uint64_t)heap + heap_size) {
        // 判断是record还是array
        uint64_t *p = (uint64_t *)value;
        //        printf("descriptor_tag=%llu size=%llu\n", p[-2], p[-1]);
        if (p[-2] == 1) {
          // array
          uint64_t size = p[-1];
          // 这里的范围跟descriptor对应上就行
          int from = value - (uint64_t)heap;
          //          printf("from = %d  [%d,%d]\n", from, from - 16,
          //                 from - 16 + (int)size);
          for (int j = from - 16; j < from - 16 + size; j++) {
            mark[j] = true;
          }
        } else if (p[-2] == 2) {
          // record 这里之前设置成tiger中的string类了
          // 放在record的指针前面8byte
          struct string *str = *((struct string **)value - 1);
          //          printf("%d %s\n", str->length, str->chars);
          int size = str->length * 8;
          int from = value - (uint64_t)heap;
          for (int j = from - 16; j < from + size; j++) {
            mark[j] = true;
          }
        }
      }
    }
  }

  // sweep
  // 先存下所有的块 (start, size)
  std::vector<std::pair<int, int>> v;
  int max_block = 0;
  for (int i = 0; i < heap_size; i++) {
    if (mark[i]) {
      // 被标记，free_list中的block中断
      if (max_block > 0) {
        // [i-1-max_block+1 , i-1]
        v.emplace_back(i - max_block, max_block);
        max_block = 0;
      }
    } else {
      // 没有被标记，可以回收
      max_block++;
    }
  }
  if (max_block > 0)
    v.emplace_back(heap_size - max_block, max_block);

  int num = v.size();
  // 先标记为没有
  free_list = heap_size;
  if (num > 0) {
    free_list = v[0].first;
  }
  for (int i = 0; i < num; i++) {
    int pre = i == 0 ? -1 : v[i - 1].first;
    int nxt = i == num - 1 ? heap_size : v[i + 1].first;
    node[v[i].first] = NODE(pre, nxt, v[i].second);
  }
}

} // namespace gc
