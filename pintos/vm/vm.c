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

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	struct page *page = malloc(sizeof(struct page));
	if (spt_find_page (spt, upage) == NULL) {	
		switch (VM_TYPE(type)) // VM_ANON, VM_FILE을 제외한 나머지는 전부 uninit_으로 처리.
		{
			case VM_ANON:
				uninit_new(page, upage, writable, init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(page, upage, writable, init, type, aux, file_backed_initializer);
				break;
		}
		if(spt_insert_page(spt, page)) return true; // init된 page를 spt에 삽입.
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	page->va = pg_round_down(va);

	return hash_entry(hash_find(&spt->hash, &page->hash_elem), struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	bool succ = false;
	/* TODO: Fill this function. */
	if (spt == NULL || page == NULL) return false;
	
	if (spt_find_page(spt, page->va) == NULL)
	{
		if (hash_insert(&spt->hash, &page->hash_elem) == NULL)
			succ = true;
	}

	return succ;
}

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
	struct frame *frame = NULL;
	frame->kva = palloc_get_page(PAL_USER) + KERN_BASE;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	frame_insert(frame);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
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
		exit(-1);
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page; // 물리 프레임 테이블 안에 있는 frame에 page 연결
	page->frame = frame; // spt 내에 있는 page에 물리 프레임인 frame을 연결

	struct thread *curr = thread_current();

	if(!pml4_set_page(curr->pml4, page, frame, page->writable)){
		exit(-1);
	}

	return swap_in (page, frame->kva);
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}
/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	struct hash *pages;
	if(!hash_init (pages, page_hash, page_less, NULL)) exit(-1);
	spt->hash = pages;
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */  
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
