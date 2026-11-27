/**
 * @file    bsp_format.h
 * @brief   Selectable string formatter: C library or built-in mini version.
 * @details The rest of the BSP formats text only through the macros
 *          BSP_VSNPRINTF and BSP_SNPRINTF. Depending on
 *          BSP_LOG_USE_LIBC_PRINTF they map to the C library functions or to
 *          the small formatter in bsp_format.c, which avoids pulling the
 *          newlib printf family (several KB of flash) into the image.
 *
 *          Supported by the mini formatter: %d %i %u %x %X %c %s %%, flags
 *          '-' and '0', field width, the 'l' modifier (32-bit), precision for
 *          %s. Anything else is printed literally. Floating point and 64-bit
 *          integers are not supported.
 */

#ifndef BSP_FORMAT_H
#define BSP_FORMAT_H

#include <stddef.h>
#include <stdarg.h>
#include "bsp_config.h"

#if BSP_LOG_USE_LIBC_PRINTF

#include <stdio.h>
#define BSP_VSNPRINTF   vsnprintf
#define BSP_SNPRINTF    snprintf

#else

/**
 * @brief   Mini replacement for vsnprintf.
 * @details Follows the C99 contract: writes at most size - 1 characters plus
 *          a terminating NUL (when size > 0) and returns the length the full
 *          output would have had, so truncation can be detected.
 *
 * @param[out] buf   Destination buffer. May be NULL when @p size is 0.
 * @param[in]  size  Size of @p buf in bytes.
 * @param[in]  fmt   Format string (see the file description for the subset).
 * @param[in]  ap    Argument list.
 *
 * @return Length of the complete formatted text, without the NUL.
 */
int Bsp_Vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/**
 * @brief   Mini replacement for snprintf.
 *
 * @param[out] buf   Destination buffer.
 * @param[in]  size  Size of @p buf in bytes.
 * @param[in]  fmt   Format string.
 *
 * @return Length of the complete formatted text, without the NUL.
 */
int Bsp_Snprintf(char *buf, size_t size, const char *fmt, ...) BSP_PRINTF_FORMAT(3, 4);

#define BSP_VSNPRINTF   Bsp_Vsnprintf
#define BSP_SNPRINTF    Bsp_Snprintf

#endif /* BSP_LOG_USE_LIBC_PRINTF */

#endif /* BSP_FORMAT_H */
