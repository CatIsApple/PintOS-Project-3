/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "userprog/syscall.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "vm/uninit.h"
#include "threads/vaddr.h"



/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
struct list frame_table;
struct lock frame_table_lock;

void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
	frame_init();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)
	// printf("[vm_alloc_page_with_initializer] START\n");
	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	struct page *page = malloc(sizeof(struct page));
	struct page *find_page = spt_find_page (spt, upage);
	
	if (find_page == NULL) {	
		// printf("[vm_alloc_page_with_initializer] TRY VM_TYPE case.\n");
		switch (VM_TYPE(type)) // VM_ANON, VM_FILE을 제외한 나머지는 전부 uninit_으로 처리.
		{
			case VM_ANON:
				uninit_new(page, upage, writable, init, VM_ANON, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(page, upage, writable, init, VM_FILE, aux, file_backed_initializer);
				break;
			default:
				// printf("[vm_alloc_page_with_initializer] FAIL switch (VM_TYPE(type)). jump err;\n");
				free(page);
				goto err;
		}

		// printf("[vm_alloc_page_with_initializer] TRY spt_insert_page.\n");
		if(!spt_insert_page(spt, page)) {
			// printf("[vm_alloc_page_with_initializer] FAIL spt_insert_page. jump err;\n");
			free(page);
			goto err;
		} // init된 page를 spt에 삽입.

		// printf("[vm_alloc_page_with_initializer] SUCCESS return : true\n");
		return true;
	}
	// printf("[vm_alloc_page_with_initializer] FAIL spt_find_page spt : %p, upage : %p\n", spt, upage);
	// printf("[vm_alloc_page_with_initializer] FAIL spt_find_page va : %p\n", find_page->va);
err:
	// printf("[vm_alloc_page_with_initializer] FAIL return : false\n");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	// printf("[spt_find_page] TRY spt_find_page();\n");
	// printf("[spt_find_page] RETURN va : %p\n", va);

	struct page *page = malloc(sizeof(struct page)); 
	page->va = pg_round_down(va);

	// printf("[spt_find_page] RETURN page->va : %p\n", page.va);
	// printf("[spt_find_page] RETURN &spt->hash : %p\n", &spt->hash);
	struct hash_elem *find_entry = hash_find(&spt->hash, &page->hash_elem);
	// printf("[spt_find_page] RETURN find_entry : %p\n", find_entry);

	if(find_entry != NULL){
		free(page);
		// printf("hash_entry(find_entry, struct page, hash_elem) : %p\n", hash_entry(find_entry, struct page, hash_elem));
		// printf("(struct page *)hash_entry(find_entry, struct page, hash_elem) : %p\n", (struct page *)hash_entry(find_entry, struct page, hash_elem));
		return hash_entry(find_entry, struct page, hash_elem);
	}
	return NULL;
}

// struct page *
// spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
// 	struct page *page = malloc(sizeof(struct page));
// 	struct hash_elem *e;
// 	if(page == NULL){
// 		return NULL;
// 	}

// 	page->va = pg_round_down(va);
// 	e = hash_find(&spt->hash_table, &page->hash_elem);
	
// 	if(e != NULL){
// 		struct page *found_page =  hash_entry(e, struct page, hash_elem);
// 		free(page);
// 		return found_page;
// 	}
// 	else{
// 		free(page);
// 		return NULL;
// 	}
// }

// struct page *
// spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
// 	struct page *page = NULL;
// 	page->va = pg_round_down(va);
// }

/* Insert PAGE into spt with validation. */
// bool
// spt_insert_page (struct supplemental_page_table *spt UNUSED,f
// 		struct page *page UNUSED) {
// 	bool succ = false;
// 	/* TODO: Fill this function. */
// 	if (spt == NULL || page == NULL) return false;
	
// 	if (spt_find_page(spt, page->va) == NULL)
// 	{
// 		if (hash_insert(&spt->hash, &page->hash_elem) != NULL)
// 			succ = true;
// 	}

// 	return succ;
// }

bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
		// printf("[spt_insert_page] TRY spt_insert_page();\n");
    int succ = false;
    
		if (spt == NULL || page == NULL) return false;

    struct hash_elem *old_page = hash_insert(&spt->hash, &page->hash_elem);
    
    if (old_page == NULL) {
        succ = true;
    }

		// printf("[spt_insert_page] RESULT %s\n", succ ? "success" : "failed");
    
    return succ;
}

// bool
// spt_insert_page (struct supplemental_page_table *spt, struct page *page) 
// {
//     int succ = false;
//     struct hash_elem *old_page = hash_insert(&spt->hash, &page->hash_elem);
//     if (!old_page){
//         succ = true; 
//     }
//     return succ;
// }

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);
	ASSERT (frame != NULL);

	if (frame->kva == NULL) { // frame에서 가용 가능한 페이지가 없다면

		free(frame);
		PANIC("todo");
	}
	frame->page == NULL;
	// frame_insert(frame);
	return frame;
}

/* Growing the stack. */
static bool
vm_stack_growth (void *addr UNUSED) {
	void *va = pg_round_down(addr);
	if(!vm_alloc_page(VM_ANON, va, true)) return false;
	return vm_claim_page(va);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
// bool
// vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
// 	// return vm_claim_page (addr)
// 	// printf ("[page_fault_try] vm_try_handle_fault START.\n");
// 	struct supplemental_page_table *spt = &thread_current ()->spt;
// 		if (not_present) {
// 			// printf("[page_fault_try] FAIL !user || !write || not_present\n");



// 		}
// 	if(spt == NULL){
// 		// printf("[page_fault_try] FAIL spt == NULL\n");
// 		return false;
// 	}
// 	if(addr == NULL){
// 		// printf("[page_fault_try] FAIL addr == NULL\n");
// 		return false;
// 	}
// 	struct page *page = spt_find_page(spt, addr);
	
// 	// printf("spt_find_page va : %s", page ? "in_value" : "NULL" );

// 	if(page == NULL){
// 		// printf("[page_fault_try] page == NULL\n");
// 		return false;
// 	}

// 	// printf ("[page_fault_try] vm_try_handle_fault return\n");
// 	return vm_do_claim_page (page);
// 	// return true;
// }

bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	if (!not_present || addr ==NULL) return false;
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, addr);
	if (!page)
	{
		if (user)
		{
			if ((uintptr_t) addr < USER_STACK && (uintptr_t) addr >= (USER_STACK - (1UL << 20)) && (uintptr_t) addr >= (f->rsp) - 8)
			{
				return vm_stack_growth(addr);
			}
			else return false;
		}
		else
		{
			if ((uintptr_t) addr < USER_STACK && (uintptr_t) addr >= (USER_STACK - (1UL << 20)) && (uintptr_t) addr >= (thread_current()->rsp) - 8)
			{
				return vm_stack_growth(addr);
			}
			else return false;
		}
	}
	
	return vm_claim_page (addr);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, va);
	
	if(page == NULL){
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if(page == NULL && frame == NULL){
		return false;
	}
	/* Set links */
	frame->page = page; // 물리 프레임 테이블 안에 있는 frame에 page 연결
	page->frame = frame; // spt 내에 있는 page에 물리 프레임인 frame을 연결

	struct thread *curr = thread_current();

	if(!pml4_set_page(curr->pml4, page->va, frame->kva, page->writable)){
		vm_dealloc_page(page);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	// printf("[page_hash] TRY page_hash();\n");
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}
/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
	// printf("[page_less] TRY page_less();\n");
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);
	// printf("[page_less] RESULT a : %p, b : %p, a->va %s b->va\n", a->va, b->va, a->va < b->va ? "<" : ">");
  return a->va < b->va;
}

void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// struct hash *pages;
	if(!hash_init(&spt->hash, page_hash, page_less, NULL)) {
		return;
	}
	// spt->hash = pages;
}

hash_action_func * spt_copy_page(struct hash_iterator* i, struct supplemental_page_table *dst)
{
		struct hash_elem *e = hash_cur(&i);
		struct page *page = hash_entry(e, struct page, hash_elem);
		if (page->frame != NULL) {
			void *aux = malloc(sizeof(page->uninit.aux));
			aux = memcpy(aux, page->uninit.aux, sizeof(page->uninit.aux));
			vm_alloc_page(page_get_type(page), page->va, true);
			struct page *find_page = spt_find_page(dst, page->va);
			if (!vm_claim_page(find_page->va)) {
				return false;
			}
			page->frame->kva = memcpy(find_page->frame->kva, page->frame->kva, sizeof(struct frame));
		}
		else
		{
			void *aux = malloc(sizeof(page->uninit.aux));
			aux = memcpy(aux, page->uninit.aux, sizeof(page->uninit.aux));
			vm_alloc_page_with_initializer(page_get_type(page), page->va, page->writable, page->uninit.init, aux);
		}
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	struct hash_iterator i;
	hash_first(&i, &src->hash); 
	hash_apply(&src->hash, spt_copy_page(&i, dst));
}

hash_action_func * spt_destroy_page(struct hash_elem *e, void *aux UNUSED)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(page);
}

/* Free the resource hold by the supplemental page table */

/* 
	TODO: 스레드가 보유한 모든 supplemental_page_table을 파괴하고,
	TODO: 수정된 모든 내용을 저장소에 다시 기록(writeback)한다.
*/
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	struct hash_iterator i;
	hash_first(&i, &spt->hash); 
	hash_apply(&spt->hash, spt_destroy_page(&i, spt));
}

void frame_init() // 여기도 끝난거같은디염
{
  //frame_table = malloc(sizeof(struct list) * (void *));
  list_init(&frame_table);
  lock_init(&frame_table_lock);
  // lock_init 필요
  //return &frame_table;
}

bool frame_insert(struct frame *f)
{
  if (f == NULL) return false;

  lock_acquire(&frame_table_lock);
  list_push_back(&frame_table, &f->elem);
  lock_release(&frame_table_lock);
  return true;
}

bool frame_delete(struct frame *f)
{
  if (f == NULL) return false;

  lock_acquire(&frame_table_lock);
  list_remove(&f->elem);
  lock_release(&frame_table_lock);
  palloc_free_page(f->kva);
  free(f);
  return true;
}
