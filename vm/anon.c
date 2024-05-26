/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct bitmap *bitmap;
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);
struct lock anon_lock;
/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
    /* TODO: Set up the swap_disk. */
    swap_disk = disk_get(1, 1);
    bitmap = bitmap_create(disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE)); // 7560개
    lock_init(&anon_lock);
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    page->operations = &anon_ops;
    struct anon_page *anon_page = &page->anon;
    // anon_page->sec_no = -1;

    return true;
}

/* Swap in the page by read contents from the swap disk.
🐬 스왑 디스크 데이터 내용을 읽어서 익명 페이지를(디스크에서 메모리로) swap in합니다.
스왑 아웃 될 때 페이지 구조체는 스왑 디스크에 저장되어 있어야 합니다.
스왑 테이블을 업데이트해야 합니다(스왑 테이블 관리 참조).
*/
static bool anon_swap_in(struct page *page, void *kva)
{
    // printf("anony swap in!!!\n");
    struct anon_page *anon_page = &page->anon;
    disk_sector_t sec_no = anon_page->sec_no;
    // lock_acquire(&anon_lock);
    // // printf("sec_no : %d\n", sec_no);
    // if (!bitmap_test(bitmap, sec_no))
    //     return NULL;
    // lock_release(&anon_lock);

    for (int i = 0; i < 8; i++)
        disk_read(swap_disk, sec_no * 8 + i, kva + DISK_SECTOR_SIZE * i);

    lock_acquire(&anon_lock);
    bitmap_reset(bitmap, sec_no);
    lock_release(&anon_lock);

    return true;
}

/* Swap out the page by writing contents to the swap disk.
🐬 메모리에서 디스크로 내용을 복사하여 익명 페이지를 스왑 디스크로 교체합니다. 먼저 스왑 테이블을
사용하여 디스크에서 사용 가능한 스왑 슬롯을 찾은 다음 데이터 페이지를 슬롯에 복사합니다. 데이터의
위치는 페이지 구조체에 저장되어야 합니다. 디스크에 사용 가능한 슬롯이 더 이상 없으면 커널 패닉이
발생할 수 있습니다.
 */
static bool anon_swap_out(struct page *page)
{
    // printf("anony swap out!!!\n");
    struct anon_page *anon_page = &page->anon;
    // return true;
    lock_acquire(&anon_lock);
    size_t sec_num = bitmap_scan_and_flip(bitmap, 0, 1, 0);
    lock_release(&anon_lock);
    if (sec_num == BITMAP_ERROR)
        return false;
    disk_sector_t sec_no = sec_num;

    for (int i = 0; i < 8; i++)
    {
        disk_write(swap_disk, sec_no * 8 + i, page->frame->kva + DISK_SECTOR_SIZE * i);
        // printf("이게 아이다 %d\n", i);
        // printf("이게 섹넘이다 %d\n", sec_no * 8 + i);
        // printf("여기에 이만큼 쓴다 %p\n", page->va + DISK_SECTOR_SIZE * i);
        // printf("이게 페이지 va다 %p\n", page->va);
    }

    anon_page->sec_no = sec_no;
    page->frame = NULL;

    // pml4_clear_page(thread_current()->pml4, page->va);
    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    if (!anon_page->sec_no)
        bitmap_reset(bitmap, anon_page->sec_no);

    if (page->frame)
    {
        lock_acquire(&anon_lock);
        list_remove(&page->frame->f_elem);
        lock_release(&anon_lock);
        page->frame->page = NULL;
        free(page->frame);
        page->frame = NULL;
    }

    // pml4_clear_page(thread_current()->pml4, page->va);
}