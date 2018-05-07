// This is free and unencumbered software released into the public domain.
// For more information, see LICENSE.

#ifndef COLOR_H
#define COLOR_H

static struct color colors_table[] = {
    // glyphs
    [0x00] = {0.85, 1.00, 1.00}, // light gray
    [0x01] = {1.00, 0.00, 0.37}, // dark pink
    [0x02] = {1.00, 0.84, 0.37}, // yellow
    [0x03] = {0.52, 0.68, 0.37}, // green
    [0x04] = {0.52, 0.37, 0.52}, // dark purple
    [0x05] = {0.68, 0.68, 0.52}, // light green
    [0x06] = {0.84, 0.52, 0.37}, // orange
    // selections
    [0x07] = {2.00, 2.00, 2.00}, // white
    [0x08] = {1.80, 1.80, 1.40}, // yellow
    [0x09] = {2.00, 0.00, 0.00}, // red
    [0x0a] = {1.20, 1.40, 1.80}, // blue
};

#endif // COLOR_H
