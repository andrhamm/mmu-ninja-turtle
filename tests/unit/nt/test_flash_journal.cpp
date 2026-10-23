#include "catch2/catch_test_macros.hpp"
#include "../../../src/nt/flash_journal.h"
#include <algorithm>
#include <vector>

namespace {
std::vector<uint8_t> flash(128 * 1024, 0xFF); // one STM32H7 sector
int erases = 0;
bool failNextProgramAfter = false;
int programsLeft = -1; // simulate power loss: stop after N program calls

bool Program(uint32_t off, const uint8_t *d, uint32_t len) {
    if (programsLeft == 0)
        return false;
    if (programsLeft > 0)
        --programsLeft;
    REQUIRE(off % nt::FlashJournal::align == 0);
    REQUIRE(len % nt::FlashJournal::align == 0);
    for (uint32_t i = 0; i < len; ++i) {
        REQUIRE((flash[off + i] & d[i]) == d[i]); // flash can only clear bits
        flash[off + i] = d[i];
    }
    return true;
}
bool Erase() {
    std::fill(flash.begin(), flash.end(), 0xFF);
    ++erases;
    return true;
}
nt::FlashOps Ops() { return { flash.data(), uint32_t(flash.size()), Program, Erase }; }
void Reset() {
    std::fill(flash.begin(), flash.end(), 0xFF);
    erases = 0;
    programsLeft = -1;
}
constexpr uint32_t imageSize = 2049; // EEPROM End() + 1: includes the layout-version byte at offset 2048
} // namespace

TEST_CASE("flash_journal::empty region loads as erased", "[nt][flash_journal]") {
    Reset();
    nt::FlashJournal j(Ops(), imageSize);
    std::vector<uint8_t> img(imageSize, 0);
    j.Load(img.data());
    CHECK(std::all_of(img.begin(), img.end(), [](uint8_t b) { return b == 0xFF; }));
}

TEST_CASE("flash_journal::round trip keeps every byte incl. the last", "[nt][flash_journal]") {
    Reset();
    std::vector<uint8_t> img(imageSize);
    for (uint32_t i = 0; i < imageSize; ++i)
        img[i] = uint8_t(i * 7);
    img[imageSize - 1] = 0x42; // permanent_storage layoutVersion lives here
    {
        nt::FlashJournal j(Ops(), imageSize);
        j.Load(std::vector<uint8_t>(imageSize).data());
        REQUIRE(j.Save(img.data()));
    }
    nt::FlashJournal reboot(Ops(), imageSize);
    std::vector<uint8_t> out(imageSize);
    reboot.Load(out.data());
    CHECK(out == img);
}

TEST_CASE("flash_journal::newest image wins and erase only when full", "[nt][flash_journal]") {
    Reset();
    nt::FlashJournal j(Ops(), imageSize);
    std::vector<uint8_t> img(imageSize, 0);
    j.Load(img.data());
    const uint32_t saves = j.SlotCount() * 2 + 3;
    for (uint32_t n = 1; n <= saves; ++n) {
        img[0] = uint8_t(n);
        REQUIRE(j.Save(img.data()));
    }
    CHECK(erases == 2); // ~62 saves per erase instead of one erase per save
    nt::FlashJournal reboot(Ops(), imageSize);
    std::vector<uint8_t> out(imageSize);
    reboot.Load(out.data());
    CHECK(out[0] == uint8_t(saves));
}

TEST_CASE("flash_journal::torn save is ignored", "[nt][flash_journal]") {
    Reset();
    std::vector<uint8_t> a(imageSize, 0xA1), b(imageSize, 0xB2), out(imageSize);
    nt::FlashJournal j(Ops(), imageSize);
    j.Load(out.data());
    REQUIRE(j.Save(a.data()));
    programsLeft = 5; // power fails while writing image b (header never written)
    CHECK_FALSE(j.Save(b.data()));
    programsLeft = -1;
    nt::FlashJournal reboot(Ops(), imageSize);
    reboot.Load(out.data());
    CHECK(out == a);
    REQUIRE(reboot.Save(b.data())); // next save skips the torn slot
    nt::FlashJournal again(Ops(), imageSize);
    again.Load(out.data());
    CHECK(out == b);
}
