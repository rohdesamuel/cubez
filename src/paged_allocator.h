#ifndef PAGED_ALLOCATOR__H
#define PAGED_ALLOCATOR__H

class PagedAllocator {
public:
  struct Page {
    char* memory = nullptr;
    size_t offset = 0;
    size_t size = 0;

    struct Page* next = nullptr;
  };

  PagedAllocator(size_t page_size);
  ~PagedAllocator();

  void* allocate(size_t size);
  void clear();

private:
  Page* new_page(size_t size);

  Page* head_;
  size_t page_size_;
};

#endif  // PAGED_ALLOCATOR__H