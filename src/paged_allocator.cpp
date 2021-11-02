#include "paged_allocator.h"

PagedAllocator::PagedAllocator(size_t page_size): page_size_(page_size) {
  head_ = new_page(4096);
}

PagedAllocator::~PagedAllocator() {
  clear();
  delete head_;
}

void* PagedAllocator::allocate(size_t size) {
  if (size > page_size_) {
    Page* page = new_page(size);
    page->next = head_;
    head_ = page;

    return page->memory;
  }

  if (head_->offset + size > head_->size) {
    Page* page = new_page(page_size_);
    page->next = head_;
    head_ = page;
  }

  void* ret = head_->memory + head_->offset;
  head_->offset += size;
  return ret;
}

void PagedAllocator::clear() {
  Page* cur = head_;
  while (cur->next) {
    Page* next = cur->next;
    delete[] cur->memory;
    delete cur;

    cur = next;
  }
  head_ = cur;
  head_->offset = 0;
}

PagedAllocator::Page* PagedAllocator::new_page(size_t size) {
  return new Page{
    .memory = new char[size],
    .offset = 0,
    .size = size
  };
}
