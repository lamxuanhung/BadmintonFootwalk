/**
 * @file    uart_comm.h
 * @brief   UART receive driver based on circular DMA.
 * @details DMA writes received bytes into a ring buffer without CPU help.
 *          The main loop reads them with Uart_RxRead() / Uart_RxReadBuf().
 *          The UART channel is chosen with the BSP_RX_UART macro in
 *          bsp_config.h; this API does not depend on a specific UART.
 *
 *          Overwrite detection: the driver counts how many times DMA wrapped
 *          around, so it can tell a full buffer from an empty one and can
 *          report when unread data was overwritten (see
 *          Uart_RxOverflowTaken()).
 *
 *          When BSP_RX_ENABLE is 0 the whole driver is removed and every
 *          function below becomes an empty inline stub (returns 0), so
 *          application code compiles unchanged.
 *
 * @warning All functions must be called from one context only (the main
 *          loop). Usable buffer capacity is BSP_RX_DMA_SIZE - 1 bytes.
 */

#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdint.h>
#include "bsp_config.h"

/** @defgroup BSP_UART  UART receive
 *  @{ */

#if BSP_RX_ENABLE

/**
 * @brief   Start receiving with circular DMA.
 * @details Resets the read position and discards earlier data. Call once
 *          after CubeMX initialization.
 *
 * @retval 1  Reception started.
 * @retval 0  HAL refused to start (UART or DMA not initialized or busy).
 *
 * @note    CubeMX: DMA RX request in Circular mode, USART global interrupt
 *          and DMA channel interrupt enabled.
 */
uint8_t Uart_RxStart(void);

/**
 * @brief   Get the number of bytes waiting to be read.
 * @details Also recovers the receiver after a UART error and detects
 *          overwritten data. If data was overwritten, all unread data is
 *          dropped, the overflow flag is set and 0 is returned.
 *
 * @return  Number of unread bytes, 0 to BSP_RX_DMA_SIZE - 1.
 */
uint16_t Uart_RxAvailable(void);

/**
 * @brief   Read one byte.
 * @details Non-blocking.
 *
 * @param[out] byte  Receives the byte read. Must not be NULL.
 *
 * @retval 1  A byte was read.
 * @retval 0  Nothing to read; @p byte is unchanged.
 */
uint8_t Uart_RxRead(uint8_t *byte);

/**
 * @brief   Read up to @p max bytes at once.
 * @details Non-blocking. Faster than calling Uart_RxRead() in a loop.
 *
 * @param[out] buf  Destination buffer. Must not be NULL.
 * @param[in]  max  Size of @p buf (unit: byte).
 *
 * @return  Number of bytes copied into @p buf (0 if none available).
 */
uint16_t Uart_RxReadBuf(uint8_t *buf, uint16_t max);

/**
 * @brief   Report and clear the "unread data was lost" flag.
 * @details Set when the main loop was too slow and DMA overwrote unread
 *          data, or when the receiver had to be restarted after an error.
 *          Callers that assemble lines or frames must discard the partial
 *          data when this returns 1.
 *
 * @retval 1  Data was lost since the last call.
 * @retval 0  No loss.
 */
uint8_t Uart_RxOverflowTaken(void);

/**
 * @brief   Get the number of UART receive errors since reset.
 * @return  Error count (overrun, noise, framing, parity, DMA error).
 */
uint32_t Uart_GetErrorCount(void);

/**
 * @brief   Get the number of buffer overwrites since reset.
 * @return  Overwrite count.
 */
uint32_t Uart_GetOverflowCount(void);

#else /* BSP_RX_ENABLE == 0: empty stubs */

static inline uint8_t  Uart_RxStart(void)                          { return 0U; }
static inline uint16_t Uart_RxAvailable(void)                      { return 0U; }
static inline uint8_t  Uart_RxRead(uint8_t *byte)                  { (void)byte; return 0U; }
static inline uint16_t Uart_RxReadBuf(uint8_t *buf, uint16_t max)  { (void)buf; (void)max; return 0U; }
static inline uint8_t  Uart_RxOverflowTaken(void)                  { return 0U; }
static inline uint32_t Uart_GetErrorCount(void)                    { return 0U; }
static inline uint32_t Uart_GetOverflowCount(void)                 { return 0U; }

#endif /* BSP_RX_ENABLE */

/** @} */

#endif /* UART_COMM_H */
