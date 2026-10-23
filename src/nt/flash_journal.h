/// @file nt/flash_journal.h
/// Append-only journal of fixed-size images in one erasable flash region (Ninja Turtle EEPROM emulation).
/// Each save programs the next free slot; the region is erased only when full, so the long CPU stall of a flash erase
/// happens once per ~(region / slot) saves instead of on every save. A slot counts only once its header is programmed
/// (written last), so a save interrupted by power loss is ignored on the next load.
#pragma once
#include <stdint.h>

namespace nt {

struct FlashOps {
    const uint8_t *base; ///< memory-mapped region start (reads)
    uint32_t size; ///< region size in bytes
    /// program @p len bytes (multiple of FlashJournal::align) at @p offset (aligned); may only clear bits
    bool (*program)(uint32_t offset, const uint8_t *data, uint32_t len);
    bool (*erase)(); ///< erase the whole region to 0xFF
};

class FlashJournal {
public:
    static constexpr uint32_t align = 32; ///< STM32H7 flash word (256 bit)
    static constexpr uint32_t magic = 0x4E54454Eu; ///< "NETN"

    FlashJournal(const FlashOps &ops, uint32_t imageSize);

    /// Copy the newest valid image into @p dst (imageSize bytes); all 0xFF if there is none. Also finds the next free slot.
    void Load(uint8_t *dst);
    /// Append @p img as the newest image; @returns false on a flash error. Sets ErasedLastSave() if it had to erase.
    bool Save(const uint8_t *img);

    bool ErasedLastSave() const { return erasedLastSave; }
    uint32_t SlotCount() const { return ops.size / slotSize; }
    static constexpr uint32_t Round(uint32_t n) { return (n + align - 1) / align * align; }

private:
    struct Header {
        uint32_t magic;
        uint32_t seq;
    };
    bool SlotErased(uint32_t slot) const;
    const Header &HeaderAt(uint32_t slot) const;

    FlashOps ops;
    uint32_t imageSize;
    uint32_t slotSize; ///< header (one flash word) + padded image
    uint32_t nextSlot = 0;
    uint32_t nextSeq = 1;
    bool erasedLastSave = false;
};

} // namespace nt
