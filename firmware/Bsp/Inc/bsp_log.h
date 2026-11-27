/**
 * @file    bsp_log.h
 * @brief   Non-blocking logging over UART (ring buffer + DMA/interrupt TX).
 * @details Log text is copied into a ring buffer and sent in the background,
 *          so logging never waits for the UART. When the buffer is full a
 *          whole message is dropped (and counted) instead of blocking.
 *
 *          The UART channel is chosen with the BSP_LOG_UART macro in
 *          bsp_config.h; this API does not depend on a specific UART.
 *          Memory-heavy features are switched by macros (see the profile
 *          table in bsp_config.h).
 *
 *          Usage:
 * @code
 *   LOG_I("boot, state=%u", state);
 *   LOG_E("nRF self test failed");
 *   LOG_HEX("pkt", buf, len);          // only if BSP_LOG_ENABLE_HEXDUMP
 *   Log_Printf("raw text without prefix\r\n");
 * @endcode
 *
 * @warning All functions must be called from one context only (the main
 *          loop). Do not log from an ISR.
 */

#ifndef BSP_LOG_H
#define BSP_LOG_H

#include <stdint.h>
#include "bsp_config.h"

/** @defgroup BSP_LOG  UART logging
 *  @{ */

/* ------------------------------------------------------------------------- */
/* Log macros (compiled out above BSP_LOG_LEVEL)                             */
/* ------------------------------------------------------------------------- */

#if BSP_LOG_LEVEL >= BSP_LOG_LEVEL_ERROR
/** Log an error. Arguments are a printf format and its values. */
#define LOG_E(...)  Log_Print(BSP_LOG_LEVEL_ERROR, __VA_ARGS__)
#else
#define LOG_E(...)  do { if (0) { Log_Print(BSP_LOG_LEVEL_ERROR, __VA_ARGS__); } } while (0)
#endif

#if BSP_LOG_LEVEL >= BSP_LOG_LEVEL_WARN
/** Log a warning. */
#define LOG_W(...)  Log_Print(BSP_LOG_LEVEL_WARN, __VA_ARGS__)
#else
#define LOG_W(...)  do { if (0) { Log_Print(BSP_LOG_LEVEL_WARN, __VA_ARGS__); } } while (0)
#endif

#if BSP_LOG_LEVEL >= BSP_LOG_LEVEL_INFO
/** Log normal information. */
#define LOG_I(...)  Log_Print(BSP_LOG_LEVEL_INFO, __VA_ARGS__)
#else
#define LOG_I(...)  do { if (0) { Log_Print(BSP_LOG_LEVEL_INFO, __VA_ARGS__); } } while (0)
#endif

#if BSP_LOG_LEVEL >= BSP_LOG_LEVEL_DEBUG
/** Log verbose debug information. */
#define LOG_D(...)  Log_Print(BSP_LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define LOG_D(...)  do { if (0) { Log_Print(BSP_LOG_LEVEL_DEBUG, __VA_ARGS__); } } while (0)
#endif

#if BSP_LOG_ENABLE_HEXDUMP
#if BSP_LOG_LEVEL >= BSP_LOG_LEVEL_DEBUG
/** Log a hex dump at debug level: LOG_HEX(label, data, len). */
#define LOG_HEX(label, data, len) \
    Log_HexDump(BSP_LOG_LEVEL_DEBUG, (label), (data), (len))
#else
#define LOG_HEX(label, data, len) \
    do { if (0) { Log_HexDump(BSP_LOG_LEVEL_DEBUG, (label), (data), (len)); } } while (0)
#endif
#else
/* Hex dump disabled: the macro stays valid so application code compiles. */
#define LOG_HEX(label, data, len) \
    do { (void)(label); (void)(data); (void)(len); } while (0)
#endif

/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief   Copy raw bytes into the log ring buffer and start transmission.
 * @details Non-blocking. The message is accepted entirely or not at all:
 *          if there is not enough free space the whole message is dropped
 *          and the drop counter is incremented.
 *
 * @param[in] data  Bytes to send. Must not be NULL.
 * @param[in] len   Number of bytes (unit: byte).
 *
 * @return Number of bytes accepted (@p len on success, 0 if dropped).
 */
uint16_t Log_Write(const char *data, uint16_t len);

/**
 * @brief   Format text with printf rules and send it without any prefix.
 * @details Output is truncated to BSP_LOG_LINE_MAX - 1 characters. Does not
 *          append a line ending; include "\r\n" yourself.
 *
 * @param[in] fmt  printf-style format string.
 *
 * @note    With BSP_LOG_USE_LIBC_PRINTF = 0 only the subset described in
 *          bsp_format.h is available (no %f).
 */
void Log_Printf(const char *fmt, ...) BSP_PRINTF_FORMAT(1, 2);

/**
 * @brief   Format and send one log line with level tag and timestamp.
 * @details Output looks like "[12345][I] message\r\n" (timestamp only when
 *          BSP_LOG_TIMESTAMP is 1). Lines above the run-time level are
 *          ignored. Prefer the LOG_E/LOG_W/LOG_I/LOG_D macros.
 *
 * @param[in] level  One of BSP_LOG_LEVEL_ERROR..BSP_LOG_LEVEL_DEBUG.
 * @param[in] fmt    printf-style format string (no line ending needed).
 */
void Log_Print(uint8_t level, const char *fmt, ...) BSP_PRINTF_FORMAT(2, 3);

#if BSP_LOG_ENABLE_HEXDUMP
/**
 * @brief   Send a labelled hex dump as one log line.
 * @details Output looks like "[..][D] label[4]: DE AD BE EF". Long data is
 *          cut and ends with "...". Available only when
 *          BSP_LOG_ENABLE_HEXDUMP is 1.
 *
 * @param[in] level  Log level of the line.
 * @param[in] label  Short text printed before the data. May be NULL.
 * @param[in] data   Bytes to dump. Must not be NULL when @p len > 0.
 * @param[in] len    Number of bytes (unit: byte).
 */
void Log_HexDump(uint8_t level, const char *label, const uint8_t *data, uint16_t len);
#endif

/**
 * @brief   Set the run-time log level.
 * @details Can only lower the output below BSP_LOG_LEVEL, because macros
 *          above that level are removed at compile time.
 *
 * @param[in] level  BSP_LOG_LEVEL_NONE..BSP_LOG_LEVEL_DEBUG (clamped).
 */
void Log_SetLevel(uint8_t level);

/**
 * @brief   Get the current run-time log level.
 * @return  Current level, BSP_LOG_LEVEL_NONE..BSP_LOG_LEVEL_DEBUG.
 */
uint8_t Log_GetLevel(void);

/**
 * @brief   Get the number of messages dropped because the buffer was full.
 * @return  Drop count since reset.
 */
uint32_t Log_GetDroppedCount(void);

/**
 * @brief   Check whether all queued log data has been transmitted.
 * @retval  1  Buffer empty and no transfer in progress.
 * @retval  0  Data still waiting or being sent.
 */
uint8_t Log_IsIdle(void);

/**
 * @brief   Service routine; restarts transmission if it stalled.
 * @details Call once per main-loop iteration. Cheap when idle. It recovers
 *          from a UART error that aborted a transfer, and retries after a
 *          busy UART refused a start request.
 */
void Log_Task(void);

/** @} */

#endif /* BSP_LOG_H */
