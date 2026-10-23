/// @file nt/flash_journal.cpp
#include "flash_journal.h"
#include <string.h>

namespace nt {

FlashJournal::FlashJournal(const FlashOps &ops, uint32_t imageSize)
    : ops(ops)
    , imageSize(imageSize)
    , slotSize(align + Round(imageSize)) {}

const FlashJournal::Header &FlashJournal::HeaderAt(uint32_t slot) const {
    return *reinterpret_cast<const Header *>(ops.base + slot * slotSize);
}

bool FlashJournal::SlotErased(uint32_t slot) const {
    const uint8_t *p = ops.base + slot * slotSize;
    for (uint32_t i = 0; i < slotSize; ++i)
        if (p[i] != 0xFF)
            return false;
    return true;
}

void FlashJournal::Load(uint8_t *dst) {
    uint32_t best = SlotCount(), bestSeq = 0;
    for (uint32_t s = 0; s < SlotCount(); ++s) {
        const Header &h = HeaderAt(s);
        if (h.magic == magic && h.seq != 0xFFFFFFFFu && h.seq >= bestSeq) {
            best = s;
            bestSeq = h.seq;
        }
    }
    if (best < SlotCount())
        memcpy(dst, ops.base + best * slotSize + align, imageSize);
    else
        memset(dst, 0xFF, imageSize);
    nextSeq = bestSeq + 1;
    nextSlot = SlotCount();
    for (uint32_t s = (best < SlotCount() ? best + 1 : 0); s < SlotCount(); ++s) // skip torn (partly programmed) slots
        if (SlotErased(s)) {
            nextSlot = s;
            break;
        }
}

bool FlashJournal::Save(const uint8_t *img) {
    erasedLastSave = false;
    if (nextSlot >= SlotCount()) {
        if (!ops.erase())
            return false;
        nextSlot = 0;
        erasedLastSave = true;
    }
    const uint32_t off = nextSlot * slotSize;
    uint8_t chunk[align];
    for (uint32_t i = 0; i < Round(imageSize); i += align) { // image first...
        uint32_t n = imageSize - i < align ? imageSize - i : align;
        memset(chunk, 0xFF, align);
        memcpy(chunk, img + i, n);
        if (!ops.program(off + align + i, chunk, align))
            return false;
    }
    memset(chunk, 0xFF, align); // ...header last: the slot only becomes valid once fully written
    Header h { magic, nextSeq };
    memcpy(chunk, &h, sizeof(h));
    if (!ops.program(off, chunk, align))
        return false;
    ++nextSlot;
    ++nextSeq;
    return true;
}

} // namespace nt
