/**
 * @file    bsp_format.c
 * @brief   Mini formatter replacing the newlib printf family.
 * @details Compiled only when BSP_LOG_USE_LIBC_PRINTF is 0. Output goes
 *          through a small sink that counts every character but stores only
 *          what fits, which gives the C99 snprintf return value for free.
 * @author  <author name>
 * @date    <yyyy-mm-dd>
 */

#include "bsp_format.h"

#if !BSP_LOG_USE_LIBC_PRINTF

#include <stdint.h>

/** Output sink: destination buffer plus the number of characters produced. */
typedef struct {
    char   *buf;    /**< Destination buffer.                              */
    size_t  size;   /**< Buffer size in bytes.                            */
    size_t  pos;    /**< Characters produced so far (may exceed size).    */
} fmt_out_t;

/** Largest field width accepted; larger values are clamped. */
#define FMT_WIDTH_MAX   255U

/**
 * @brief Append one character, storing it only if it fits.
 *
 * @param[in,out] o  Output sink.
 * @param[in]     c  Character to append.
 */
static void out_char(fmt_out_t *o, char c)
{
    if ((o->size > 0U) && (o->pos < (o->size - 1U))) {
        o->buf[o->pos] = c;
    }
    o->pos++;
}

/**
 * @brief Append a character several times.
 *
 * @param[in,out] o      Output sink.
 * @param[in]     c      Character to repeat.
 * @param[in]     count  Number of repetitions.
 */
static void out_pad(fmt_out_t *o, char c, uint16_t count)
{
    while (count > 0U) {
        out_char(o, c);
        count--;
    }
}

/**
 * @brief Append an unsigned number with padding.
 *
 * @param[in,out] o      Output sink.
 * @param[in]     value  Magnitude to print.
 * @param[in]     base   10 or 16.
 * @param[in]     upper  1 for upper-case hex digits.
 * @param[in]     neg    1 to print a leading '-'.
 * @param[in]     left   1 for left-justify (pad on the right with spaces).
 * @param[in]     zero   1 to pad with zeros (ignored when @p left is 1).
 * @param[in]     width  Minimum field width.
 */
static void out_number(fmt_out_t *o, uint32_t value, uint32_t base, uint8_t upper,
                       uint8_t neg, uint8_t left, uint8_t zero, uint16_t width)
{
    char tmp[10];                       /* 32-bit decimal has at most 10 digits */
    uint8_t n = 0U;
    uint16_t total;
    uint16_t pad;
    const char *digits = (upper != 0U) ? "0123456789ABCDEF" : "0123456789abcdef";

    do {
        tmp[n] = digits[value % base];
        n++;
        value /= base;
    } while (value != 0U);

    total = (uint16_t)((uint16_t)n + (uint16_t)neg);
    pad = (width > total) ? (uint16_t)(width - total) : 0U;

    if ((left == 0U) && (zero == 0U)) {
        out_pad(o, ' ', pad);
    }
    if (neg != 0U) {
        out_char(o, '-');
    }
    if ((left == 0U) && (zero != 0U)) {
        out_pad(o, '0', pad);
    }
    while (n > 0U) {
        n--;
        out_char(o, tmp[n]);
    }
    if (left != 0U) {
        out_pad(o, ' ', pad);
    }
}

/**
 * @brief Append a string with padding and optional precision.
 *
 * @param[in,out] o      Output sink.
 * @param[in]     s      NUL-terminated string.
 * @param[in]     left   1 for left-justify.
 * @param[in]     width  Minimum field width.
 * @param[in]     prec   Maximum number of characters, or -1 for no limit.
 */
static void out_string(fmt_out_t *o, const char *s, uint8_t left, uint16_t width, int prec)
{
    uint16_t len = 0U;
    uint16_t pad;

    while ((s[len] != '\0') && ((prec < 0) || ((int)len < prec))) {
        len++;
    }
    pad = (width > len) ? (uint16_t)(width - len) : 0U;

    if (left == 0U) {
        out_pad(o, ' ', pad);
    }
    for (uint16_t i = 0U; i < len; i++) {
        out_char(o, s[i]);
    }
    if (left != 0U) {
        out_pad(o, ' ', pad);
    }
}

int Bsp_Vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    fmt_out_t o;

    o.buf = buf;
    o.size = size;
    o.pos = 0U;

    while (*fmt != '\0') {
        char c = *fmt++;
        uint8_t left = 0U;
        uint8_t zero = 0U;
        uint8_t is_long = 0U;
        uint16_t width = 0U;
        int prec = -1;
        char conv;

        if (c != '%') {
            out_char(&o, c);
            continue;
        }

        /* Flags. */
        for (;;) {
            if (*fmt == '-') {
                left = 1U;
            } else if (*fmt == '0') {
                zero = 1U;
            } else {
                break;
            }
            fmt++;
        }

        /* Width. */
        while ((*fmt >= '0') && (*fmt <= '9')) {
            width = (uint16_t)((width * 10U) + (uint16_t)(*fmt - '0'));
            if (width > FMT_WIDTH_MAX) {
                width = FMT_WIDTH_MAX;
            }
            fmt++;
        }

        /* Precision (used by %s only). */
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            while ((*fmt >= '0') && (*fmt <= '9')) {
                if (prec < 1000) {
                    prec = (prec * 10) + (*fmt - '0');
                }
                fmt++;
            }
        }

        /* Length modifiers: 'l' selects long (32-bit); 'h' is ignored. */
        while ((*fmt == 'l') || (*fmt == 'h')) {
            if (*fmt == 'l') {
                is_long = 1U;
            }
            fmt++;
        }

        conv = *fmt;
        if (conv == '\0') {
            break;
        }
        fmt++;

        switch (conv) {
        case 'd':
        case 'i': {
            int32_t v = (is_long != 0U) ? (int32_t)va_arg(ap, long)
                                        : (int32_t)va_arg(ap, int);
            uint8_t neg = (uint8_t)((v < 0) ? 1U : 0U);
            uint32_t mag = (neg != 0U) ? ((uint32_t)0U - (uint32_t)v) : (uint32_t)v;

            out_number(&o, mag, 10U, 0U, neg, left, zero, width);
            break;
        }
        case 'u': {
            uint32_t v = (is_long != 0U) ? (uint32_t)va_arg(ap, unsigned long)
                                         : (uint32_t)va_arg(ap, unsigned int);

            out_number(&o, v, 10U, 0U, 0U, left, zero, width);
            break;
        }
        case 'x':
        case 'X': {
            uint32_t v = (is_long != 0U) ? (uint32_t)va_arg(ap, unsigned long)
                                         : (uint32_t)va_arg(ap, unsigned int);

            out_number(&o, v, 16U, (conv == 'X') ? 1U : 0U, 0U, left, zero, width);
            break;
        }
        case 'c': {
            char tmp[2];

            tmp[0] = (char)va_arg(ap, int);
            tmp[1] = '\0';
            out_string(&o, tmp, left, width, -1);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);

            out_string(&o, (s != NULL) ? s : "(null)", left, width, prec);
            break;
        }
        case '%':
            out_char(&o, '%');
            break;
        default:
            /* Unsupported conversion: print it literally. */
            out_char(&o, '%');
            out_char(&o, conv);
            break;
        }
    }

    if (size > 0U) {
        buf[(o.pos < size) ? o.pos : (size - 1U)] = '\0';
    }
    return (int)o.pos;
}

int Bsp_Snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = Bsp_Vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

#endif /* !BSP_LOG_USE_LIBC_PRINTF */
