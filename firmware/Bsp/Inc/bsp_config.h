/**
 * @file    bsp_config.h
 * @brief   Compile-time configuration of the BSP debug and UART modules.
 * @details Contains only macros and no HAL include, so it is safe to include
 *          from any layer (App or Bsp).
 *
 *          Every macro can be overridden from the compiler command line
 *          (for example `-DBSP_LOG_LEVEL=4`) or by defining it before this
 *          header is included.
 *
 *          Profiles
 *          --------
 *          BSP_PROFILE selects the defaults of all memory-related options at
 *          once:
 *            - BSP_PROFILE_LITE (default): for small parts (for example
 *              16 KB flash / 4 KB RAM). Heavy features are OFF and buffers
 *              are small.
 *            - BSP_PROFILE_FULL: for larger parts. Heavy features are ON.
 *          Any single option can still be overridden on top of a profile.
 *
 *          Option           | LITE | FULL | What it costs when ON (estimate)
 *          -----------------|------|------|----------------------------------
 *          LIBC_PRINTF      |  0   |  1   | newlib vsnprintf, several KB flash
 *          ENABLE_HEXDUMP   |  0   |  1   | hundreds of bytes flash
 *          LOG_USE_DMA      |  0   |  1   | HAL DMA code + a DMA channel
 *          RX_ENABLE        |  0   |  1   | RX driver + command line, ~2 KB
 *          CMD_USE_LIBC_... |  0   |  1   | strtoul, ~1 KB flash
 *          buffer sizes     | small| big  | RAM
 *
 *          The figures are estimates; measure with the Build Analyzer.
 *
 *          Porting to another MCU: change BSP_DEBUG_UART, choose the FULL
 *          profile, and adapt the port-specific lines in uart_comm.c.
 */

#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

#if defined(__GNUC__)
/** Enables compile-time printf-style format checking on GCC. */
#define BSP_PRINTF_FORMAT(fmt_idx, arg_idx) \
    __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define BSP_PRINTF_FORMAT(fmt_idx, arg_idx)
#endif

/* ------------------------------------------------------------------------- */
/* Profile                                                                   */
/* ------------------------------------------------------------------------- */

#define BSP_PROFILE_LITE    1   /**< Small flash/RAM: heavy features OFF. */
#define BSP_PROFILE_FULL    2   /**< Large parts: heavy features ON.      */

/** Selected profile. Default: LITE. */
#ifndef BSP_PROFILE
#define BSP_PROFILE         BSP_PROFILE_LITE
#endif

#if (BSP_PROFILE == BSP_PROFILE_FULL)
/** 1 when the FULL profile is active, 0 for LITE. Used for option defaults. */
#define BSP_DEF_FULL        1
#else
#define BSP_DEF_FULL        0
#endif

/* ------------------------------------------------------------------------- */
/* UART channel selection                                                    */
/* ------------------------------------------------------------------------- */

/**
 * @brief Name of the CubeMX-generated UART handle used for debug
 *        (for example huart1, huart2, huart3).
 * @note  This is the variable name, not a pointer. The code takes its
 *        address with &.
 */
#ifndef BSP_DEBUG_UART
#define BSP_DEBUG_UART      huart1
#endif

/**
 * @brief UART handle used to transmit log output.
 * @details Defaults to @ref BSP_DEBUG_UART. Override to send logs on a
 *          different UART than the one used to receive commands.
 */
#ifndef BSP_LOG_UART
#define BSP_LOG_UART        BSP_DEBUG_UART
#endif

/**
 * @brief UART handle used to receive debug commands.
 * @details Defaults to @ref BSP_DEBUG_UART.
 */
#ifndef BSP_RX_UART
#define BSP_RX_UART         BSP_DEBUG_UART
#endif

/* ------------------------------------------------------------------------- */
/* Log levels                                                                */
/* ------------------------------------------------------------------------- */

#define BSP_LOG_LEVEL_NONE      0U  /**< Logging disabled.            */
#define BSP_LOG_LEVEL_ERROR     1U  /**< Errors only.                 */
#define BSP_LOG_LEVEL_WARN      2U  /**< Errors and warnings.         */
#define BSP_LOG_LEVEL_INFO      3U  /**< Normal information.          */
#define BSP_LOG_LEVEL_DEBUG     4U  /**< Verbose debug output.        */

/**
 * @brief Compile-time maximum log level.
 * @details Log macros above this level are removed by the compiler and cost
 *          no flash or CPU time. The run-time level (Log_SetLevel) can only
 *          lower the output below this value, never raise it.
 */
#ifndef BSP_LOG_LEVEL
#define BSP_LOG_LEVEL       BSP_LOG_LEVEL_INFO
#endif

/* ------------------------------------------------------------------------- */
/* Log transmit options                                                      */
/* ------------------------------------------------------------------------- */

/** Size of the log ring buffer in bytes. Must be a power of two (<= 32768). */
#ifndef BSP_LOG_BUF_SIZE
#define BSP_LOG_BUF_SIZE    128U
#endif

/** Maximum length of one formatted log line in bytes, including "\r\n". */
#ifndef BSP_LOG_LINE_MAX
#define BSP_LOG_LINE_MAX    96U
#endif

/** Set to 1 to prefix each log line with the tick in milliseconds. */
#ifndef BSP_LOG_TIMESTAMP
#define BSP_LOG_TIMESTAMP   1
#endif

/**
 * @brief Set to 1 to transmit with DMA, 0 to transmit with interrupts.
 * @details Use 0 when the log UART has no free DMA channel. Both modes
 *          report completion through HAL_UART_TxCpltCallback.
 */
#ifndef BSP_LOG_USE_DMA
#define BSP_LOG_USE_DMA     1
#endif

/**
 * @brief Tick source (milliseconds) used for log timestamps.
 * @note  Replace with Bsp_Millis() once the timer module is available.
 */
#ifndef BSP_LOG_GET_TICK
#define BSP_LOG_GET_TICK()  HAL_GetTick()
#endif

/* ------------------------------------------------------------------------- */
/* UART receive options                                                      */
/* ------------------------------------------------------------------------- */

/**
 * @brief Size of the circular DMA receive buffer in bytes.
 * @details Must be a power of two (<= 32768). Usable capacity is one byte
 *          less. At 115200 baud, 256 bytes fill in about 22 ms.
 */
#ifndef BSP_RX_DMA_SIZE
#define BSP_RX_DMA_SIZE     256U
#endif

#endif /* BSP_CONFIG_H */
