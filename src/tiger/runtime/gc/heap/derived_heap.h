#pragma once

#include "heap.h"
#include <vector>

struct string {
  int length;
  unsigned char chars[1];
};

namespace gc {

class DerivedHeap : public TigerHeap {
  // TODO(lab7): You need to implement those interfaces inherited from TigerHeap
  // correctly according to your design.
  /**
   * Allocate a contiguous space from heap.
   * If your heap has enough space, just allocate and return.
   * If fails, return nullptr.
   * @param size size of the space allocated, in bytes.
   * @return start address of allocated space, if fail, return nullptr.
   */
  virtual char *Allocate(uint64_t size);

  /**
   * Acquire the total allocated space from heap.
   * Hint: If you implement a contigous heap, you could simply calculate the
   * distance between top and bottom, but if you implement in a link-list way,
   * you may need to sum the whole free list and calculate the total free space.
   * We will evaluate your work through this, make sure you implement it
   * correctly.
   * @return total size of the allocated space, in bytes.
   */
  virtual uint64_t Used() const;

  /**
   * Acquire the longest contiguous space from heap.
   * It should at least be 50% of inital heap size when initalization.
   * However, this may be tricky when you try to implement a Generatioal-GC, it
   * depends on how you set the ratio between Young Gen and Old Gen, we suggest
   * you making sure Old Gen is larger than Young Gen. Hint: If you implement a
   * contigous heap, you could simply calculate the distance between top and
   * end, if you implement in a link-list way, you may need to find the biggest
   * free list and return it. We use this to evaluate your utilzation, make sure
   * you implement it correctly.
   * @return size of the longest , in bytes.
   */
  virtual uint64_t MaxFree() const;

  /**
   * Initialize your heap.
   * @param size size of your heap in bytes.
   */
  virtual void Initialize(uint64_t size);

  /**
   * Do Garbage collection!
   * Hint: Though we do not suggest you implementing a Generational-GC due to
   * limited time, if you are willing to try it, add a function named YoungGC,
   * this will be treated as FullGC by default.
   */
  virtual void GC();

  ~DerivedHeap();

  int heap_size;
  uint8_t *heap;
  bool *mark;

  struct NODE {
    int prev, next, size;
    NODE(int prev, int next, int size) : prev(prev), next(next), size(size) {}
    NODE() : prev(-1), next(-1), size(-1) {}
  };
  int free_list;
  NODE *node;

};

} // namespace gc
