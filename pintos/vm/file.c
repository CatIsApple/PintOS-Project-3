/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include "vm/uninit.h"
#include <string.h>

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct load_aux *aux = page->uninit.aux;
	if (aux == NULL) return false;
	struct file_page *file_page = &page->file;

	file_page->file = aux->file;
	file_page->offset = aux->ofs;
	file_page->read_bytes = aux->read_bytes;
	file_page->length = aux->length;
	
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	uint64_t *pml4 = thread_current()->pml4;
	if(pml4_is_dirty(pml4, page->va)){
		file_write_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->offset);
	}
	pml4_set_dirty(pml4, page->va, false);
	memset(page->frame->kva, 0, file_page->read_bytes);	
	pml4_clear_page(pml4, page->va);
}

// void *
// do_mmap (void *addr, size_t length, int writable,
// 	struct file *file, off_t offset) {
// 	//file의 offset을 시작으로 lengthzeof(struct load_aux));
// 	// 인자 검증 → 매핑용 파일 확보 → 페이지별 aux 생성 → UNINIT 등록
	
// 	if (addr == NULL || length == 0 || file == NULL) {
// 		printf("MMAP: #0 return NULL\n");
// 		return NULL;
// 	}

// 	if(!is_user_vaddr(addr)){
// 		printf("MMAP: #1 return NULL\n");
// 		return NULL;
// 	}

// 	uint32_t read_bytes = ;
// 	uint32_t zero_bytes;

// 	while (read_bytes > 0 || zero_bytes > 0) {

// 		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
// 		size_t page_zero_bytes = PGSIZE - page_read_bytes;

// 		struct load_aux *aux = malloc(sizeof(struct load_aux));
// 		aux->file = file;
// 		aux->ofs = offset;
// 		aux->length = length;
// 		aux->read_bytes = page_read_bytes;
// 		aux->zero_bytes = page_zero_bytes;

// 		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
// 					writable, lazy_load_segment, aux)){
// 						free(aux);
// 						return false;
// 					}

// 		/* Advance. */
// 		read_bytes -= page_read_bytes;
// 		zero_bytes -= page_zero_bytes;
// 		offset += PGSIZE;
// 		addr += PGSIZE;
// 	}

// }

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
	struct file *file, off_t offset) {
	//file의 offset을 시작으로 lengthzeof(struct load_aux));
	// 인자 검증 → 매핑용 파일 확보 → 페이지별 aux 생성 → UNINIT 등록
	
	if (addr == NULL || length == 0 || file == NULL) {
		// printf("MMAP: #0 return NULL\n");
		return NULL;
	}
	if (!is_user_vaddr(addr) ||
			length > (uintptr_t)KERN_BASE - (uintptr_t)addr) {
			return NULL;
	}

	int page_cnt = length %  PGSIZE > 0 ? length / PGSIZE + 1 : length / PGSIZE;
  size_t length_cpy = length;
	void *addr_cpy = addr;
	uint32_t read_bytes = file_length(file);
	file = file_reopen(file);
	if (file == NULL)
		return NULL;
  for (int i = 0; i < page_cnt; i++)
  {
    struct load_aux *aux = malloc(sizeof(struct load_aux));
		if(aux == NULL) return NULL;
    aux->file = file;
    aux->ofs = offset;
    aux->length = length;

		// length            = 4096  → 요청한 전체 매핑 크기
		// file_length(file) = 794   → 실제 파일 크기
		// read_bytes        = 794   → 파일에서 읽을 양
		// zero_bytes        = 3302  → 나머지를 0으로 채울 양

		// printf("length_cpy: %d\n", length_cpy);

		if (read_bytes >= PGSIZE) {
				aux->read_bytes = PGSIZE;
				aux->zero_bytes = 0;
				read_bytes -= PGSIZE;
		} else {
				aux->read_bytes = read_bytes;
				aux->zero_bytes = PGSIZE - read_bytes;
				read_bytes = 0;
		}
		
		// printf("file: %p, ofs: %d, length: %d, read_bytes: %d, zero_bytes: %lu\n", aux->file, aux->ofs, aux->length, aux->read_bytes, aux->zero_bytes);

		if (!vm_alloc_page_with_initializer (VM_FILE, addr_cpy, 
			writable, lazy_load_segment, aux)){
			free(aux);
			// printf("MMAP: #3 return NULL\n");
			return NULL;
		}

		addr_cpy = (char *)addr_cpy + PGSIZE;
    offset += PGSIZE;
  }
	// printf("MMAP: return addr: %p\n", addr);
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *thread = thread_current();
	struct supplemental_page_table *spt = &thread->spt;
	uint64_t *pml4 = thread->pml4;
	struct page *page = spt_find_page(spt, addr);
	
	if(page == NULL){
		// printf("ERROR #19 FAILED\n");
		exit(-1);
	}
	
  size_t length = 0;

	struct file *file_;
	
  if (page->operations->type == VM_FILE) {
    length = page->file.length;
		file_ = page->file.file;
  }
  else if (page->operations->type == VM_UNINIT) {
    if(VM_TYPE(page->uninit.type) == VM_FILE)
    {
		  struct load_aux *aux = (struct load_aux *)page->uninit.aux;
		  length = aux->length;
			file_ = aux->file;
    }
  }
  else {
    return;
  }
	
	int page_cnt = length %  PGSIZE > 0 ? length / PGSIZE + 1 : length / PGSIZE;
	for (int i = 0; i < page_cnt; i++)
	{
		page = spt_find_page(spt, addr);
    if (page == NULL) {
			// printf("ERROR #20 FAILED\n");
			exit(-1);
		}
		hash_delete(&spt->hash, &page->hash_elem);
		vm_dealloc_page(page);
		addr = (char *)addr + PGSIZE;
	}
	file_close(file_);
}
