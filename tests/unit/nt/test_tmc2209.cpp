#include "catch2/catch_test_macros.hpp"
#include "../../../src/nt/tmc2209.h"

using namespace nt::tmc2209;

// expected bytes computed with Klipper's tmc_uart.py algorithm (_calc_crc8/_encode_write/_encode_read)
TEST_CASE("tmc2209::EncodeWrite", "[nt][tmc2209]") {
    uint8_t o[8];
    EncodeWrite(0, CHOPCONF, 0x10000053, o);
    const uint8_t want[8] = { 0x05, 0x00, 0xEC, 0x10, 0x00, 0x00, 0x53, 0x9C };
    for (int i = 0; i < 8; ++i)
        CHECK(o[i] == want[i]);
}

TEST_CASE("tmc2209::EncodeRead", "[nt][tmc2209]") {
    uint8_t o[4];
    EncodeRead(0, CHOPCONF, o);
    const uint8_t want[4] = { 0x05, 0x00, 0x6C, 0xCA };
    for (int i = 0; i < 4; ++i)
        CHECK(o[i] == want[i]);
}

TEST_CASE("tmc2209::DecodeReply", "[nt][tmc2209]") {
    uint8_t in[8] = { 0x05, 0xFF, 0x6C, 0x10, 0x00, 0x00, 0x53, 0x51 };
    uint32_t v = 0;
    REQUIRE(DecodeReply(CHOPCONF, in, v));
    CHECK(v == 0x10000053);
    in[7] ^= 1; // corrupt CRC
    CHECK_FALSE(DecodeReply(CHOPCONF, in, v));
    in[7] ^= 1;
    CHECK_FALSE(DecodeReply(GCONF, in, v)); // wrong register
}

TEST_CASE("tmc2209::register values", "[nt][tmc2209]") {
    // TMC2209 CHOPCONF power-on default is 0x10000053; ours adds tbl=2, dedge and the requested mres
    CHECK(Chopconf(0, false) == (0x10000053u | (2u << 15) | (1u << 29)));
    CHECK(((Chopconf(4, true) >> 24) & 0xF) == 4);
    CHECK((Chopconf(4, true) & (1u << 17)) != 0);
    CHECK(Gconf(false) == 0x1C0);
    CHECK(Gconf(true) == 0x1C4);
    CHECK(IholdIrun(8, 20, 8) == 0x81408u);
}
