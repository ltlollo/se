// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

static int
is_glyph_wide(uint32_t glyph) {
    if (0) {
        return 0;
    } else if (glyph >= 0x0020 && glyph < 0x007f) {
        return 0;
    } else if (glyph >= 0x00a0 && glyph < 0x00ad) {
        return 0;
    } else if (glyph >= 0x00ae && glyph < 0x034f) {
        return 0;
    } else if (glyph >= 0x0350 && glyph < 0x035c) {
        return 0;
    } else if (glyph >= 0x0363 && glyph < 0x0378) {
        return 0;
    } else if (glyph >= 0x037a && glyph < 0x0380) {
        return 0;
    } else if (glyph >= 0x0384 && glyph < 0x038b) {
        return 0;
    } else if (glyph >= 0x038e && glyph < 0x03a2) {
        return 0;
    } else if (glyph >= 0x03a3 && glyph < 0x0488) {
        return 0;
    } else if (glyph >= 0x048a && glyph < 0x052a) {
        return 0;
    } else if (glyph >= 0x0531 && glyph < 0x0557) {
        return 0;
    } else if (glyph >= 0x0559 && glyph < 0x0560) {
        return 0;
    } else if (glyph >= 0x0561 && glyph < 0x0588) {
        return 0;
    } else if (glyph >= 0x0591 && glyph < 0x05c8) {
        return 0;
    } else if (glyph >= 0x05d0 && glyph < 0x05eb) {
        return 0;
    } else if (glyph >= 0x05f0 && glyph < 0x05f5) {
        return 0;
    } else if (glyph >= 0x0617 && glyph < 0x061c) {
        return 0;
    } else if (glyph >= 0x061e && glyph < 0x0656) {
        return 0;
    } else if (glyph >= 0x0657 && glyph < 0x06aa) {
        return 0;
    } else if (glyph >= 0x06ab && glyph < 0x06d6) {
        return 0;
    } else if (glyph >= 0x06ea && glyph < 0x0700) {
        return 0;
    } else if (glyph >= 0x0701 && glyph < 0x070b) {
        return 0;
    } else if (glyph >= 0x072f && glyph < 0x073d) {
        return 0;
    } else if (glyph >= 0x073f && glyph < 0x0749) {
        return 0;
    } else if (glyph >= 0x0750 && glyph < 0x0790) {
        return 0;
    } else if (glyph >= 0x0791 && glyph < 0x079d) {
        return 0;
    } else if (glyph >= 0x07a0 && glyph < 0x07b2) {
        return 0;
    } else if (glyph >= 0x07c0 && glyph < 0x07fb) {
        return 0;
    } else if (glyph >= 0x081c && glyph < 0x082c) {
        return 0;
    } else if (glyph >= 0x08a0 && glyph < 0x08b5) {
        return 0;
    } else if (glyph >= 0x08b6 && glyph < 0x08be) {
        return 0;
    } else if (glyph >= 0x08d4 && glyph < 0x08da) {
        return 0;
    } else if (glyph >= 0x08e0 && glyph < 0x0900) {
        return 0;
    } else if (glyph >= 0x0943 && glyph < 0x0949) {
        return 0;
    } else if (glyph >= 0x0951 && glyph < 0x0958) {
        return 0;
    } else if (glyph >= 0x0e01 && glyph < 0x0e3b) {
        return 0;
    } else if (glyph >= 0x0e3f && glyph < 0x0e5c) {
        return 0;
    } else if (glyph >= 0x0e99 && glyph < 0x0ea0) {
        return 0;
    } else if (glyph >= 0x0ead && glyph < 0x0eba) {
        return 0;
    } else if (glyph >= 0x0ec0 && glyph < 0x0ec5) {
        return 0;
    } else if (glyph >= 0x0ec8 && glyph < 0x0ece) {
        return 0;
    } else if (glyph >= 0x0ed0 && glyph < 0x0eda) {
        return 0;
    } else if (glyph >= 0x0f04 && glyph < 0x0f16) {
        return 0;
    } else if (glyph >= 0x0f19 && glyph < 0x0f3a) {
        return 0;
    } else if (glyph >= 0x0f3c && glyph < 0x0f48) {
        return 0;
    } else if (glyph >= 0x0f49 && glyph < 0x0f6d) {
        return 0;
    } else if (glyph >= 0x0f71 && glyph < 0x0f98) {
        return 0;
    } else if (glyph >= 0x0f99 && glyph < 0x0fbd) {
        return 0;
    } else if (glyph >= 0x0fbe && glyph < 0x0fc5) {
        return 0;
    } else if (glyph >= 0x1012 && glyph < 0x1018) {
        return 0;
    } else if (glyph >= 0x1041 && glyph < 0x104d) {
        return 0;
    } else if (glyph >= 0x1050 && glyph < 0x1056) {
        return 0;
    } else if (glyph >= 0x10a0 && glyph < 0x10c6) {
        return 0;
    } else if (glyph >= 0x10d0 && glyph < 0x1113) {
        return 0;
    } else if (glyph >= 0x1390 && glyph < 0x1395) {
        return 0;
    } else if (glyph >= 0x13a0 && glyph < 0x13f6) {
        return 0;
    } else if (glyph >= 0x13f8 && glyph < 0x13fe) {
        return 0;
    } else if (glyph >= 0x141e && glyph < 0x142b) {
        return 0;
    } else if (glyph >= 0x1433 && glyph < 0x143a) {
        return 0;
    } else if (glyph >= 0x146b && glyph < 0x14c0) {
        return 0;
    } else if (glyph >= 0x14ea && glyph < 0x150c) {
        return 0;
    } else if (glyph >= 0x1525 && glyph < 0x1542) {
        return 0;
    } else if (glyph >= 0x1548 && glyph < 0x1553) {
        return 0;
    } else if (glyph >= 0x15a6 && glyph < 0x15b8) {
        return 0;
    } else if (glyph >= 0x1677 && glyph < 0x1680) {
        return 0;
    } else if (glyph >= 0x16a0 && glyph < 0x16e0) {
        return 0;
    } else if (glyph >= 0x16e3 && glyph < 0x16f9) {
        return 0;
    } else if (glyph >= 0x17d2 && glyph < 0x17d7) {
        return 0;
    } else if (glyph >= 0x17f0 && glyph < 0x17fa) {
        return 0;
    } else if (glyph >= 0x182e && glyph < 0x1834) {
        return 0;
    } else if (glyph >= 0x1835 && glyph < 0x183c) {
        return 0;
    } else if (glyph >= 0x184d && glyph < 0x1859) {
        return 0;
    } else if (glyph >= 0x185b && glyph < 0x1861) {
        return 0;
    } else if (glyph >= 0x186c && glyph < 0x1873) {
        return 0;
    } else if (glyph >= 0x18d4 && glyph < 0x18e0) {
        return 0;
    } else if (glyph >= 0x1900 && glyph < 0x1905) {
        return 0;
    } else if (glyph >= 0x190b && glyph < 0x1910) {
        return 0;
    } else if (glyph >= 0x1911 && glyph < 0x1919) {
        return 0;
    } else if (glyph >= 0x191a && glyph < 0x191f) {
        return 0;
    } else if (glyph >= 0x1950 && glyph < 0x196e) {
        return 0;
    } else if (glyph >= 0x1970 && glyph < 0x1975) {
        return 0;
    } else if (glyph >= 0x1a6e && glyph < 0x1a73) {
        return 0;
    } else if (glyph >= 0x1ab0 && glyph < 0x1abf) {
        return 0;
    } else if (glyph >= 0x1b6a && glyph < 0x1b70) {
        return 0;
    } else if (glyph >= 0x1b72 && glyph < 0x1b7d) {
        return 0;
    } else if (glyph >= 0x1c50 && glyph < 0x1c89) {
        return 0;
    } else if (glyph >= 0x1d00 && glyph < 0x1d7a) {
        return 0;
    } else if (glyph >= 0x1d7b && glyph < 0x1dcd) {
        return 0;
    } else if (glyph >= 0x1dce && glyph < 0x1dfa) {
        return 0;
    } else if (glyph >= 0x1dfd && glyph < 0x1f16) {
        return 0;
    } else if (glyph >= 0x1f18 && glyph < 0x1f1e) {
        return 0;
    } else if (glyph >= 0x1f20 && glyph < 0x1f46) {
        return 0;
    } else if (glyph >= 0x1f48 && glyph < 0x1f4e) {
        return 0;
    } else if (glyph >= 0x1f50 && glyph < 0x1f58) {
        return 0;
    } else if (glyph >= 0x1f5f && glyph < 0x1f7e) {
        return 0;
    } else if (glyph >= 0x1f80 && glyph < 0x1fb5) {
        return 0;
    } else if (glyph >= 0x1fb6 && glyph < 0x1fc5) {
        return 0;
    } else if (glyph >= 0x1fc6 && glyph < 0x1fd4) {
        return 0;
    } else if (glyph >= 0x1fd6 && glyph < 0x1fdc) {
        return 0;
    } else if (glyph >= 0x1fdd && glyph < 0x1ff0) {
        return 0;
    } else if (glyph >= 0x1ff6 && glyph < 0x1fff) {
        return 0;
    } else if (glyph >= 0x2000 && glyph < 0x200b) {
        return 0;
    } else if (glyph >= 0x2010 && glyph < 0x2028) {
        return 0;
    } else if (glyph >= 0x202f && glyph < 0x2057) {
        return 0;
    } else if (glyph >= 0x2058 && glyph < 0x2060) {
        return 0;
    } else if (glyph >= 0x2074 && glyph < 0x208f) {
        return 0;
    } else if (glyph >= 0x2090 && glyph < 0x209d) {
        return 0;
    } else if (glyph >= 0x20a0 && glyph < 0x20b9) {
        return 0;
    } else if (glyph >= 0x20ba && glyph < 0x20c0) {
        return 0;
    } else if (glyph >= 0x20d0 && glyph < 0x20dd) {
        return 0;
    } else if (glyph >= 0x20eb && glyph < 0x20f1) {
        return 0;
    } else if (glyph >= 0x2100 && glyph < 0x210e) {
        return 0;
    } else if (glyph >= 0x2110 && glyph < 0x212e) {
        return 0;
    } else if (glyph >= 0x212f && glyph < 0x213a) {
        return 0;
    } else if (glyph >= 0x2147 && glyph < 0x214c) {
        return 0;
    } else if (glyph >= 0x2150 && glyph < 0x2182) {
        return 0;
    } else if (glyph >= 0x2183 && glyph < 0x2188) {
        return 0;
    } else if (glyph >= 0x2190 && glyph < 0x21f4) {
        return 0;
    } else if (glyph >= 0x2200 && glyph < 0x22f2) {
        return 0;
    } else if (glyph >= 0x2300 && glyph < 0x2329) {
        return 0;
    } else if (glyph >= 0x232d && glyph < 0x237b) {
        return 0;
    } else if (glyph >= 0x239b && glyph < 0x23b2) {
        return 0;
    } else if (glyph >= 0x23b7 && glyph < 0x23c0) {
        return 0;
    } else if (glyph >= 0x23cf && glyph < 0x23d4) {
        return 0;
    } else if (glyph >= 0x2422 && glyph < 0x2427) {
        return 0;
    } else if (glyph >= 0x2440 && glyph < 0x244b) {
        return 0;
    } else if (glyph >= 0x2500 && glyph < 0x2603) {
        return 0;
    } else if (glyph >= 0x2604 && glyph < 0x2615) {
        return 0;
    } else if (glyph >= 0x261a && glyph < 0x2622) {
        return 0;
    } else if (glyph >= 0x2625 && glyph < 0x262b) {
        return 0;
    } else if (glyph >= 0x2638 && glyph < 0x2672) {
        return 0;
    } else if (glyph >= 0x26b7 && glyph < 0x26bd) {
        return 0;
    } else if (glyph >= 0x2768 && glyph < 0x2776) {
        return 0;
    } else if (glyph >= 0x27e6 && glyph < 0x27f0) {
        return 0;
    } else if (glyph >= 0x2800 && glyph < 0x2900) {
        return 0;
    } else if (glyph >= 0x2980 && glyph < 0x2993) {
        return 0;
    } else if (glyph >= 0x29d1 && glyph < 0x29da) {
        return 0;
    } else if (glyph >= 0x29ee && glyph < 0x29f4) {
        return 0;
    } else if (glyph >= 0x29f5 && glyph < 0x29fe) {
        return 0;
    } else if (glyph >= 0x2a21 && glyph < 0x2a27) {
        return 0;
    } else if (glyph >= 0x2a28 && glyph < 0x2a2d) {
        return 0;
    } else if (glyph >= 0x2a46 && glyph < 0x2a4c) {
        return 0;
    } else if (glyph >= 0x2a6f && glyph < 0x2a74) {
        return 0;
    } else if (glyph >= 0x2a8f && glyph < 0x2a95) {
        return 0;
    } else if (glyph >= 0x2abf && glyph < 0x2ac7) {
        return 0;
    } else if (glyph >= 0x2aee && glyph < 0x2af3) {
        return 0;
    } else if (glyph >= 0x2b25 && glyph < 0x2b2c) {
        return 0;
    } else if (glyph >= 0x2c00 && glyph < 0x2c0f) {
        return 0;
    } else if (glyph >= 0x2c10 && glyph < 0x2c1f) {
        return 0;
    } else if (glyph >= 0x2c20 && glyph < 0x2c27) {
        return 0;
    } else if (glyph >= 0x2c2a && glyph < 0x2c2f) {
        return 0;
    } else if (glyph >= 0x2c30 && glyph < 0x2c3f) {
        return 0;
    } else if (glyph >= 0x2c40 && glyph < 0x2c4f) {
        return 0;
    } else if (glyph >= 0x2c50 && glyph < 0x2c57) {
        return 0;
    } else if (glyph >= 0x2c5a && glyph < 0x2c5f) {
        return 0;
    } else if (glyph >= 0x2c60 && glyph < 0x2cce) {
        return 0;
    } else if (glyph >= 0x2cd0 && glyph < 0x2ce7) {
        return 0;
    } else if (glyph >= 0x2cf9 && glyph < 0x2d05) {
        return 0;
    } else if (glyph >= 0x2d15 && glyph < 0x2d1b) {
        return 0;
    } else if (glyph >= 0x2d30 && glyph < 0x2d48) {
        return 0;
    } else if (glyph >= 0x2d49 && glyph < 0x2d68) {
        return 0;
    } else if (glyph >= 0x2de0 && glyph < 0x2e0e) {
        return 0;
    } else if (glyph >= 0x2e16 && glyph < 0x2e3a) {
        return 0;
    } else if (glyph >= 0x2e3c && glyph < 0x2e43) {
        return 0;
    } else if (glyph >= 0x2e44 && glyph < 0x2e4a) {
        return 0;
    } else if (glyph >= 0x3017 && glyph < 0x301c) {
        return 0;
    } else if (glyph >= 0x3196 && glyph < 0x319b) {
        return 0;
    } else if (glyph >= 0x31b3 && glyph < 0x31b8) {
        return 0;
    } else if (glyph >= 0x31f0 && glyph < 0x31f5) {
        return 0;
    } else if (glyph >= 0xa4d0 && glyph < 0xa500) {
        return 0;
    } else if (glyph >= 0xa640 && glyph < 0xa64c) {
        return 0;
    } else if (glyph >= 0xa652 && glyph < 0xa65e) {
        return 0;
    } else if (glyph >= 0xa673 && glyph < 0xa684) {
        return 0;
    } else if (glyph >= 0xa686 && glyph < 0xa698) {
        return 0;
    } else if (glyph >= 0xa69a && glyph < 0xa6f8) {
        return 0;
    } else if (glyph >= 0xa700 && glyph < 0xa728) {
        return 0;
    } else if (glyph >= 0xa72a && glyph < 0xa732) {
        return 0;
    } else if (glyph >= 0xa73e && glyph < 0xa74e) {
        return 0;
    } else if (glyph >= 0xa750 && glyph < 0xa758) {
        return 0;
    } else if (glyph >= 0xa75a && glyph < 0xa771) {
        return 0;
    } else if (glyph >= 0xa778 && glyph < 0xa7af) {
        return 0;
    } else if (glyph >= 0xa7b0 && glyph < 0xa7b8) {
        return 0;
    } else if (glyph >= 0xa7f7 && glyph < 0xa7ff) {
        return 0;
    } else if (glyph >= 0xa882 && glyph < 0xa893) {
        return 0;
    } else if (glyph >= 0xa894 && glyph < 0xa8b4) {
        return 0;
    } else if (glyph >= 0xa9ef && glyph < 0xa9f9) {
        return 0;
    } else if (glyph >= 0xab30 && glyph < 0xab66) {
        return 0;
    } else if (glyph >= 0xab70 && glyph < 0xabc0) {
        return 0;
    } else if (glyph >= 0xe014 && glyph < 0xe01c) {
        return 0;
    } else if (glyph >= 0xe020 && glyph < 0xe02f) {
        return 0;
    } else if (glyph >= 0xe034 && glyph < 0xe03a) {
        return 0;
    } else if (glyph >= 0xe040 && glyph < 0xe05b) {
        return 0;
    } else if (glyph >= 0xe060 && glyph < 0xe06d) {
        return 0;
    } else if (glyph >= 0xe080 && glyph < 0xe0ed) {
        return 0;
    } else if (glyph >= 0xe150 && glyph < 0xe16f) {
        return 0;
    } else if (glyph >= 0xe170 && glyph < 0xe18f) {
        return 0;
    } else if (glyph >= 0xe190 && glyph < 0xe19d) {
        return 0;
    } else if (glyph >= 0xe1a0 && glyph < 0xe1ac) {
        return 0;
    } else if (glyph >= 0xe1b0 && glyph < 0xe1b7) {
        return 0;
    } else if (glyph >= 0xe1bf && glyph < 0xe1cc) {
        return 0;
    } else if (glyph >= 0xe22b && glyph < 0xe239) {
        return 0;
    } else if (glyph >= 0xe23c && glyph < 0xe26c) {
        return 0;
    } else if (glyph >= 0xe274 && glyph < 0xe279) {
        return 0;
    } else if (glyph >= 0xe5c0 && glyph < 0xe5e0) {
        return 0;
    } else if (glyph >= 0xe68e && glyph < 0xe6ac) {
        return 0;
    } else if (glyph >= 0xe6af && glyph < 0xe6d0) {
        return 0;
    } else if (glyph >= 0xe740 && glyph < 0xe768) {
        return 0;
    } else if (glyph >= 0xe7b1 && glyph < 0xe7ee) {
        return 0;
    } else if (glyph >= 0xe7f1 && glyph < 0xe7f6) {
        return 0;
    } else if (glyph >= 0xe800 && glyph < 0xe82a) {
        return 0;
    } else if (glyph >= 0xe8e0 && glyph < 0xe8f5) {
        return 0;
    } else if (glyph >= 0xf8a0 && glyph < 0xf8bd) {
        return 0;
    } else if (glyph >= 0xf8bf && glyph < 0xf8c9) {
        return 0;
    } else if (glyph >= 0xfb00 && glyph < 0xfb07) {
        return 0;
    } else if (glyph >= 0xfb13 && glyph < 0xfb18) {
        return 0;
    } else if (glyph >= 0xfb29 && glyph < 0xfb37) {
        return 0;
    } else if (glyph >= 0xfb38 && glyph < 0xfb3d) {
        return 0;
    } else if (glyph >= 0xfb46 && glyph < 0xfbc2) {
        return 0;
    } else if (glyph >= 0xfbd3 && glyph < 0xfc1f) {
        return 0;
    } else if (glyph >= 0xfc26 && glyph < 0xfc3d) {
        return 0;
    } else if (glyph >= 0xfc3f && glyph < 0xfcad) {
        return 0;
    } else if (glyph >= 0xfcb8 && glyph < 0xfce7) {
        return 0;
    } else if (glyph >= 0xfceb && glyph < 0xfcfb) {
        return 0;
    } else if (glyph >= 0xfcff && glyph < 0xfd17) {
        return 0;
    } else if (glyph >= 0xfd21 && glyph < 0xfd2d) {
        return 0;
    } else if (glyph >= 0xfe20 && glyph < 0xfe35) {
        return 0;
    } else if (glyph >= 0xfe54 && glyph < 0xfe67) {
        return 0;
    } else if (glyph >= 0xfe70 && glyph < 0xfe75) {
        return 0;
    } else if (glyph >= 0xfe76 && glyph < 0xfefd) {
        return 0;
    } else if (glyph >= 0xff61 && glyph < 0xffbf) {
        return 0;
    } else if (glyph >= 0xffc2 && glyph < 0xffc8) {
        return 0;
    } else if (glyph >= 0xffca && glyph < 0xffd0) {
        return 0;
    } else if (glyph >= 0xffd2 && glyph < 0xffd8) {
        return 0;
    } else if (glyph >= 0xffe8 && glyph < 0xffef) {
        return 0;
    }
    return 1;
}
