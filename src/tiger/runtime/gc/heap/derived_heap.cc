#include "derived_heap.h"
#include <cstring>
#include <stack>
#include <stdio.h>

namespace gc {
// TODO(lab7): You need to implement those interfaces inherited from TigerHeap
// correctly according to your design.

void DerivedHeap::Initialize(uint64_t size) {}

char *DerivedHeap::Allocate(uint64_t size) {}

uint64_t DerivedHeap::Used() const {}

uint64_t DerivedHeap::MaxFree() const {}

void DerivedHeap::GC() {}

} // namespace gc
