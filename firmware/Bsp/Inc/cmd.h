/**
 * @file    cmd.h
 * @brief   Text command line for debugging over UART.
 * @details Bytes from the UART are assembled into lines; a line is executed
 *          when '\n' arrives. Commands are matched by name and receive their
 *          arguments like a main() function (argc, argv).
 *
 *          Built-in commands: help, log, status. Application modules add
 *          their own commands with Cmd_Register().
 *
 *          This module only uses the Bsp API (uart_comm.h, bsp_log.h); it
 *          has no dependency on HAL.
 *
 *          Usage:
 * @code
 *   static void cmd_speed(uint8_t argc, char *argv[]) { ... }
 *
 *   Cmd_Register("speed", "speed <n>: set step time (100 ms)", cmd_speed);
 *   ...
 *   Cmd_Poll(Bsp_Millis());     // every main-loop iteration
 * @endcode
 */

#ifndef CMD_H
#define CMD_H

#include <stdint.h>

/** @defgroup APP_CMD  Debug command line
 *  @{ */

/** Maximum command line length in characters (without line ending). */
#ifndef CMD_LINE_MAX
#define CMD_LINE_MAX            32U
#endif

/** Maximum number of tokens in one line (command name plus arguments). */
#ifndef CMD_MAX_ARGS
#define CMD_MAX_ARGS            4U
#endif

/** Maximum number of commands added with Cmd_Register(). */
#ifndef CMD_MAX_CUSTOM
#define CMD_MAX_CUSTOM          8U
#endif

/** A partially typed line is discarded after this idle time (unit: ms). */
#ifndef CMD_TIMEOUT_MS
#define CMD_TIMEOUT_MS          1000U
#endif

/** Maximum number of bytes taken from the UART in one Cmd_Poll() call. */
#ifndef CMD_MAX_BYTES_PER_POLL
#define CMD_MAX_BYTES_PER_POLL  32U
#endif

/**
 * @brief Command handler prototype.
 *
 * @param[in] argc  Number of tokens, at least 1 (argv[0] is the command name).
 * @param[in] argv  Tokens. Valid only during the call.
 */
typedef void (*cmd_handler_t)(uint8_t argc, char *argv[]);

/**
 * @brief   Register an application command.
 *
 * @param[in] name     Command name. The string must stay valid (use a literal).
 * @param[in] help     One-line help text shown by "help". Same lifetime rule.
 * @param[in] handler  Function called when the command is entered.
 *
 * @retval 1  Command registered.
 * @retval 0  Invalid argument or the custom command table is full.
 */
uint8_t Cmd_Register(const char *name, const char *help, cmd_handler_t handler);

/**
 * @brief   Feed one received byte into the line assembler.
 * @details Keeps its state between calls, so a command split across several
 *          polls is assembled correctly. '\r' is ignored, '\n' runs the line,
 *          backspace (0x08 or 0x7F) deletes the last character, other
 *          control characters are ignored. A line longer than
 *          CMD_LINE_MAX - 1 is rejected as a whole.
 *
 * @param[in] c  Received byte.
 */
void Cmd_Feed(uint8_t c);

/**
 * @brief   Split a line into tokens and run the matching command.
 * @details The line is modified in place (separators become '\0').
 *
 * @param[in,out] line  NUL-terminated command line.
 */
void Cmd_Execute(char *line);

/**
 * @brief   Discard the line currently being assembled.
 * @details Call when input was lost (UART overflow) or timed out.
 */
void Cmd_Reset(void);

/**
 * @brief   Read the UART, feed the line assembler and handle errors.
 * @details Drains at most CMD_MAX_BYTES_PER_POLL bytes per call. Discards the
 *          partial line when the UART reports lost data, or when no byte was
 *          received for CMD_TIMEOUT_MS. Call once per main-loop iteration.
 *
 * @param[in] now_ms  Current time in milliseconds (any wrapping 32-bit tick).
 */
void Cmd_Poll(uint32_t now_ms);

/**
 * @brief   Parse an unsigned integer given in decimal or as "0x" hexadecimal.
 * @details Rejects empty strings, a leading '-', and trailing garbage.
 *
 * @param[in]  s      NUL-terminated text.
 * @param[out] value  Receives the number. Unchanged on failure.
 *
 * @retval 1  Parsed successfully.
 * @retval 0  Invalid text.
 */
uint8_t Cmd_ParseU32(const char *s, uint32_t *value);

/** @} */

#endif /* CMD_H */
