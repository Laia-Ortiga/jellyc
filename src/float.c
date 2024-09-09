//! Arbitrary-precision decimal class for fallback algorithms.
//!
//! This is only used if the fast-path (native floats) and
//! the Eisel-Lemire algorithm are unable to unambiguously
//! determine the float.
//!
//! The technique used is "Simple Decimal Conversion", developed
//! by Nigel Tao and Ken Thompson. A detailed description of the
//! algorithm can be found in "ParseNumberF64 by Simple Decimal Conversion",
//! available online: <https://nigeltao.github.io/blog/2020/parse-number-f64-simple.html>.
#include "float.h"

#include "adt.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/// The maximum number of digits required to unambiguously round a float.
///
/// For a double-precision IEEE 754 float, this required 767 digits,
/// so we store the max digits + 1.
///
/// We can exactly represent a float in radix `b` from radix 2 if
/// `b` is divisible by 2. This function calculates the exact number of
/// digits required to exactly represent that float.
///
/// According to the "Handbook of Floating Point Arithmetic",
/// for IEEE754, with emin being the min exponent, p2 being the
/// precision, and b being the radix, the number of digits follows as:
///
/// `−emin + p2 + ⌊(emin + 1) log(2, b) − log(1 − 2^(−p2), b)⌋`
///
/// For f32, this follows as:
///     emin = -126
///     p2 = 24
///
/// For f64, this follows as:
///     emin = -1022
///     p2 = 53
///
/// In Python:
///     `-emin + p2 + math.floor((emin+ 1)*math.log(2, b)-math.log(1-2**(-p2), b))`
#define MAX_DIGITS 768

#define MAX_DIGITS_WITHOUT_OVERFLOW 19
#define DECIMAL_POINT_RANGE 2047

typedef struct {
    /// The number of significant digits in the decimal.
    int num_digits;
    /// The offset of the decimal point in the significant digits.
    int decimal_point;
    /// If the number of significant digits stored in the decimal is truncated.
    bool truncated;
    /// Buffer of the raw digits, in the range [0, 9].
    unsigned char digits[MAX_DIGITS];
} Decimal;

/// Append a digit to the buffer.
static void try_add_digit(Decimal *self, unsigned char digit) {
    if (self->num_digits < MAX_DIGITS) {
        self->digits[self->num_digits] = digit;
    }
    self->num_digits += 1;
}

/// Trim trailing zeros from the buffer.
static void trim(Decimal *self) {
    // All of the following calls to `Decimal::trim` can't panic because:
    //
    //  1. `parse_decimal` sets `num_digits` to a max of `Decimal::MAX_DIGITS`.
    //  2. `right_shift` sets `num_digits` to `write_index`, which is bounded by `num_digits`.
    //  3. `left_shift` `num_digits` to a max of `Decimal::MAX_DIGITS`.
    //
    // Trim is only called in `right_shift` and `left_shift`.
    assert(self->num_digits <= MAX_DIGITS);
    while (self->num_digits != 0 && self->digits[self->num_digits - 1] == 0) {
        self->num_digits -= 1;
    }
}

static uint64_t round_decimal(Decimal const *self) {
    if (self->num_digits == 0 || self->decimal_point < 0) {
        return 0;
    } else if (self->decimal_point > 18) {
        return 0xFFFFFFFFFFFFFFFF;
    }
    int dp = self->decimal_point;
    uint64_t n = 0;
    for (int i = 0; i < dp; i++) {
        n *= 10;
        if (i < self->num_digits) {
            n += (uint64_t) self->digits[i];
        }
    }
    bool round_up = false;
    if (dp < self->num_digits) {
        round_up = self->digits[dp] >= 5;
        if (self->digits[dp] == 5 && dp + 1 == self->num_digits) {
            round_up = self->truncated || ((dp != 0) && ((1 & self->digits[dp - 1]) != 0));
        }
    }
    if (round_up) {
        n += 1;
    }
    return n;
}

static int number_of_digits_decimal_left_shift(Decimal const *d, int shift) {
    uint16_t const TABLE[] = {
        0x0000, 0x0800, 0x0801, 0x0803, 0x1006, 0x1009, 0x100D, 0x1812, 0x1817, 0x181D, 0x2024,
        0x202B, 0x2033, 0x203C, 0x2846, 0x2850, 0x285B, 0x3067, 0x3073, 0x3080, 0x388E, 0x389C,
        0x38AB, 0x38BB, 0x40CC, 0x40DD, 0x40EF, 0x4902, 0x4915, 0x4929, 0x513E, 0x5153, 0x5169,
        0x5180, 0x5998, 0x59B0, 0x59C9, 0x61E3, 0x61FD, 0x6218, 0x6A34, 0x6A50, 0x6A6D, 0x6A8B,
        0x72AA, 0x72C9, 0x72E9, 0x7B0A, 0x7B2B, 0x7B4D, 0x8370, 0x8393, 0x83B7, 0x83DC, 0x8C02,
        0x8C28, 0x8C4F, 0x9477, 0x949F, 0x94C8, 0x9CF2, 0x051C, 0x051C, 0x051C, 0x051C,
    };
    uint8_t const TABLE_POW5[] = {
        5, 2, 5, 1, 2, 5, 6, 2, 5, 3, 1, 2, 5, 1, 5, 6, 2, 5, 7, 8, 1, 2, 5, 3, 9, 0, 6, 2, 5, 1,
        9, 5, 3, 1, 2, 5, 9, 7, 6, 5, 6, 2, 5, 4, 8, 8, 2, 8, 1, 2, 5, 2, 4, 4, 1, 4, 0, 6, 2, 5,
        1, 2, 2, 0, 7, 0, 3, 1, 2, 5, 6, 1, 0, 3, 5, 1, 5, 6, 2, 5, 3, 0, 5, 1, 7, 5, 7, 8, 1, 2,
        5, 1, 5, 2, 5, 8, 7, 8, 9, 0, 6, 2, 5, 7, 6, 2, 9, 3, 9, 4, 5, 3, 1, 2, 5, 3, 8, 1, 4, 6,
        9, 7, 2, 6, 5, 6, 2, 5, 1, 9, 0, 7, 3, 4, 8, 6, 3, 2, 8, 1, 2, 5, 9, 5, 3, 6, 7, 4, 3, 1,
        6, 4, 0, 6, 2, 5, 4, 7, 6, 8, 3, 7, 1, 5, 8, 2, 0, 3, 1, 2, 5, 2, 3, 8, 4, 1, 8, 5, 7, 9,
        1, 0, 1, 5, 6, 2, 5, 1, 1, 9, 2, 0, 9, 2, 8, 9, 5, 5, 0, 7, 8, 1, 2, 5, 5, 9, 6, 0, 4, 6,
        4, 4, 7, 7, 5, 3, 9, 0, 6, 2, 5, 2, 9, 8, 0, 2, 3, 2, 2, 3, 8, 7, 6, 9, 5, 3, 1, 2, 5, 1,
        4, 9, 0, 1, 1, 6, 1, 1, 9, 3, 8, 4, 7, 6, 5, 6, 2, 5, 7, 4, 5, 0, 5, 8, 0, 5, 9, 6, 9, 2,
        3, 8, 2, 8, 1, 2, 5, 3, 7, 2, 5, 2, 9, 0, 2, 9, 8, 4, 6, 1, 9, 1, 4, 0, 6, 2, 5, 1, 8, 6,
        2, 6, 4, 5, 1, 4, 9, 2, 3, 0, 9, 5, 7, 0, 3, 1, 2, 5, 9, 3, 1, 3, 2, 2, 5, 7, 4, 6, 1, 5,
        4, 7, 8, 5, 1, 5, 6, 2, 5, 4, 6, 5, 6, 6, 1, 2, 8, 7, 3, 0, 7, 7, 3, 9, 2, 5, 7, 8, 1, 2,
        5, 2, 3, 2, 8, 3, 0, 6, 4, 3, 6, 5, 3, 8, 6, 9, 6, 2, 8, 9, 0, 6, 2, 5, 1, 1, 6, 4, 1, 5,
        3, 2, 1, 8, 2, 6, 9, 3, 4, 8, 1, 4, 4, 5, 3, 1, 2, 5, 5, 8, 2, 0, 7, 6, 6, 0, 9, 1, 3, 4,
        6, 7, 4, 0, 7, 2, 2, 6, 5, 6, 2, 5, 2, 9, 1, 0, 3, 8, 3, 0, 4, 5, 6, 7, 3, 3, 7, 0, 3, 6,
        1, 3, 2, 8, 1, 2, 5, 1, 4, 5, 5, 1, 9, 1, 5, 2, 2, 8, 3, 6, 6, 8, 5, 1, 8, 0, 6, 6, 4, 0,
        6, 2, 5, 7, 2, 7, 5, 9, 5, 7, 6, 1, 4, 1, 8, 3, 4, 2, 5, 9, 0, 3, 3, 2, 0, 3, 1, 2, 5, 3,
        6, 3, 7, 9, 7, 8, 8, 0, 7, 0, 9, 1, 7, 1, 2, 9, 5, 1, 6, 6, 0, 1, 5, 6, 2, 5, 1, 8, 1, 8,
        9, 8, 9, 4, 0, 3, 5, 4, 5, 8, 5, 6, 4, 7, 5, 8, 3, 0, 0, 7, 8, 1, 2, 5, 9, 0, 9, 4, 9, 4,
        7, 0, 1, 7, 7, 2, 9, 2, 8, 2, 3, 7, 9, 1, 5, 0, 3, 9, 0, 6, 2, 5, 4, 5, 4, 7, 4, 7, 3, 5,
        0, 8, 8, 6, 4, 6, 4, 1, 1, 8, 9, 5, 7, 5, 1, 9, 5, 3, 1, 2, 5, 2, 2, 7, 3, 7, 3, 6, 7, 5,
        4, 4, 3, 2, 3, 2, 0, 5, 9, 4, 7, 8, 7, 5, 9, 7, 6, 5, 6, 2, 5, 1, 1, 3, 6, 8, 6, 8, 3, 7,
        7, 2, 1, 6, 1, 6, 0, 2, 9, 7, 3, 9, 3, 7, 9, 8, 8, 2, 8, 1, 2, 5, 5, 6, 8, 4, 3, 4, 1, 8,
        8, 6, 0, 8, 0, 8, 0, 1, 4, 8, 6, 9, 6, 8, 9, 9, 4, 1, 4, 0, 6, 2, 5, 2, 8, 4, 2, 1, 7, 0,
        9, 4, 3, 0, 4, 0, 4, 0, 0, 7, 4, 3, 4, 8, 4, 4, 9, 7, 0, 7, 0, 3, 1, 2, 5, 1, 4, 2, 1, 0,
        8, 5, 4, 7, 1, 5, 2, 0, 2, 0, 0, 3, 7, 1, 7, 4, 2, 2, 4, 8, 5, 3, 5, 1, 5, 6, 2, 5, 7, 1,
        0, 5, 4, 2, 7, 3, 5, 7, 6, 0, 1, 0, 0, 1, 8, 5, 8, 7, 1, 1, 2, 4, 2, 6, 7, 5, 7, 8, 1, 2,
        5, 3, 5, 5, 2, 7, 1, 3, 6, 7, 8, 8, 0, 0, 5, 0, 0, 9, 2, 9, 3, 5, 5, 6, 2, 1, 3, 3, 7, 8,
        9, 0, 6, 2, 5, 1, 7, 7, 6, 3, 5, 6, 8, 3, 9, 4, 0, 0, 2, 5, 0, 4, 6, 4, 6, 7, 7, 8, 1, 0,
        6, 6, 8, 9, 4, 5, 3, 1, 2, 5, 8, 8, 8, 1, 7, 8, 4, 1, 9, 7, 0, 0, 1, 2, 5, 2, 3, 2, 3, 3,
        8, 9, 0, 5, 3, 3, 4, 4, 7, 2, 6, 5, 6, 2, 5, 4, 4, 4, 0, 8, 9, 2, 0, 9, 8, 5, 0, 0, 6, 2,
        6, 1, 6, 1, 6, 9, 4, 5, 2, 6, 6, 7, 2, 3, 6, 3, 2, 8, 1, 2, 5, 2, 2, 2, 0, 4, 4, 6, 0, 4,
        9, 2, 5, 0, 3, 1, 3, 0, 8, 0, 8, 4, 7, 2, 6, 3, 3, 3, 6, 1, 8, 1, 6, 4, 0, 6, 2, 5, 1, 1,
        1, 0, 2, 2, 3, 0, 2, 4, 6, 2, 5, 1, 5, 6, 5, 4, 0, 4, 2, 3, 6, 3, 1, 6, 6, 8, 0, 9, 0, 8,
        2, 0, 3, 1, 2, 5, 5, 5, 5, 1, 1, 1, 5, 1, 2, 3, 1, 2, 5, 7, 8, 2, 7, 0, 2, 1, 1, 8, 1, 5,
        8, 3, 4, 0, 4, 5, 4, 1, 0, 1, 5, 6, 2, 5, 2, 7, 7, 5, 5, 5, 7, 5, 6, 1, 5, 6, 2, 8, 9, 1,
        3, 5, 1, 0, 5, 9, 0, 7, 9, 1, 7, 0, 2, 2, 7, 0, 5, 0, 7, 8, 1, 2, 5, 1, 3, 8, 7, 7, 7, 8,
        7, 8, 0, 7, 8, 1, 4, 4, 5, 6, 7, 5, 5, 2, 9, 5, 3, 9, 5, 8, 5, 1, 1, 3, 5, 2, 5, 3, 9, 0,
        6, 2, 5, 6, 9, 3, 8, 8, 9, 3, 9, 0, 3, 9, 0, 7, 2, 2, 8, 3, 7, 7, 6, 4, 7, 6, 9, 7, 9, 2,
        5, 5, 6, 7, 6, 2, 6, 9, 5, 3, 1, 2, 5, 3, 4, 6, 9, 4, 4, 6, 9, 5, 1, 9, 5, 3, 6, 1, 4, 1,
        8, 8, 8, 2, 3, 8, 4, 8, 9, 6, 2, 7, 8, 3, 8, 1, 3, 4, 7, 6, 5, 6, 2, 5, 1, 7, 3, 4, 7, 2,
        3, 4, 7, 5, 9, 7, 6, 8, 0, 7, 0, 9, 4, 4, 1, 1, 9, 2, 4, 4, 8, 1, 3, 9, 1, 9, 0, 6, 7, 3,
        8, 2, 8, 1, 2, 5, 8, 6, 7, 3, 6, 1, 7, 3, 7, 9, 8, 8, 4, 0, 3, 5, 4, 7, 2, 0, 5, 9, 6, 2,
        2, 4, 0, 6, 9, 5, 9, 5, 3, 3, 6, 9, 1, 4, 0, 6, 2, 5,
    };

    shift &= 63;
    uint16_t x_a = TABLE[shift];
    uint16_t x_b = TABLE[shift + 1];
    int num_new_digits = x_a >> 11;
    int pow5_a = 0x7FF & x_a;
    int pow5_b = 0x7FF & x_b;
    for (int j = pow5_a; j < pow5_b; j++) {
        int i = j - pow5_a;
        if (i >= d->num_digits) {
            return num_new_digits - 1;
        } else if (d->digits[i] == TABLE_POW5[j]) {
            continue;
        } else if (d->digits[i] < TABLE_POW5[j]) {
            return num_new_digits - 1;
        } else {
            return num_new_digits;
        }
    }
    return num_new_digits;
}

/// Computes decimal * 2^shift.
static void left_shift(Decimal *self, int shift) {
    if (self->num_digits == 0) {
        return;
    }
    int num_new_digits = number_of_digits_decimal_left_shift(self, shift);
    int read_index = self->num_digits;
    int write_index = self->num_digits + num_new_digits;
    uint64_t n = 0;
    while (read_index != 0) {
        read_index -= 1;
        write_index -= 1;
        n += (uint64_t) self->digits[read_index] << shift;
        uint64_t quotient = n / 10;
        uint64_t remainder = n - (10 * quotient);
        if (write_index < MAX_DIGITS) {
            self->digits[write_index] = (unsigned char) remainder;
        } else if (remainder > 0) {
            self->truncated = true;
        }
        n = quotient;
    }
    while (n > 0) {
        write_index -= 1;
        uint64_t quotient = n / 10;
        uint64_t remainder = n - (10 * quotient);
        if (write_index < MAX_DIGITS) {
            self->digits[write_index] = (unsigned char) remainder;
        } else if (remainder > 0) {
            self->truncated = true;
        }
        n = quotient;
    }
    self->num_digits += num_new_digits;
    if (self->num_digits > MAX_DIGITS) {
        self->num_digits = MAX_DIGITS;
    }
    self->decimal_point += num_new_digits;
    trim(self);
}

/// Computes decimal * 2^-shift.
void right_shift(Decimal *self, int shift) {
    int read_index = 0;
    int write_index = 0;
    uint64_t n = 0;
    while ((n >> shift) == 0) {
        if (read_index < self->num_digits) {
            n = (10 * n) + (uint64_t) self->digits[read_index];
            read_index += 1;
        } else if (n == 0) {
            return;
        } else {
            while ((n >> shift) == 0) {
                n *= 10;
                read_index += 1;
            }
            break;
        }
    }
    self->decimal_point -= read_index - 1;
    if (self->decimal_point < -DECIMAL_POINT_RANGE) {
        // `self = Self::Default()`, but without the overhead of clearing `digits`.
        self->num_digits = 0;
        self->decimal_point = 0;
        self->truncated = false;
        return;
    }
    uint64_t mask = ((uint64_t) 1 << shift) - 1;
    while (read_index < self->num_digits) {
        unsigned char new_digit = (unsigned char) (n >> shift);
        n = (10 * (n & mask)) + (uint64_t) self->digits[read_index];
        read_index += 1;
        self->digits[write_index] = new_digit;
        write_index += 1;
    }
    while (n > 0) {
        unsigned char new_digit = (unsigned char) (n >> shift);
        n = 10 * (n & mask);
        if (write_index < MAX_DIGITS) {
            self->digits[write_index] = new_digit;
            write_index += 1;
        } else if (new_digit > 0) {
            self->truncated = true;
        }
    }
    self->num_digits = write_index;
    trim(self);
}

static uint64_t read_u64(String s) {
    uint64_t value = 0;
    int shift = 0;

    for (int i = 0; i < 8; i++) {
        value |= ((unsigned char) s.ptr[i]) << shift;
        shift += 8;
    }

    return value;
}

static void write_u64(unsigned char *s, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        s[i] = value >> (i * 8);
    }
}

/// Determine if 8 bytes are all decimal digits.
/// This does not care about the order in which the bytes were loaded.
static bool is_8digits(uint64_t v) {
    uint64_t a = v + 0x4646464646464646;
    uint64_t b = v - 0x3030303030303030;
    return ((a | b) & 0x8080808080808080) == 0;
}

/// Parse a big integer representation of the float as a decimal.
static Decimal parse_decimal(String s) {
    Decimal d = {0};
    String start = s;

    while (s.len && s.ptr[0] == '0') {
        s = substring(s, 1, s.len);
    }

    while (s.len && s.ptr[0] >= '0' && s.ptr[0] <= '9') {
        try_add_digit(&d, s.ptr[0] - '0');
        s = substring(s, 1, s.len);
    }

    if (s.len && s.ptr[0] == '.') {
        s = substring(s, 1, s.len);
        String first = s;
        // Skip leading zeros.
        if (d.num_digits == 0) {
            while (s.len && s.ptr[0] == '0') {
                s = substring(s, 1, s.len);
            }
        }
        while (s.len >= 8 && d.num_digits + 8 < MAX_DIGITS) {
            uint64_t v = read_u64(s);
            if (!is_8digits(v)) {
                break;
            }
            write_u64(d.digits + d.num_digits, v - 0x3030303030303030);
            d.num_digits += 8;
            s = substring(s, 8, s.len);
        }
        while (s.len && s.ptr[0] >= '0' && s.ptr[0] <= '9') {
            try_add_digit(&d, s.ptr[0] - '0');
            s = substring(s, 1, s.len);
        }
        d.decimal_point = (int) (s.len - first.len);
    }
    if (d.num_digits != 0) {
        // Ignore the trailing zeros if there are any
        int n_trailing_zeros = 0;
        for (ptrdiff_t i = s.len - 1; i >= start.len; i--) {
            if (s.ptr[i] == '0') {
                n_trailing_zeros += 1;
            } else if (s.ptr[i] != '.') {
                break;
            }
        }
        d.decimal_point += n_trailing_zeros;
        d.num_digits -= n_trailing_zeros;
        d.decimal_point += d.num_digits;
        if (d.num_digits > MAX_DIGITS) {
            d.truncated = true;
            d.num_digits = MAX_DIGITS;
        }
    }
    if (s.len) {
        unsigned char ch = s.ptr[0];
        if (ch == 'e' || ch == 'E') {
            s = substring(s, 1, s.len);
            bool neg_exp = false;
            if (s.len) {
                unsigned char ch2 = s.ptr[0];
                neg_exp = ch2 == '-';
                if (ch2 == '-' || ch2 == '+') {
                    s = substring(s, 1, s.len);
                }
            }
            int exp_num = 0;

            while (s.len && s.ptr[0] >= '0' && s.ptr[0] <= '9') {
                if (exp_num < 0x10000) {
                    exp_num = 10 * exp_num + (int) (s.ptr[0] - '0');
                }
                s = substring(s, 1, s.len);
            }

            d.decimal_point += neg_exp ? -exp_num : exp_num;
        }
    }
    for (int i = d.num_digits; i < MAX_DIGITS_WITHOUT_OVERFLOW; i++) {
        d.digits[i] = 0;
    }
    return d;
}

/// A custom 64-bit floating point type, representing `f * 2^e`.
/// e is biased, so it be directly shifted into the exponent bits.
typedef struct {
    /// The significant digits.
    uint64_t f;
    /// The biased, binary exponent.
    int32_t e;
} BiasedFp;

static int get_shift(int n) {
    #define NUM_POWERS 19
    #define MAX_SHIFT 60
    static unsigned char const POWERS[] =
        {0, 3, 6, 9, 13, 16, 19, 23, 26, 29, 33, 36, 39, 43, 46, 49, 53, 56, 59};
    if (n < NUM_POWERS) {
        return POWERS[n];
    }
    return MAX_SHIFT;
}

static int const MANTISSA_EXPLICIT_BITS = 52;
static int const INFINITE_POWER = 0x7FF;
static int const MINIMUM_EXPONENT = -1023;

/// Parse the significant digits and biased, binary exponent of a float.
///
/// This is a fallback algorithm that uses a big-integer representation
/// of the float, and therefore is considerably slower than faster
/// approximations. However, it will always determine how to round
/// the significant digits to the nearest machine float, allowing
/// use to handle near half-way cases.
///
/// Near half-way cases are halfway between two consecutive machine floats.
/// For example, the float `16777217.0` has a bitwise representation of
/// `100000000000000000000000 1`. Rounding to a single-precision float,
/// the trailing `1` is truncated. Using round-nearest, tie-even, any
/// value above `16777217.0` must be rounded up to `16777218.0`, while
/// any value before or equal to `16777217.0` must be rounded down
/// to `16777216.0`. These near-halfway conversions therefore may require
/// a large number of digits to unambiguously determine how to round.
///
/// The algorithms described here are based on "Processing Long Numbers Quickly",
/// available here: <https://arxiv.org/pdf/2101.11408.pdf#section.11>.
static double parse_long_mantissa(String s) {
    Decimal d = parse_decimal(s);

    union {
        uint64_t bits;
        double value;
    } result;

    // Short-circuit if the value can only be a literal 0 or infinity.
    if (d.num_digits == 0 || d.decimal_point < -324) {
        return 0.0;
    } else if (d.decimal_point >= 310) {
        result.bits = 0x7FF0000000000000;
        return result.value;
    }
    int exp2 = 0;
    // Shift right toward (1/2 ... 1].
    while (d.decimal_point > 0) {
        int shift = get_shift(d.decimal_point);
        right_shift(&d, shift);
        if (d.decimal_point < -DECIMAL_POINT_RANGE) {
            return 0.0;
        }
        exp2 += shift;
    }
    // Shift left toward (1/2 ... 1].
    while (d.decimal_point <= 0) {
        int shift;

        if (d.decimal_point == 0) {
            if (d.digits[0] >= 5) {
                break;
            }

            if (d.digits[0] == '0' || d.digits[0] == '1') {
                shift = 2;
            } else {
                shift = 1;
            }
        } else {
            shift = get_shift(-d.decimal_point);
        };
        left_shift(&d, shift);
        if (d.decimal_point > DECIMAL_POINT_RANGE) {
            result.bits = 0x7FF0000000000000;
            return result.value;
        }
        exp2 -= shift;
    }
    // We are now in the range [1/2 ... 1] but the binary format uses [1 ... 2].
    exp2 -= 1;
    while ((MINIMUM_EXPONENT + 1) > exp2) {
        int n = (MINIMUM_EXPONENT + 1) - exp2;
        if (n > MAX_SHIFT) {
            n = MAX_SHIFT;
        }
        right_shift(&d, n);
        exp2 += n;
    }
    if ((exp2 - MINIMUM_EXPONENT) >= INFINITE_POWER) {
        result.bits = 0x7FF0000000000000;
        return result.value;
    }
    // Shift the decimal to the hidden bit, and then round the value
    // to get the high mantissa+1 bits.
    left_shift(&d, MANTISSA_EXPLICIT_BITS + 1);
    uint64_t mantissa = round_decimal(&d);
    if (mantissa >= ((uint64_t) 1 << (MANTISSA_EXPLICIT_BITS + 1))) {
        // Rounding up overflowed to the carry bit, need to
        // shift back to the hidden bit.
        right_shift(&d, 1);
        exp2 += 1;
        mantissa = round_decimal(&d);
        if ((exp2 - MINIMUM_EXPONENT) >= INFINITE_POWER) {
            result.bits = 0x7FF0000000000000;
            return result.value;
        }
    }
    int power2 = exp2 - MINIMUM_EXPONENT;
    if (mantissa < ((uint64_t) 1 << MANTISSA_EXPLICIT_BITS)) {
        power2 -= 1;
    }
    // Zero out all the bits above the explicit mantissa bits.
    mantissa &= ((uint64_t) 1 << MANTISSA_EXPLICIT_BITS) - 1;

    result.bits = mantissa | ((uint64_t) power2 << MANTISSA_EXPLICIT_BITS);
    return result.value;
}

double parse_float(String s, Arena scratch) {
    char *raw_string = arena_alloc(&scratch, char, s.len);
    ptrdiff_t length = 0;
    for (ptrdiff_t i = 0; i < s.len; i++) {
        if (s.ptr[i] == '_') {
            continue;
        }
        raw_string[length++] = s.ptr[i];
    }
    return parse_long_mantissa((String) {length, raw_string});
}
