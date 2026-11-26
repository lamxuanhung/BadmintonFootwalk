/*
 * TM1637.c
 *
 *  Created on: Jul 27, 2019
 *      Author: XuanHung
 */

/* Display RAM layout (each GRID holds one byte, SEG1..SEG8 map to B0..B7):
 *
 * @verbatim
 *   Address   Low nibble   High nibble   Grid
 *   C0H       xxHL         xxHU          GRID1
 *   C1H       xxHL         xxHU          GRID2
 *   C2H       xxHL         xxHU          GRID3
 *   C3H       xxHL         xxHU          GRID4
 *   C4H       xxHL         xxHU          GRID5
 *   C5H       xxHL         xxHU          GRID6
 *             B0..B3       B4..B7
 *             SEG1..SEG4   SEG5..SEG8
 * @endverbatim
 *
 * @note TM1637_Proc() (the function that calls tm1637_send_display() when
 *       @c s_dirty is set) and the key-scan code are not part of this file.
 */

#include "../Inc/tm1637.h"

/* ------------------------------------------------------------------------- */
/* Pin assignment                                                            */
/* ------------------------------------------------------------------------- */
#define TM1637_CLK_PORT         GPIOA           /**< CLK port */
#define TM1637_CLK_PIN          GPIO_PIN_1      /**< CLK pin  */
#define TM1637_DIO_PORT         GPIOA           /**< DIO port */
#define TM1637_DIO_PIN          GPIO_PIN_2      /**< DIO pin  */

/* ------------------------------------------------------------------------- */
/* TM1637 commands                                                           */
/* ------------------------------------------------------------------------- */
#define TM1637_CMD_DATA_AUTO    0x40    /**< Write data, address auto-increment */
#define TM1637_CMD_READKEY      0x42    /**< Read key scan data                 */
#define TM1637_CMD_DATA_FIXED   0x44    /**< Write data, fixed address          */
#define TM1637_CMD_DISPLAY      0x80    /**< Display control                    */
#define TM1637_DISPLAY_ON_BIT   0x08    /**< Display ON bit in display control  */
#define TM1637_CMD_ADDRESS      0xC0    /**< Display address command (C0H)      */

#define TM1637_SEG_MINUS   0x40    /**< Minus sign: only segment g on (SEG7)                  */
#define TM1637_SEG_BLANK   0x00    /**< Blank digit: all segments off                         */
#define TM1637_SEG_MASK    0x7F    /**< Mask of segments a..g (SEG1..SEG7), excludes DP bit 7 */

/** Half clock period in microseconds (3~5 us is fine). */
#define TM1637_DELAY_US         4

/* ------------------------------------------------------------------------- */
/* Low-level pin access                                                      */
/* ------------------------------------------------------------------------- */

/** @brief Drive CLK high (atomic, via BSRR). */
static inline void clk_high(void) { TM1637_CLK_PORT->BSRR = TM1637_CLK_PIN; }

/** @brief Drive CLK low (atomic, via BRR). */
static inline void clk_low(void)  { TM1637_CLK_PORT->BRR  = TM1637_CLK_PIN; }

/** @brief Release DIO (high with open-drain output + pull-up). */
static inline void dio_high(void) { TM1637_DIO_PORT->BSRR = TM1637_DIO_PIN; }

/** @brief Drive DIO low (atomic, via BRR). */
static inline void dio_low(void)  { TM1637_DIO_PORT->BRR  = TM1637_DIO_PIN; }

/**
 * @brief  Read the current level of the DIO pin.
 * @retval 1  DIO is high.
 * @retval 0  DIO is low.
 */
static inline uint8_t dio_read(void)
{
    return (TM1637_DIO_PORT->IDR & TM1637_DIO_PIN) ? 1U : 0U;
}

static uint8_t  s_buf[TM1637_GRID_NUM];     /**< Display buffer (GRID1..)            */
static uint8_t  s_ctrl;                     /**< Display control command             */
static uint8_t  s_dirty;                    /**< 1 = buffer must be re-sent to chip  */
static uint16_t s_refreshCnt;               /**< Refresh counter                     */
static uint32_t s_errCnt;                   /**< Number of missing ACKs              */

/** Candidate key: the raw key code from the most recent scan (TM1637_KEY_NONE = no key). */
static uint8_t  s_keyCand   = TM1637_KEY_NONE;

/** Number of consecutive scans in which s_keyCand has been read unchanged. */
static uint8_t  s_keyCnt;

/** Debounced (stable) key code, returned by TM1637_GetKey(). TM1637_KEY_NONE = no key. */
static uint8_t  s_keyStable = TM1637_KEY_NONE;

/** Pending key-press event, set once per new press and cleared by TM1637_GetKeyPressed(). */
static uint8_t  s_keyEvent  = TM1637_KEY_NONE;

/**
 * @brief  7-segment lookup table for hexadecimal digits 0..F (common cathode).
 *
 * Bit 0..6 = segments a..g (SEG1..SEG7), bit 7 = DP (see TM1637_SEG_DP).
 * Letters b and d are shown in lower case to be distinguishable from 8 and 0.
 *
 * Index: 0x0..0x9 = '0'..'9', 0xA..0xF = 'A','b','C','d','E','F'.
 */
static const uint8_t s_seg[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,   /* 0 1 2 3 4 5 6 7 */
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71    /* 8 9 A b C d E F */
};

/**
 * @brief  Busy-wait for TM1637_DELAY_US microseconds.
 *
 * @note   Uses TIM17 exclusively for generating the TM1637 timing. TIM17 must
 *         be running with a 1 MHz counter clock (1 tick = 1 us) before any
 *         TM1637 function is called.
 * @note   Because the start value is taken at an arbitrary point inside a tick,
 *         the real delay is between (TM1637_DELAY_US - 1) and TM1637_DELAY_US us.
 */
static inline void tm1637_delay(void)
{
#if TM1637_USE_TIM17
	/* Request TIM17 run 1MHz, ARR = 0xFFFF */
    uint16_t start = (uint16_t)TIM17->CNT;
    while ((uint16_t)(TIM17->CNT - start) < TM1637_DELAY_US) { }
#else
    for (volatile uint32_t i = 0; i < TM1637_DELAY_LOOPS; i++) {
        __NOP();
    }
#endif
}

/**
 * @brief  Generate a START condition (DIO falls while CLK is high).
 * @note   On exit CLK is low and DIO is low.
 */
void tm1637_start(void)
{
    clk_high();
    dio_high();
    tm1637_delay();
    dio_low();          /* DIO falling while CLK high = START */
    tm1637_delay();
    clk_low();
    tm1637_delay();
}

/**
 * @brief  Generate a STOP condition (DIO rises while CLK is high).
 * @note   On exit both CLK and DIO are high (bus idle).
 */
void tm1637_stop(void)
{
    clk_low();
    dio_low();
    tm1637_delay();
    clk_high();
    tm1637_delay();
    dio_high();         /* DIO rising while CLK high = STOP */
    tm1637_delay();
}

/**
 * @brief  Send one byte to the TM1637 (LSB first) and read the ACK bit.
 *
 * Each data bit is placed on DIO while CLK is low, then CLK is raised so the
 * TM1637 can sample it. After 8 bits, a 9th clock pulse is generated and DIO
 * is read to check the ACK from the TM1637 (the chip pulls DIO low if the byte
 * was received successfully).
 *
 * @note   DIO and CLK must be configured as open-drain with a pull-up, so that
 *         DIO can be read back even while it is configured as an output.
 * @note   Must be called after tm1637_start(). CLK is low on entry. On exit
 *         CLK is low and DIO has been released (high).
 * @note   Blocking: one byte takes 19 calls of tm1637_delay() (~76 us with
 *         TM1637_DELAY_US = 4).
 *
 * @param[in] b  Byte to send (bit 0 is sent first).
 *
 * @retval 1  ACK received (DIO low during the 9th CLK pulse).
 * @retval 0  No ACK (DIO high): chip not responding, wiring fault or missing
 *            pull-up. The error counter is incremented.
 */
static uint8_t tm1637_write_byte(uint8_t b)
{
    for (uint8_t i = 0; i < 8; i++) {
        clk_low();
        if (b & 0x01U) dio_high(); else dio_low();
        tm1637_delay();
        clk_high();
        tm1637_delay();
        b >>= 1;
    }

    /* 9th CLK pulse: read ACK */
    clk_low();
    dio_high();         /* release DIO so the TM1637 can pull it low */
    tm1637_delay();
    clk_high();
    tm1637_delay();
    uint8_t ack = (dio_read() == 0U) ? 1U : 0U;
    clk_low();
    tm1637_delay();

    if (!ack) s_errCnt++;
    return ack;
}

/**
 * @brief  Send a single-byte command (START, command, STOP).
 * @param[in] cmd  Command byte.
 */
static void tm1637_send_command(uint8_t cmd)
{
    tm1637_start();
    tm1637_write_byte(cmd);
    tm1637_stop();
}

/**
 * @brief  Send the whole display buffer and the display control command.
 *
 * Writes the display RAM using the address auto-increment mode:
 * @verbatim
 *   Start Command1 Stop
 *   Start Command2 Data1 Data2 ... DataN Stop
 *   Start Command3 Stop
 * @endverbatim
 *
 * - Command1: data command (write display RAM, auto-increment address)
 * - Command2: start address C0H followed by the data of every GRID
 * - Command3: display control (ON/OFF + brightness)
 *
 * @note   Blocking. Normally called from TM1637_Proc() when @c s_dirty is set.
 */
static void tm1637_send_display(void)
{
    /* COMM1: write data, address auto-increment */
    tm1637_send_command(TM1637_CMD_DATA_AUTO);

    /* COMM2: address C0H + data of every GRID */
    tm1637_start();
    tm1637_write_byte(TM1637_CMD_ADDRESS);
    for (uint8_t i = 0; i < TM1637_GRID_NUM; i++) {
        tm1637_write_byte(s_buf[i]);
    }
    tm1637_stop();

    /* COMM3: display ON/OFF + brightness */
    tm1637_send_command(s_ctrl);
}

/**
 * @brief  Read the raw key-scan byte from the TM1637.
 *
 * Sequence: START, read-key command (0x42), 8 data bits (LSB first), ACK pulse
 * driven by the MCU, STOP.
 *
 * @note   Requires DIO to be open-drain with a pull-up (see TM1637_Init()):
 *         DIO is read back while it is configured as an output, and the chip
 *         drives it low.
 * @note   Blocking: about 46 calls of tm1637_delay() (~0.2 ms with
 *         TM1637_DELAY_US = 4).
 *
 * @return Raw key code as defined in the TM1637 datasheet (K1/K2 and KS1..KS8
 *         encoded in one byte), or 0xFF (equal to TM1637_KEY_NONE) when no key
 *         is pressed.
 */
static uint8_t tm1637_read_key(void)
{
    uint8_t v = 0;

    tm1637_start();
    tm1637_write_byte(TM1637_CMD_READKEY);  /* returns with CLK low and DIO released */

    dio_high();                             /* make sure DIO is released */
    tm1637_delay();                         /* tWAIT (see datasheet) before reading the first bit */
    tm1637_delay();

    for (uint8_t i = 0; i < 8; i++) {
        clk_high();
        tm1637_delay();
        if (dio_read()) v |= (uint8_t)(1U << i);    /* LSB first */
        clk_low();                                  /* the chip drives the next bit on the falling edge */
        tm1637_delay();
    }

    /* MCU ACK: hold DIO low for one CLK pulse */
    dio_low();
    tm1637_delay();
    clk_high();
    tm1637_delay();
    clk_low();
    tm1637_delay();
    dio_high();

    tm1637_stop();
    return v;
}

/**
 * @brief  Debounce filter for the raw key code.
 *
 * A new raw value becomes the stable key state only after it has been read in
 * TM1637_KEY_DEBOUNCE consecutive scans. When the stable state changes to an
 * actual key (not TM1637_KEY_NONE), a press event is stored in s_keyEvent.
 *
 * @param[in] raw  Raw key code from the latest scan (TM1637_KEY_NONE = no key).
 *
 * @note   Called once per scan from TM1637_Proc().
 */
static void tm1637_update_key(uint8_t raw)
{
    if (raw == s_keyCand) {
        if (s_keyCnt < 255U) s_keyCnt++;    /* saturate to avoid wrap-around */
    } else {
        s_keyCand = raw;                    /* value changed: start a new candidate */
        s_keyCnt  = 1;
    }

    if (s_keyCnt >= TM1637_KEY_DEBOUNCE && s_keyStable != s_keyCand) {
        s_keyStable = s_keyCand;
        if (s_keyStable != TM1637_KEY_NONE) {
            s_keyEvent = s_keyStable;       /* new key-press event */
        }
    }
}


/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * @brief  Set display brightness and ON/OFF state.
 *
 * The new setting is only stored; it is sent to the chip on the next refresh
 * (the dirty flag is set only if the setting actually changed).
 *
 * @param[in] brightness  Brightness level, 0..TM1637_BRIGHTNESS_MAX. Larger
 *                        values are clamped to TM1637_BRIGHTNESS_MAX.
 * @param[in] on          Non-zero = display ON, 0 = display OFF.
 */
void TM1637_SetBrightness(uint8_t brightness, uint8_t on)
{
    if (brightness > TM1637_BRIGHTNESS_MAX) brightness = TM1637_BRIGHTNESS_MAX;
    uint8_t ctrl = (uint8_t)(TM1637_CMD_DISPLAY | (on ? TM1637_DISPLAY_ON_BIT : 0U) | brightness);
    if (ctrl != s_ctrl) {
        s_ctrl  = ctrl;
        s_dirty = 1;
    }
}

/**
 * @brief  Clear the whole display buffer (all segments off).
 * @note   The dirty flag is always set.
 */
void TM1637_Clear(void)
{
    for (uint8_t i = 0; i < TM1637_GRID_NUM; i++) s_buf[i] = 0;
    s_dirty = 1;
}

/**
 * @brief  Turn a single segment/LED of a GRID on or off.
 *
 * @param[in] grid  GRID index, 0..(TM1637_GRID_NUM - 1).
 * @param[in] bit   Segment bit, 0..7 (SEG1..SEG8).
 * @param[in] on    Non-zero = on, 0 = off.
 *
 * @retval TM1637_OK         Segment updated.
 * @retval TM1637_ERR_PARAM  @p grid or @p bit out of range; nothing changed.
 */
TM1637_Status_t TM1637_SetLed(uint8_t grid, uint8_t bit, uint8_t on)
{
    if (grid >= TM1637_GRID_NUM || bit > 7U) return TM1637_ERR_PARAM;
    uint8_t v = s_buf[grid];
    if (on) v |= (uint8_t)(1U << bit);
    else    v &= (uint8_t)~(1U << bit);
    return TM1637_SetRaw(grid, v);
}

/**
 * @brief  Show a single hexadecimal digit on a GRID.
 *
 * @param[in] grid   GRID index, 0..(TM1637_GRID_NUM - 1).
 * @param[in] digit  Digit to show; only the low 4 bits are used (0..F).
 * @param[in] dp     Non-zero = also turn on the decimal point / colon.
 *
 * @retval TM1637_OK         Digit stored in the display buffer.
 * @retval TM1637_ERR_PARAM  @p grid out of range; nothing changed.
 */
TM1637_Status_t TM1637_SetDigit(uint8_t grid, uint8_t digit, uint8_t dp)
{
    uint8_t v = s_seg[digit & 0x0F];
    if (dp) v |= TM1637_SEG_DP;
    return TM1637_SetRaw(grid, v);
}

/**
 * @brief  Show a signed decimal number on a group of consecutive GRIDs.
 *
 * The number is right-aligned. A negative number uses one digit position for
 * the minus sign. Values that do not fit are clamped to the largest value that
 * can be displayed. The DP/colon bit (bit 7) of each GRID is preserved.
 *
 * @param[in] startGrid    First GRID used (0-based).
 * @param[in] numDigits    Number of digit positions, 1..5.
 * @param[in] value        Value to display.
 * @param[in] leadingZero  Non-zero = pad with zeros, 0 = pad with blanks.
 *
 * @retval TM1637_OK         Number stored in the display buffer (possibly
 *                           clamped, see above).
 * @retval TM1637_ERR_PARAM  @p numDigits is 0 or greater than 5, or the range
 *                           [startGrid, startGrid + numDigits) exceeds
 *                           TM1637_GRID_NUM; nothing changed.
 */
TM1637_Status_t TM1637_SetNumber(uint8_t startGrid, uint8_t numDigits,
                                 int16_t value, uint8_t leadingZero)
{
    if (numDigits == 0 || numDigits > 5 ||
        (uint16_t)startGrid + numDigits > TM1637_GRID_NUM) return TM1637_ERR_PARAM;

    uint8_t  negative = (value < 0) ? 1U : 0U;
    uint16_t v = negative ? (uint16_t)(-(int32_t)value) : (uint16_t)value;

    /* Limit: a negative number needs one position for the minus sign */
    uint8_t  nd = negative ? (uint8_t)(numDigits - 1U) : numDigits;
    uint32_t maxVal = 1U;                       /* 32-bit: 10^5 overflows uint16_t */
    for (uint8_t i = 0; i < nd; i++) maxVal *= 10U;
    if (nd == 0U) v = 0U;
    else if (v >= maxVal) v = (uint16_t)(maxVal - 1U);

    uint8_t seg[5];
    int8_t  i = (int8_t)(numDigits - 1U);

    /* Split digits from right to left (at least one digit) */
    do {
        seg[i--] = s_seg[v % 10U];
        v /= 10U;
    } while (v > 0U && i >= 0);

    uint8_t signIdx = (i >= 0) ? (uint8_t)i : 0U;   /* position right before the first digit */

    /* Fill the left part */
    while (i >= 0) {
        seg[i--] = leadingZero ? s_seg[0] : TM1637_SEG_BLANK;
    }

    if (negative) {
        seg[leadingZero ? 0U : signIdx] = TM1637_SEG_MINUS;
    }

    /* Write to the buffer, keeping bit 7 (DP/colon) of each GRID */
    for (uint8_t k = 0; k < numDigits; k++) {
        uint8_t g = (uint8_t)(startGrid + k);
        TM1637_SetRaw(g, (uint8_t)(seg[k] | (s_buf[g] & TM1637_SEG_DP)));   /* g is always valid here */
    }
    return TM1637_OK;
}

/**
 * @brief  Show a time in HH:MM format on GRID1..GRID4.
 *
 * The colon is the DP segment of GRID2 (typical 4-digit clock module).
 *
 * @param[in] hour    Hours (taken modulo 100).
 * @param[in] minute  Minutes (taken modulo 100).
 * @param[in] colon   Non-zero = colon on, 0 = colon off.
 *
 * @retval TM1637_OK         Time stored in the display buffer.
 * @retval TM1637_ERR_PARAM  TM1637_GRID_NUM is less than 4; nothing changed.
 */
TM1637_Status_t TM1637_SetTime(uint8_t hour, uint8_t minute, uint8_t colon)
{
    if (TM1637_GRID_NUM < 4) return TM1637_ERR_PARAM;
    hour   %= 100U;
    minute %= 100U;

    TM1637_SetRaw(0, s_seg[hour / 10U]);
    TM1637_SetRaw(1, (uint8_t)(s_seg[hour % 10U] | (colon ? TM1637_SEG_DP : 0U)));
    TM1637_SetRaw(2, s_seg[minute / 10U]);
    TM1637_SetRaw(3, s_seg[minute % 10U]);
    return TM1637_OK;
}

/**
 * @brief  Get the current debounced (stable) key code.
 * @return Key code, or TM1637_KEY_NONE if no key is pressed.
 */
uint8_t TM1637_GetKey(void)
{
    return s_keyStable;
}

/**
 * @brief  Get the last key-press event and clear it (read-and-clear).
 * @return Key code of the pending press event, or TM1637_KEY_NONE if none.
 * @note   Not atomic: do not call from both main loop and an ISR.
 */
uint8_t TM1637_GetKeyPressed(void)
{
    uint8_t k = s_keyEvent;
    s_keyEvent = TM1637_KEY_NONE;
    return k;
}

/**
 * @brief  Get the number of missing ACKs since TM1637_Init().
 * @return Error counter value.
 */
uint32_t TM1637_GetErrorCount(void)
{
    return s_errCnt;
}
/*
    SEG1 SEG2 SEG3 SEG4	SEG5 SEG6 SEG7 SEG8
	xxHL(low four bits) 	xxHU(high four bits)
	B0 B1 B2 B3 		B4 B5 B6 B7
	C0HL 				C0HU 					GRID1
	C1HL 				C1HU 					GRID2
	C2HL 				C2HU 					GRID3
	C3HL 				C3HU 					GRID4
	C4HL 				C4HU 					GRID5
	C5HL 				C5HU 					GRID6
 	PosDigit : Start register, which will be write
 	NumDigit : Number of led7 digit will be dispaly
 */


/**
 * @brief  Initialize the TM1637 driver.
 *
 * Configures CLK and DIO as open-drain outputs with pull-up, releases both
 * lines, clears the display buffer and key state, and schedules the first
 * transfer (brightness maximum, display ON).
 *
 * @note   TIM17 must be configured and running at 1 MHz before the first
 *         transfer is sent.
 * @note   An external pull-up (e.g. 2.2k..10k) on CLK and DIO is recommended;
 *         the internal pull-up is weak and slows the rising edge.
 */
void TM1637_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Pins should be set to HIGH before they are configured as outputs */
    HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;    /* open-drain: DIO can be read back (ACK / keys) */
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = TM1637_CLK_PIN;
    HAL_GPIO_Init(TM1637_CLK_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TM1637_DIO_PIN;
    HAL_GPIO_Init(TM1637_DIO_PORT, &GPIO_InitStruct);

    for (uint8_t i = 0; i < TM1637_GRID_NUM; i++) s_buf[i] = 0;

    s_keyCand = s_keyStable = s_keyEvent = TM1637_KEY_NONE;
    s_keyCnt = 0;
    s_errCnt = 0;
    s_refreshCnt = 0;

    TM1637_SetBrightness(TM1637_BRIGHTNESS_MAX, 1);   /* s_dirty = 1 -> first transfer is sent by TM1637_Proc() */
}

/**
 * @brief  Periodic task: refresh the display and scan the keys.
 *
 * Call it regularly from the main loop. Each call:
 *  -# Counts calls and, every TM1637_FORCE_REFRESH_CALLS calls, forces a full
 *     re-send of the display buffer (noise protection / recovery after a chip
 *     reset). The transfer includes the display-control command, so brightness
 *     and ON/OFF state are restored as well.
 *  -# Sends the display buffer if it changed (s_dirty) or a refresh was forced.
 *  -# Reads the key scan data and feeds it to the debounce filter.
 *
 * @note   Blocking (bit-banged transfers). A full display transfer takes
 *         roughly 0.7 ms and a key read about 0.3 ms with TM1637_DELAY_US = 4.
 * @note   The call period sets both the key debounce time
 *         (TM1637_KEY_DEBOUNCE x period) and the forced refresh period
 *         (TM1637_FORCE_REFRESH_CALLS x period).
 */
void TM1637_Proc(void)
{
    /* Periodic re-send to protect against noise / chip reset */
#if TM1637_FORCE_REFRESH_CALLS > 0
    if (++s_refreshCnt >= TM1637_FORCE_REFRESH_CALLS) {
        s_refreshCnt = 0;
        s_dirty = 1;
    }
#endif

    if (s_dirty) {
        s_dirty = 0;                /* clear before sending: a change made during the transfer re-arms the flag */
        tm1637_send_display();
    }

    /* Read the raw key code and run it through the debounce filter */
    tm1637_update_key(tm1637_read_key());
}

//
//void TM1637_ReadKey()
//{
//	uint8_t data_temp = 0;
//    /* Write Read key command */
//	tm1637_start();
//	TM1637_WriteByte(TM1637_CMD_READKEY_MSK);
//	DIO_Config_Input(TM1637_DIO_PORT, TM1637_DIO_PIN);
//	for(uint8_t i = 0; i < 8; i++)
//	{
//		data_temp = data_temp << 1 ;
//		HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN,GPIO_PIN_RESET);
//		if (HAL_GPIO_ReadPin(TM1637_DIO_PORT,TM1637_DIO_PIN) == GPIO_PIN_SET)
//		{
//			data_temp |= 0x01;
//		}
//		tm1637_delay();
//		HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN,GPIO_PIN_SET);
//		tm1637_delay();
//	}
//	HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN,GPIO_PIN_RESET);
//	DIO_Config_Output(TM1637_DIO_PORT,TM1637_DIO_PIN);
//	HAL_GPIO_WritePin(TM1637_DIO_PORT,TM1637_DIO_PIN,GPIO_PIN_RESET);
//	tm1637_delay();
//	HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN,GPIO_PIN_SET);
//	tm1637_delay();
//	tm1637_stop();
//}
//
//void TM1637_Clear()
//{
//	uint8_t i;
//	for (i=0; i < (TM1637_LED_SEGMENT + TM1637_LED7_SEGMENT + TM1637_KEY_SEGMENT); i++)
//	{
//		tm1637.databuffer[i] = 0;
//	}
//}



//void TM1637_Proc()
//{
//	uint8_t i;
//	/* Write COMM1	*/
//	tm1637_start();
//	/* Write data to display regiter with auto address adding*/
//	TM1637_WriteByte(TM1637_CMD_DATA_AUTO);
//	TM1637_Ack();
//	tm1637_stop();
//
//	/* Write COMM2 + first digit address */
//	tm1637_start();
//	/* Write the data to display */
//	TM1637_WriteByte(TM1637_CMD_ADDRESS_MSK);
//	TM1637_Ack();
//	for (i=0; i < TM1637_LED_SEGMENT; i++)
//	{
//		/* Write address command, start from CH */
//		TM1637_WriteByte(tm1637.databuffer[i]);
//		TM1637_Ack();
//	}
//	tm1637_stop();
//
//	/* Write COMM3 + brightness	*/
//	tm1637_start();
//	TM1637_WriteByte((TM1637_CMD_DISPLAY_MSK | 0x08) + (tm1637.brightness & 0x07));
//	TM1637_Ack();
//	tm1637_stop();
//
//	/* Send read key command */
//	tm1637_start();
//	TM1637_WriteByte(TM1637_CMD_READKEY_MSK);
//	TM1637_Ack();
//	DIO_Config_Input(TM1637_DIO_PORT, TM1637_DIO_PIN);
//	for(i = 0; i < 8; i++)
//	{
//		tm1637.databuffer[5] = tm1637.databuffer[5] << 1 ;
//		HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN,GPIO_PIN_RESET);
//		if (HAL_GPIO_ReadPin(TM1637_DIO_PORT,TM1637_DIO_PIN) == GPIO_PIN_SET)
//		{
//			tm1637.databuffer[5] |= 0x01;
//		}
//		tm1637_delay();
//		HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN,GPIO_PIN_SET);
//		tm1637_delay();
//	}
//	TM1637_Ack();
//	HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN,GPIO_PIN_SET);
//	tm1637_stop();
//}
