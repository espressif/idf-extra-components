/* Dhara - NAND flash management layer
 * Copyright (C) 2013 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include "journal.h"
#include "bytes.h"

/************************************************************************
 * Metapage binary format
 */

/* Does the page buffer contain a valid checkpoint page? */
static inline int hdr_has_magic(const uint8_t *buf)
{
    return (buf[0] == 'D') &&
           (buf[1] == 'h') &&
           (buf[2] == 'a');
}

static inline void hdr_put_magic(uint8_t *buf)
{
    buf[0] = 'D';
    buf[1] = 'h';
    buf[2] = 'a';
}

/* What epoch is this page? */
static inline uint8_t hdr_get_epoch(const uint8_t *buf)
{
    return buf[3];
}

static inline void hdr_set_epoch(uint8_t *buf, uint8_t e)
{
    buf[3] = e;
}

static inline dhara_page_t hdr_get_tail(const uint8_t *buf)
{
    return dhara_r32(buf + 4);
}

static inline void hdr_set_tail(uint8_t *buf, dhara_page_t tail)
{
    dhara_w32(buf + 4, tail);
}

static inline dhara_page_t hdr_get_bb_current(const uint8_t *buf)
{
    return dhara_r32(buf + 8);
}

static inline void hdr_set_bb_current(uint8_t *buf, dhara_page_t count)
{
    dhara_w32(buf + 8, count);
}

static inline dhara_page_t hdr_get_bb_last(const uint8_t *buf)
{
    return dhara_r32(buf + 12);
}

static inline void hdr_set_bb_last(uint8_t *buf, dhara_page_t count)
{
    dhara_w32(buf + 12, count);
}

/* Clear user metadata */
static inline void hdr_clear_user(uint8_t *buf, uint8_t log2_page_size)
{
    const size_t page_size = (size_t)1 << log2_page_size;
    const size_t reserved = DHARA_HEADER_SIZE + DHARA_COOKIE_SIZE;

    /* Every dhara journal page must be large enough to hold the
     * checkpoint header + cookie (a precondition that already applies
     * elsewhere, e.g. choose_ppc()'s max_meta computation). If
     * page_size were ever smaller than that (misconfigured NAND
     * geometry), the unsigned subtraction below would underflow to a
     * huge length and memset() would write far past the end of buf.
     * Clamp defensively instead: this is a no-op for every real NAND
     * page size (always >> 20 bytes in practice).
     */
    if (page_size <= reserved) {
        return;
    }

    memset(buf + reserved, 0xff, page_size - reserved);
}

/* Obtain pointers to user data */
static inline size_t hdr_user_offset(uint8_t which)
{
    return DHARA_HEADER_SIZE + DHARA_COOKIE_SIZE +
           which * DHARA_META_SIZE;
}

/************************************************************************
 * Page geometry helpers
 */

/* Is this page index aligned to N bits? */
static inline int is_aligned(dhara_page_t p, int n)
{
    return !(p & ((1 << n) - 1));
}

/* Are these two pages from the same alignment group? */
static inline int align_eq(dhara_page_t a, dhara_page_t b,
                           int n)
{
    return !((a ^ b) >> n);
}

/* What is the successor of this block? */
static dhara_block_t next_block(const struct dhara_nand *n, dhara_block_t blk)
{
    blk++;
    if (blk >= n->num_blocks) {
        blk = 0;
    }

    return blk;
}

static dhara_page_t next_upage(const struct dhara_journal *j,
                               dhara_page_t p)
{
    p++;
    if (is_aligned(p + 1, j->log2_ppc)) {
        p++;
    }

    if (p >= (j->nand->num_blocks << j->nand->log2_ppb)) {
        p = 0;
    }

    return p;
}

/* Calculate a checkpoint period: the largest value of ppc such that
 * (2**ppc - 1) metadata blocks can fit on a page with one journal
 * header.
 *
 * Note: this can legitimately return `max` itself (not just values
 * strictly below it). `max` is always the block's log2_ppb, so
 * ppc == max means "one checkpoint group per block" -- a degenerate
 * but valid configuration, not an overflow. All call sites in this
 * file rely on the resulting invariant log2_ppc <= log2_ppb (e.g.
 * dhara_journal_capacity()'s good_cps shift), which holds whether or
 * not the loop reaches `max`.
 */
static int choose_ppc(int log2_page_size, int max)
{
    const int max_meta = (1 << log2_page_size) -
                         DHARA_HEADER_SIZE - DHARA_COOKIE_SIZE;
    int total_meta = DHARA_META_SIZE;
    int ppc = 1;

    while (ppc < max) {
        total_meta <<= 1;
        total_meta += DHARA_META_SIZE;

        if (total_meta > max_meta) {
            break;
        }

        ppc++;
    }

    return ppc;
}

/************************************************************************
 * Journal setup/resume
 */

/* Clear recovery status */
static void clear_recovery(struct dhara_journal *j)
{
    j->recover_next = DHARA_PAGE_NONE;
    j->recover_root = DHARA_PAGE_NONE;
    j->recover_meta = DHARA_PAGE_NONE;
    j->flags &= ~(DHARA_JOURNAL_F_BAD_META |
                  DHARA_JOURNAL_F_RECOVERY |
                  DHARA_JOURNAL_F_ENUM_DONE);
}

/* Set up an empty journal */
static void reset_journal(struct dhara_journal *j)
{
    /* We don't yet have a bad block estimate, so make a
     * conservative guess.
     */
    j->epoch = 0;
    j->bb_last = j->nand->num_blocks >> 6;
    j->bb_current = 0;

    j->flags = 0;

    /* Empty journal */
    j->head = 0;
    j->tail = 0;
    j->tail_sync = 0;
    j->root = DHARA_PAGE_NONE;

    /* No recovery required */
    clear_recovery(j);

    /* Empty metadata buffer */
    memset(j->page_buf, 0xff, 1 << j->nand->log2_page_size);
}

static void roll_stats(struct dhara_journal *j)
{
    j->bb_last = j->bb_current;
    j->bb_current = 0;
    j->epoch++;
}

void dhara_journal_init(struct dhara_journal *j,
                        const struct dhara_nand *n,
                        uint8_t *page_buf)
{
    /* Set fixed parameters */
    j->nand = n;
    j->page_buf = page_buf;
    j->log2_ppc = choose_ppc(n->log2_page_size, n->log2_ppb);

    reset_journal(j);
}

/* Find the first checkpoint-containing block. If a block contains any
 * checkpoints at all, then it must contain one in the first checkpoint
 * location -- otherwise, we would have considered the block eraseable.
 *
 * Note: the retry counter `i` is incremented for every block examined,
 * including bad ones -- bad blocks are NOT exempt from the
 * DHARA_MAX_RETRIES budget. This is intentional and consistent with
 * every other DHARA_MAX_RETRIES loop in this file (prepare_head(),
 * dump_meta(), dhara_journal_peek(), dhara_journal_enqueue(),
 * dhara_journal_copy()): DHARA_MAX_RETRIES bounds how many consecutive
 * bad blocks we are willing to tolerate before giving up, so that
 * pathological runs of bad blocks can't turn an O(log N) search into
 * an unbounded scan. It is not meant to "skip bad blocks for free".
 */
static int find_checkblock(struct dhara_journal *j,
                           dhara_block_t blk, dhara_block_t *where,
                           dhara_error_t *err)
{
    int i;

    for (i = 0; (blk < j->nand->num_blocks) &&
            (i < DHARA_MAX_RETRIES); i++) {
        const dhara_page_t p =
            (blk << j->nand->log2_ppb) |
            ((1 << j->log2_ppc) - 1);

        if (!(dhara_nand_is_bad(j->nand, blk) ||
                dhara_nand_read(j->nand, p,
                                0, 1 << j->nand->log2_page_size,
                                j->page_buf, err)) &&
                hdr_has_magic(j->page_buf)) {
            *where = blk;
            return 0;
        }

        blk++;
    }

    dhara_set_error(err, DHARA_E_TOO_BAD);
    return -1;
}

static dhara_block_t find_last_checkblock(struct dhara_journal *j,
        dhara_block_t first)
{
    dhara_block_t low = first;
    dhara_block_t high = j->nand->num_blocks - 1;

    while (low <= high) {
        const dhara_block_t mid = (low + high) >> 1;
        dhara_block_t found;

        if ((find_checkblock(j, mid, &found, NULL) < 0) ||
                (hdr_get_epoch(j->page_buf) != j->epoch)) {
            if (!mid) {
                return first;
            }

            high = mid - 1;
        } else {
            dhara_block_t nf;

            if (((found + 1) >= j->nand->num_blocks) ||
                    (find_checkblock(j, found + 1,
                                     &nf, NULL) < 0) ||
                    (hdr_get_epoch(j->page_buf) != j->epoch)) {
                return found;
            }

            low = nf;
        }
    }

    return first;
}

/* Test whether a checkpoint group is in a state fit for reprogramming,
 * but allow for the fact that is_free() might not have any way of
 * distinguishing between an unprogrammed page, and a page programmed
 * with all-0xff bytes (but if so, it must be ok to reprogram such a
 * page).
 *
 * We used to test for an unprogrammed checkpoint group by checking to
 * see if the first user-page had been programmed since last erase (by
 * testing only the first page with is_free). This works if is_free is
 * precise, because the pages are written in order.
 *
 * If is_free is imprecise, we need to check all pages in the group.
 * That also works, because the final page in a checkpoint group is
 * guaranteed to contain non-0xff bytes. Therefore, we return 1 only if
 * the group is truly unprogrammed, or if it was partially programmed
 * with some all-0xff user pages (which changes nothing for us).
 */
static int cp_free(const struct dhara_journal *j, dhara_page_t first_user)
{
    const int count = 1 << j->log2_ppc;
    int i;

    for (i = 0; i < count; i++)
        if (!dhara_nand_is_free(j->nand, first_user + i)) {
            return 0;
        }

    return 1;
}

static dhara_page_t find_last_group(struct dhara_journal *j,
                                    dhara_block_t blk)
{
    const int num_groups = 1 << (j->nand->log2_ppb - j->log2_ppc);
    int low = 0;
    int high = num_groups - 1;

    /* If a checkpoint group is completely unprogrammed, everything
     * following it will be completely unprogrammed also.
     *
     * Therefore, binary search checkpoint groups until we find the
     * last programmed one.
     */
    while (low <= high) {
        int mid = (low + high) >> 1;
        const dhara_page_t p = (mid << j->log2_ppc) |
                               (blk << j->nand->log2_ppb);

        if (cp_free(j, p)) {
            high = mid - 1;
        } else if (((mid + 1) >= num_groups) ||
                   cp_free(j, p + (1 << j->log2_ppc))) {
            return p;
        } else {
            low = mid + 1;
        }
    }

    /* Precondition (guaranteed by the only call site, in
     * dhara_journal_resume(): `blk` was already validated by
     * find_checkblock()/find_last_checkblock() to have a valid
     * checkpoint magic at group 0's last page): group 0 in `blk` is
     * always programmed, so this fallback is never actually reached
     * in practice -- cp_free() will report group 0 as non-free before
     * the search can conclude "everything is free". It is kept as a
     * defensive fallback; if it were ever reached, `blk << log2_ppb`
     * is exactly the start of group 0, which is the correct answer
     * when group 0 is the only programmed group.
     */
    return blk << j->nand->log2_ppb;
}

static int find_root(struct dhara_journal *j, dhara_page_t start,
                     dhara_error_t *err)
{
    const dhara_block_t blk = start >> j->nand->log2_ppb;
    int i = (start & ((1 << j->nand->log2_ppb) - 1)) >> j->log2_ppc;

    while (i >= 0) {
        const dhara_page_t p = (blk << j->nand->log2_ppb) +
                               ((i + 1) << j->log2_ppc) - 1;

        if (!dhara_nand_read(j->nand, p,
                             0, 1 << j->nand->log2_page_size,
                             j->page_buf, err) &&
                (hdr_has_magic(j->page_buf)) &&
                (hdr_get_epoch(j->page_buf) == j->epoch)) {
            /* p is the checkpoint header page (the LAST page of this
             * checkpoint group); j->root ("the last written user
             * page") is therefore the page immediately before it, not
             * the first page of the group. This matches push_meta(),
             * which likewise sets j->root = old_head (the page just
             * written, i.e. header_page - 1) right before programming
             * the header at header_page. Verified against a
             * fm_agent-generated report that inferred root should be
             * (first_checkpoint_page - 1) instead -- that reading
             * would point root at a page in the *previous* checkpoint
             * group (potentially its own header page), contradicting
             * both push_meta()'s behaviour and root's documented
             * meaning.
             */
            j->root = p - 1;
            return 0;
        }

        i--;
    }

    dhara_set_error(err, DHARA_E_TOO_BAD);
    return -1;
}

/* Locate the next free user page after the last good checkpoint.
 *
 * Always returns 0: the scan uses dhara_nand_is_free() only as an
 * informational probe and treats wrapping onto the next block as
 * success, so there is no I/O error path. `err` is unused and is
 * retained only to match find_root()'s signature.
 */
static int find_head(struct dhara_journal *j, dhara_page_t start,
                     dhara_error_t *err)
{
    (void)err;
    j->head = next_upage(j, start);
    if (!j->head) {
        roll_stats(j);
    }

    /* Starting from the last good checkpoint, find either:
     *
     *   (a) the next free user-page in the same block
     *   (b) or, the first page of the next block
     *
     * The block we end up on might be bad, but that's ok -- we'll
     * skip it when we go to prepare the next write.
     */
    for (;;) {
        /* How many free pages trail this checkpoint group? */
        const unsigned int ppc = 1 << j->log2_ppc;
        unsigned int n = 0;
        dhara_page_t first = j->head & ~(dhara_page_t)(ppc - 1);

        while (n < ppc &&
                dhara_nand_is_free(j->nand, first + ppc - n - 1)) {
            n++;
        }

        /* If we have some, then we've found our next free
         * userpage.
         */
        if (n > 1) {
            j->head = first + ppc - n;
            break;
        }

        /* Skip to the next checkpoint group */
        j->head = first + ppc;
        if (j->head >= (j->nand->num_blocks << j->nand->log2_ppb)) {
            j->head = 0;
            roll_stats(j);
        }

        /* If we hit the end of the block, we're done */
        if (is_aligned(j->head, j->nand->log2_ppb)) {
            /* Make sure we don't chase over the tail */
            if (align_eq(j->head, j->tail, j->nand->log2_ppb))
                j->tail = next_block(j->nand,
                                     j->tail >> j->nand->log2_ppb) <<
                          j->nand->log2_ppb;
            break;
        }
    }

    return 0;
}

int dhara_journal_resume(struct dhara_journal *j, dhara_error_t *err)
{
    dhara_block_t first, last;
    dhara_page_t last_group;

    /* Find the first checkpoint-containing block */
    if (find_checkblock(j, 0, &first, err) < 0) {
        reset_journal(j);
        return -1;
    }

    /* Find the last checkpoint-containing block in this epoch */
    j->epoch = hdr_get_epoch(j->page_buf);
    last = find_last_checkblock(j, first);

    /* Find the last programmed checkpoint group in the block */
    last_group = find_last_group(j, last);

    /* Perform a linear scan to find the last good checkpoint (and
     * therefore the root).
     */
    if (find_root(j, last_group, err) < 0) {
        reset_journal(j);
        return -1;
    }

    /* Restore settings from checkpoint */
    j->tail = hdr_get_tail(j->page_buf);
    j->bb_current = hdr_get_bb_current(j->page_buf);
    j->bb_last = hdr_get_bb_last(j->page_buf);
    hdr_clear_user(j->page_buf, j->nand->log2_page_size);

    /* Perform another linear scan to find the next free user page.
     * find_head() currently cannot fail (always returns 0, never writes
     * *err), so this < 0 check is always false. It is kept as a
     * defensive counterpart to find_root() above so a future fallible
     * find_head() still resets the journal.
     */
    if (find_head(j, last_group, err) < 0) {
        reset_journal(j);
        return -1;
    }

    j->flags = 0;
    j->tail_sync = j->tail;

    clear_recovery(j);
    return 0;
}

/**************************************************************************
 * Public interface
 */

dhara_page_t dhara_journal_capacity(const struct dhara_journal *j)
{
    const dhara_block_t max_bad = j->bb_last > j->bb_current ?
                                  j->bb_last : j->bb_current;
    const dhara_block_t good_blocks = j->nand->num_blocks - max_bad - 1;
    /* log2_ppc <= log2_ppb is a class invariant established by
     * choose_ppc() in dhara_journal_init() and relied upon throughout
     * this file. Clamp defensively so that this can never become a
     * shift by a negative amount (undefined behaviour in C) if that
     * invariant is ever violated (e.g. j->log2_ppc corrupted/mutated
     * directly). This is a no-op for every valid journal.
     */
    const int log2_cpb = j->nand->log2_ppb > j->log2_ppc ?
                         j->nand->log2_ppb - j->log2_ppc : 0;
    const dhara_page_t good_cps = good_blocks << log2_cpb;

    /* Good checkpoints * (checkpoint period - 1) */
    return (good_cps << j->log2_ppc) - good_cps;
}

dhara_page_t dhara_journal_size(const struct dhara_journal *j)
{
    /* Find the number of raw pages, and the number of checkpoints
     * between the head and the tail. The difference between the two
     * is the number of user pages (upper limit).
     */
    dhara_page_t num_pages = j->head;
    dhara_page_t num_cps = j->head >> j->log2_ppc;

    if (j->head < j->tail_sync) {
        const dhara_page_t total_pages =
            j->nand->num_blocks << j->nand->log2_ppb;

        num_pages += total_pages;
        num_cps += total_pages >> j->log2_ppc;
    }

    num_pages -= j->tail_sync;
    num_cps -= j->tail_sync >> j->log2_ppc;

    return num_pages - num_cps;
}

int dhara_journal_read_meta(const struct dhara_journal *j, dhara_page_t p,
                            uint8_t *buf, dhara_error_t *err)
{
    /* Offset of metadata within the metadata page */
    const dhara_page_t ppc_mask = (1 << j->log2_ppc) - 1;
    const size_t offset = hdr_user_offset(p & ppc_mask);

    /* Special case: buffered metadata. `p` is a user page (API
     * contract), so p & ppc_mask is in [0, ppc-2] and
     * offset + DHARA_META_SIZE stays inside the checkpoint page laid
     * out by choose_ppc(). `buf` is the caller's DHARA_META_SIZE slot.
     */
    if (align_eq(p, j->head, j->log2_ppc)) {
        memcpy(buf, j->page_buf + offset, DHARA_META_SIZE);
        return 0;
    }

    /* Special case: incomplete metadata dumped at start of
     * recovery.
     */
    if ((j->recover_meta != DHARA_PAGE_NONE) &&
            align_eq(p, j->recover_root, j->log2_ppc))
        return dhara_nand_read(j->nand, j->recover_meta,
                               offset, DHARA_META_SIZE,
                               buf, err);

    /* General case: fetch from metadata page for checkpoint group */
    return dhara_nand_read(j->nand, p | ppc_mask,
                           offset, DHARA_META_SIZE,
                           buf, err);
}

dhara_page_t dhara_journal_peek(struct dhara_journal *j)
{
    if (j->head == j->tail) {
        return DHARA_PAGE_NONE;
    }

    if (is_aligned(j->tail, j->nand->log2_ppb)) {
        dhara_block_t blk = j->tail >> j->nand->log2_ppb;
        int i;

        for (i = 0; i < DHARA_MAX_RETRIES; i++) {
            if ((blk == (j->head >> j->nand->log2_ppb)) ||
                    !dhara_nand_is_bad(j->nand, blk)) {
                j->tail = blk << j->nand->log2_ppb;

                if (j->tail == j->head) {
                    j->root = DHARA_PAGE_NONE;
                }

                return j->tail;
            }

            blk = next_block(j->nand, blk);
        }

        /* Retries exhausted: every block we tried (up to
         * DHARA_MAX_RETRIES) was bad, and none of them was head's
         * block. Per this function's contract ("return the page
         * that's ready to read. If no page is ready, return
         * DHARA_PAGE_NONE"), we must not hand back j->tail here --
         * it still points into a block we just proved is bad, and
         * callers (e.g. dhara_map_sync()/dhara_map_gc()) rely on
         * DHARA_PAGE_NONE to distinguish "nothing ready" from "safe
         * to read/recycle this page".
         */
        return DHARA_PAGE_NONE;
    }

    return j->tail;
}

/* wrap(a, b) computes a "wrapped distance" using a single subtraction
 * rather than true modulo (a % b). This is correct ONLY because both
 * call sites in dhara_journal_dequeue() guarantee a < 2*b:
 *   - raw_size:   a = head + chip_size - tail, with head,tail in
 *                 [0, chip_size) -> a in (0, 2*chip_size)
 *   - root_offset: a = head + chip_size - root, with root in
 *                 [0, chip_size) when it's a real page (a in (0,
 *                 2*chip_size) too); when root == DHARA_PAGE_NONE the
 *                 comparison result doesn't matter because the only
 *                 use (`if (root_offset > raw_size) j->root =
 *                 DHARA_PAGE_NONE;`) reassigns the same DHARA_PAGE_NONE
 *                 value regardless of the outcome.
 * A generic "true modulo, any a/b" implementation is unnecessary here:
 * this is a private, file-static helper with exactly these two
 * verified-safe call sites.
 */
static dhara_page_t wrap(dhara_page_t a, dhara_page_t b)
{
    return a >= b ? (a - b) : a;
}

void dhara_journal_dequeue(struct dhara_journal *j)
{
    if (j->head == j->tail) {
        return;
    }

    j->tail = next_upage(j, j->tail);

    /* If the journal is clean at the time of dequeue, then this
     * data was always obsolete, and can be reused immediately.
     */
    if (!(j->flags & (DHARA_JOURNAL_F_DIRTY | DHARA_JOURNAL_F_RECOVERY))) {
        j->tail_sync = j->tail;
    }

    const dhara_page_t chip_size = j->nand->num_blocks << j->nand->log2_ppb;
    const dhara_page_t raw_size = wrap(j->head + chip_size - j->tail,
                                       chip_size);
    const dhara_page_t root_offset = wrap(j->head + chip_size - j->root,
                                          chip_size);

    if (root_offset > raw_size) {
        j->root = DHARA_PAGE_NONE;
    }
}

void dhara_journal_clear(struct dhara_journal *j)
{
    j->tail = j->head;
    j->root = DHARA_PAGE_NONE;
    j->flags |= DHARA_JOURNAL_F_DIRTY;

    hdr_clear_user(j->page_buf, j->nand->log2_page_size);
}

static int skip_block(struct dhara_journal *j, dhara_error_t *err)
{
    const dhara_block_t next = next_block(j->nand,
                                          j->head >> j->nand->log2_ppb);

    /* We can't roll onto the same block as the tail */
    if ((j->tail_sync >> j->nand->log2_ppb) == next) {
        dhara_set_error(err, DHARA_E_JOURNAL_FULL);
        return -1;
    }

    j->head = next << j->nand->log2_ppb;
    if (!j->head) {
        roll_stats(j);
    }

    return 0;
}

/* Make sure the head pointer is on a ready-to-program page. */
static int prepare_head(struct dhara_journal *j, dhara_error_t *err)
{
    const dhara_page_t next = next_upage(j, j->head);
    int i;

    /* We can't write if doing so would cause the head pointer to
     * roll onto the same block as the last-synced tail.
     */
    if (align_eq(next, j->tail_sync, j->nand->log2_ppb) &&
            !align_eq(next, j->head, j->nand->log2_ppb)) {
        dhara_set_error(err, DHARA_E_JOURNAL_FULL);
        return -1;
    }

    j->flags |= DHARA_JOURNAL_F_DIRTY;
    if (!is_aligned(j->head, j->nand->log2_ppb)) {
        return 0;
    }

    for (i = 0; i < DHARA_MAX_RETRIES; i++) {
        const dhara_block_t blk = j->head >> j->nand->log2_ppb;

        if (!dhara_nand_is_bad(j->nand, blk)) {
            return dhara_nand_erase(j->nand, blk, err);
        }

        j->bb_current++;
        if (skip_block(j, err) < 0) {
            return -1;
        }
    }

    dhara_set_error(err, DHARA_E_TOO_BAD);
    return -1;
}

static void restart_recovery(struct dhara_journal *j, dhara_page_t old_head)
{
    /* Mark the current head bad immediately, unless we're also
     * using it to hold our dumped metadata (it will then be marked
     * bad at the end of recovery).
     */
    if ((j->recover_meta == DHARA_PAGE_NONE) ||
            !align_eq(j->recover_meta, old_head, j->nand->log2_ppb)) {
        dhara_nand_mark_bad(j->nand, old_head >> j->nand->log2_ppb);
    } else {
        j->flags |= DHARA_JOURNAL_F_BAD_META;
    }

    /* Start recovery again. Reset the source enumeration to
     * the start of the original bad block, and reset the
     * destination enumeration to the newly found good
     * block.
     */
    j->flags &= ~DHARA_JOURNAL_F_ENUM_DONE;
    j->recover_next =
        j->recover_root & ~((1 << j->nand->log2_ppb) - 1);

    j->root = j->recover_root;
}

static int dump_meta(struct dhara_journal *j, dhara_error_t *err)
{
    int i;

    /* We've just begun recovery on a new erasable block, but we
     * have buffered metadata from the failed block.
     */
    for (i = 0; i < DHARA_MAX_RETRIES; i++) {
        dhara_error_t my_err;

        /* Try to dump metadata on this page */
        if (!(prepare_head(j, &my_err) ||
                dhara_nand_prog(j->nand, j->head,
                                j->page_buf, &my_err))) {
            j->recover_meta = j->head;
            j->head = next_upage(j, j->head);
            if (!j->head) {
                roll_stats(j);
            }
            hdr_clear_user(j->page_buf, j->nand->log2_page_size);
            return 0;
        }

        /* Report fatal errors */
        if (my_err != DHARA_E_BAD_BLOCK) {
            dhara_set_error(err, my_err);
            return -1;
        }

        j->bb_current++;
        dhara_nand_mark_bad(j->nand, j->head >> j->nand->log2_ppb);

        if (skip_block(j, err) < 0) {
            return -1;
        }
    }

    dhara_set_error(err, DHARA_E_TOO_BAD);
    return -1;
}

static int recover_from(struct dhara_journal *j,
                        dhara_error_t write_err,
                        dhara_error_t *err)
{
    const dhara_page_t old_head = j->head;

    if (write_err != DHARA_E_BAD_BLOCK) {
        dhara_set_error(err, write_err);
        return -1;
    }

    /* Advance to the next free page */
    j->bb_current++;
    if (skip_block(j, err) < 0) {
        return -1;
    }

    /* Are we already in the middle of a recovery? */
    if (dhara_journal_in_recovery(j)) {
        restart_recovery(j, old_head);
        dhara_set_error(err, DHARA_E_RECOVER);
        return -1;
    }

    /* Were we block aligned? No recovery required!
     *
     * A block-aligned old_head implies checkpoint-aligned too, given
     * the class invariant log2_ppc <= log2_ppb (established by
     * choose_ppc() in dhara_journal_init()): every multiple of
     * 2^log2_ppb is also a multiple of 2^log2_ppc, so there is no
     * buffered metadata to lose here. We check both alignments
     * explicitly (rather than relying solely on that invariant) so
     * that this stays correct in the defensive sense even if
     * log2_ppc were ever corrupted to exceed log2_ppb -- in that
     * (invalid) case we now fall through to the metadata-dump path
     * below instead of silently discarding buffered metadata.
     */
    if (is_aligned(old_head, j->nand->log2_ppb) &&
            is_aligned(old_head, j->log2_ppc)) {
        dhara_nand_mark_bad(j->nand, old_head >> j->nand->log2_ppb);
        return 0;
    }

    j->recover_root = j->root;
    j->recover_next =
        j->recover_root & ~((1 << j->nand->log2_ppb) - 1);

    /* Are we holding buffered metadata? Dump it first. */
    if (!is_aligned(old_head, j->log2_ppc) &&
            dump_meta(j, err) < 0) {
        return -1;
    }

    j->flags |= DHARA_JOURNAL_F_RECOVERY;
    dhara_set_error(err, DHARA_E_RECOVER);
    return -1;
}

static void finish_recovery(struct dhara_journal *j)
{
    /* We just recovered the last page. Mark the recovered
     * block as bad.
     */
    dhara_nand_mark_bad(j->nand,
                        j->recover_root >> j->nand->log2_ppb);

    /* If we had to dump metadata, and the page on which we
     * did this also went bad, mark it bad too.
     */
    if (j->flags & DHARA_JOURNAL_F_BAD_META)
        dhara_nand_mark_bad(j->nand,
                            j->recover_meta >> j->nand->log2_ppb);

    /* Was the tail on this page? Skip it forward */
    clear_recovery(j);
}

static int push_meta(struct dhara_journal *j, const uint8_t *meta,
                     dhara_error_t *err)
{
    const dhara_page_t old_head = j->head;
    dhara_error_t my_err;
    const size_t offset =
        hdr_user_offset(j->head & ((1 << j->log2_ppc) - 1));

    /* We've just written a user page. Add the metadata to the
     * buffer. `j->head` is always a user page (next_upage() skips the
     * checkpoint header), so the slot at `offset` is one of the
     * (2**log2_ppc - 1) meta slices choose_ppc() fitted into page_buf.
     */
    if (meta) {
        memcpy(j->page_buf + offset, meta, DHARA_META_SIZE);
    } else {
        memset(j->page_buf + offset, 0xff, DHARA_META_SIZE);
    }

    /* Unless we've filled the buffer, don't do any IO */
    if (!is_aligned(j->head + 2, j->log2_ppc)) {
        j->root = j->head;
        j->head++;
        return 0;
    }

    /* We don't need to check for immediate recover, because that'll
     * never happen -- we're not block-aligned.
     */
    hdr_put_magic(j->page_buf);
    hdr_set_epoch(j->page_buf, j->epoch);
    hdr_set_tail(j->page_buf, j->tail);
    hdr_set_bb_current(j->page_buf, j->bb_current);
    hdr_set_bb_last(j->page_buf, j->bb_last);

    if (dhara_nand_prog(j->nand, j->head + 1, j->page_buf, &my_err) < 0) {
        return recover_from(j, my_err, err);
    }

    j->flags &= ~DHARA_JOURNAL_F_DIRTY;

    j->root = old_head;
    j->head = next_upage(j, j->head);

    if (!j->head) {
        roll_stats(j);
    }

    if (j->flags & DHARA_JOURNAL_F_ENUM_DONE) {
        finish_recovery(j);
    }

    if (!(j->flags & DHARA_JOURNAL_F_RECOVERY)) {
        j->tail_sync = j->tail;
    }

    return 0;
}

int dhara_journal_enqueue(struct dhara_journal *j,
                          const uint8_t *data, const uint8_t *meta,
                          dhara_error_t *err)
{
    dhara_error_t my_err;
    int i;

    for (i = 0; i < DHARA_MAX_RETRIES; i++) {
        if (!(prepare_head(j, &my_err) ||
                (data && dhara_nand_prog(j->nand, j->head, data,
                                         &my_err)))) {
            return push_meta(j, meta, err);
        }

        if (recover_from(j, my_err, err) < 0) {
            return -1;
        }
    }

    dhara_set_error(err, DHARA_E_TOO_BAD);
    return -1;
}

int dhara_journal_copy(struct dhara_journal *j,
                       dhara_page_t p, const uint8_t *meta,
                       dhara_error_t *err)
{
    dhara_error_t my_err;
    int i;

    for (i = 0; i < DHARA_MAX_RETRIES; i++) {
        if (!(prepare_head(j, &my_err) ||
                dhara_nand_copy(j->nand, p, j->head, &my_err))) {
            return push_meta(j, meta, err);
        }

        if (recover_from(j, my_err, err) < 0) {
            return -1;
        }
    }

    dhara_set_error(err, DHARA_E_TOO_BAD);
    return -1;
}

dhara_page_t dhara_journal_next_recoverable(struct dhara_journal *j)
{
    const dhara_page_t n = j->recover_next;

    if (!dhara_journal_in_recovery(j)) {
        return DHARA_PAGE_NONE;
    }

    if (j->flags & DHARA_JOURNAL_F_ENUM_DONE) {
        return DHARA_PAGE_NONE;
    }

    if (j->recover_next == j->recover_root) {
        j->flags |= DHARA_JOURNAL_F_ENUM_DONE;
    } else {
        j->recover_next = next_upage(j, j->recover_next);
    }

    return n;
}
