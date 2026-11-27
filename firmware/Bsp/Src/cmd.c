/**
 * @file    cmd.c
 * @brief   Text command line for debugging over UART.
 * @details Line assembly keeps its state in static variables, so a command
 *          that arrives in several pieces is handled correctly. Command
 *          replies are written with Log_Printf() (no level filtering), so
 *          they are visible even when the log level is NONE.
 *
 *          The whole file is compiled only when CMD_ENABLE is 1.
 * @author  <author name>
 * @date    <yyyy-mm-dd>
 */

#include "cmd.h"

#if CMD_ENABLE

#include "uart_comm.h"
#include "bsp_log.h"
#include <string.h>
#if CMD_USE_LIBC_STRTOUL
#include <stdlib.h>
#endif

/** One entry of a command table. */
typedef struct {
    const char    *name;     /**< Command name.            */
    const char    *help;     /**< One-line help text.      */
    cmd_handler_t  handler;  /**< Function to call.        */
} cmd_entry_t;

static void cmd_help(uint8_t argc, char *argv[]);
static void cmd_log(uint8_t argc, char *argv[]);
static void cmd_status(uint8_t argc, char *argv[]);

/** Built-in commands. */
static const cmd_entry_t s_builtin[] = {
    { "help",   "help: list commands",              cmd_help   },
    { "log",    "log [0-4]: show or set log level", cmd_log    },
    { "status", "status: show UART/log counters",   cmd_status }
};
#define BUILTIN_COUNT   ((uint8_t)(sizeof(s_builtin) / sizeof(s_builtin[0])))

static cmd_entry_t s_custom[CMD_MAX_CUSTOM];  /**< Commands added by Cmd_Register(). */
static uint8_t     s_custom_count;            /**< Number of custom commands.         */

static char     s_line[CMD_LINE_MAX];         /**< Line being assembled.              */
static uint8_t  s_len;                        /**< Characters in s_line.              */
static uint8_t  s_overlong;                   /**< Current line is too long.          */
static uint32_t s_last_byte_ms;               /**< Time of the last received byte.    */

/* ------------------------------------------------------------------------- */
/* Built-in commands                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Print the list of commands.
 *
 * @param[in] argc  Unused.
 * @param[in] argv  Unused.
 */
static void cmd_help(uint8_t argc, char *argv[])
{
    uint8_t i;

    (void)argc;
    (void)argv;

    Log_Printf("commands:\r\n");
    for (i = 0U; i < BUILTIN_COUNT; i++) {
        Log_Printf("  %s\r\n", s_builtin[i].help);
    }
    for (i = 0U; i < s_custom_count; i++) {
        Log_Printf("  %s\r\n", s_custom[i].help);
    }
}

/**
 * @brief Show or set the run-time log level.
 *
 * @param[in] argc  1 to show, 2 to set.
 * @param[in] argv  argv[1] is the new level 0..4.
 */
static void cmd_log(uint8_t argc, char *argv[])
{
    uint32_t level;

    if (argc < 2U) {
        Log_Printf("log level = %u (compiled max = %u)\r\n",
                   (unsigned)Log_GetLevel(), (unsigned)BSP_LOG_LEVEL);
        return;
    }
    if ((Cmd_ParseU32(argv[1], &level) == 0U) || (level > BSP_LOG_LEVEL_DEBUG)) {
        Log_Printf("ERR: usage: log [0-4]\r\n");
        return;
    }
    Log_SetLevel((uint8_t)level);
    Log_Printf("log level set to %u\r\n", (unsigned)level);
}

/**
 * @brief Print UART and log counters.
 *
 * @param[in] argc  Unused.
 * @param[in] argv  Unused.
 */
static void cmd_status(uint8_t argc, char *argv[])
{
    (void)argc;
    (void)argv;

    Log_Printf("log level  : %u\r\n", (unsigned)Log_GetLevel());
    Log_Printf("log dropped: %lu\r\n", (unsigned long)Log_GetDroppedCount());
    Log_Printf("rx errors  : %lu\r\n", (unsigned long)Uart_GetErrorCount());
    Log_Printf("rx overrun : %lu\r\n", (unsigned long)Uart_GetOverflowCount());
}

/* ------------------------------------------------------------------------- */
/* Public functions                                                          */
/* ------------------------------------------------------------------------- */

uint8_t Cmd_Register(const char *name, const char *help, cmd_handler_t handler)
{
    if ((name == NULL) || (handler == NULL) || (s_custom_count >= CMD_MAX_CUSTOM)) {
        return 0U;
    }
    s_custom[s_custom_count].name    = name;
    s_custom[s_custom_count].help    = (help != NULL) ? help : name;
    s_custom[s_custom_count].handler = handler;
    s_custom_count++;
    return 1U;
}

void Cmd_Feed(uint8_t c)
{
    if (c == (uint8_t)'\r') {
        return;
    }

    if (c == (uint8_t)'\n') {
        if (s_overlong != 0U) {
            Log_Printf("ERR: line too long\r\n");
        } else if (s_len > 0U) {
            s_line[s_len] = '\0';
            Cmd_Execute(s_line);
        }
        s_len = 0U;
        s_overlong = 0U;
        return;
    }

    if ((c == 0x08U) || (c == 0x7FU)) {         /* backspace / delete */
        if (s_len > 0U) {
            s_len--;
        }
        return;
    }

    if (c < 0x20U) {                            /* other control characters */
        return;
    }

    if (s_len < (CMD_LINE_MAX - 1U)) {
        s_line[s_len++] = (char)c;
    } else {
        s_overlong = 1U;
    }
}

void Cmd_Execute(char *line)
{
    char *argv[CMD_MAX_ARGS];
    uint8_t argc = 0U;
    char *p = line;
    uint8_t i;

    if (line == NULL) {
        return;
    }

    /* Tokenize in place on spaces and tabs. */
    while (argc < CMD_MAX_ARGS) {
        while ((*p == ' ') || (*p == '\t')) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while ((*p != '\0') && (*p != ' ') && (*p != '\t')) {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    if (argc == 0U) {
        return;
    }

    for (i = 0U; i < BUILTIN_COUNT; i++) {
        if (strcmp(argv[0], s_builtin[i].name) == 0) {
            s_builtin[i].handler(argc, argv);
            return;
        }
    }
    for (i = 0U; i < s_custom_count; i++) {
        if (strcmp(argv[0], s_custom[i].name) == 0) {
            s_custom[i].handler(argc, argv);
            return;
        }
    }
    Log_Printf("ERR: unknown command '%s' (try 'help')\r\n", argv[0]);
}

void Cmd_Reset(void)
{
    s_len = 0U;
    s_overlong = 0U;
}

void Cmd_Poll(uint32_t now_ms)
{
    uint8_t buf[CMD_MAX_BYTES_PER_POLL];
    uint16_t n;
    uint16_t i;

    n = Uart_RxReadBuf(buf, (uint16_t)sizeof(buf));
    for (i = 0U; i < n; i++) {
        Cmd_Feed(buf[i]);
    }

    if (Uart_RxOverflowTaken() != 0U) {
        Cmd_Reset();
        LOG_E("UART RX overflow, partial line dropped");
    }

    if (n > 0U) {
        s_last_byte_ms = now_ms;
    } else if ((s_len > 0U) && ((uint32_t)(now_ms - s_last_byte_ms) > CMD_TIMEOUT_MS)) {
        Cmd_Reset();
        LOG_W("command timeout, partial line dropped");
    }
}

uint8_t Cmd_ParseU32(const char *s, uint32_t *value)
{
    if ((s == NULL) || (value == NULL) || (*s == '\0') || (*s == '-')) {
        return 0U;
    }

#if CMD_USE_LIBC_STRTOUL
    {
        char *end;
        unsigned long v;
        int base = 10;

        if ((s[0] == '0') && ((s[1] == 'x') || (s[1] == 'X'))) {
            base = 16;
        }
        v = strtoul(s, &end, base);
        if ((*end != '\0') || (end == s)) {
            return 0U;
        }
        *value = (uint32_t)v;
        return 1U;
    }
#else
    {
        uint32_t acc = 0U;
        uint32_t base = 10U;
        uint8_t digits = 0U;
        const char *p = s;

        if ((s[0] == '0') && ((s[1] == 'x') || (s[1] == 'X'))) {
            base = 16U;
            p = s + 2;
        }
        for (; *p != '\0'; p++) {
            uint32_t d;
            char c = *p;

            if ((c >= '0') && (c <= '9')) {
                d = (uint32_t)(c - '0');
            } else if ((base == 16U) && (c >= 'a') && (c <= 'f')) {
                d = (uint32_t)(c - 'a') + 10U;
            } else if ((base == 16U) && (c >= 'A') && (c <= 'F')) {
                d = (uint32_t)(c - 'A') + 10U;
            } else {
                return 0U;
            }
            if (acc > ((0xFFFFFFFFU - d) / base)) {
                return 0U;                      /* would overflow 32 bits */
            }
            acc = (acc * base) + d;
            digits++;
        }
        if (digits == 0U) {
            return 0U;
        }
        *value = acc;
        return 1U;
    }
#endif
}

#endif /* CMD_ENABLE */
