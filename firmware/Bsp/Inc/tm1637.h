/*
 * TM1637.h
 *
 *  Created on: Jul 27, 2019
 *      Author: XuanHung
 */

#ifndef TM1637_H_
#define TM1637_H_

#include "main.h"

#define TM1637_LED_SEGMENT              1   /**< Number of single-LED GRIDs            */
#define TM1637_LED7_SEGMENT             4   /**< Number of 7-segment GRIDs             */
#define TM1637_KEY_SEGMENT              1   /**< Number of key-scan GRIDs              */

#define TM1637_LED_SEGMENT_POSITION     0   /**< First LED GRID (GRID1)                */
#define TM1637_LED7_SEGMENT_POSITION    1   /**< First 7-segment GRID (GRID2..GRID5)   */
#define TM1637_KEY_SEGMENT_POSITION     5   /**< Key GRID (GRID6)                      */

/* ------------------------------------------------------------------------- */
/* Module configuration                                                      */
/* ------------------------------------------------------------------------- */
/** Number of GRIDs in use (max 6). Each GRID is one display byte. */
#define TM1637_GRID_NUM         6

/** Number of 7-segment digits on the module (4 or 6). */
#define TM1637_NUM_DIGITS       4

#if (TM1637_GRID_NUM < 1) || (TM1637_GRID_NUM > 6)
#error "TM1637_GRID_NUM must be in the range 1..6"
#endif


/**
 * @brief Delay source selection.
 *  - 1: use TIM17 free-running at 1 MHz (ARR must be 0xFFFF).
 *  - 0: use a NOP busy loop (delay = TM1637_DELAY_LOOPS iterations).
 */
#define TM1637_USE_TIM17        1

/** Half clock period in microseconds (3..5 us). Used when TM1637_USE_TIM17 = 1. */
#define TM1637_DELAY_US         4

/** NOP loop iterations per half clock period. Used only when TM1637_USE_TIM17 = 0. */
#define TM1637_DELAY_LOOPS      40

/** Number of consecutive identical scans required to accept a new key state. */
#define TM1637_KEY_DEBOUNCE     2

/** Re-send the whole buffer every N calls of TM1637_Proc() (noise protection), 0 = disabled. */
#define TM1637_FORCE_REFRESH_CALLS  50

/* So lan quet lien tiep phai giong nhau de chap nhan trang thai phim moi */
#define TM1637_KEY_DEBOUNCE       2

/* Gui lai toan bo bo dem sau moi N lan goi Proc (chong nhieu), 0 = tat */
#define TM1637_FORCE_REFRESH_CALLS 50

/* ------------------------------------------------------------------------- */
/* Constants                                                                 */
/* ------------------------------------------------------------------------- */
#define TM1637_SEG_DP           0x80    /**< DP / colon / single LED, bit 7 of a GRID   */
#define TM1637_SEG_BLANK        0x00    /**< All segments off                           */
#define TM1637_SEG_MINUS        0x40    /**< Minus sign (segment g, SEG1..SEG7 = a..g)  */
#define TM1637_BRIGHTNESS_MAX   7       /**< Maximum brightness level                   */
#define TM1637_KEY_NONE         0xFF    /**< No key pressed                             */

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
/* ------------------------------------------------------------------------- */

/** @brief Status code returned by the display setter functions. */
typedef enum {
    TM1637_OK        = 0,   /**< Operation succeeded                    */
    TM1637_ERR_PARAM = 1    /**< Invalid parameter, nothing was changed */
} TM1637_Status_t;

/* ------------------------------------------------------------------------- */
/* General API                                                               */
/* ------------------------------------------------------------------------- */

/**
 * @brief  Initialize the driver (GPIO, buffers, key state).
 * @note   TIM17 must be running at 1 MHz before the first transfer when
 *         TM1637_USE_TIM17 = 1.
 */
void TM1637_Init(void);

/**
 * @brief  Periodic task: refresh the display and scan the keys.
 *
 * Sends the display buffer to the chip when it changed (or every
 * TM1637_FORCE_REFRESH_CALLS calls) and updates the debounced key state.
 *
 * @note   Blocking (bit-banged transfer). Call it from the main loop.
 */
void TM1637_Proc(void);

/**
 * @brief  Set display brightness and ON/OFF state.
 * @param[in] brightness  Level 0..TM1637_BRIGHTNESS_MAX (larger values are clamped).
 * @param[in] on          Non-zero = display ON, 0 = display OFF.
 */
void TM1637_SetBrightness(uint8_t brightness, uint8_t on);

/** @brief Clear the whole display buffer (all segments off). */
void TM1637_Clear(void);

/* ------------------------------------------------------------------------- */
/* Display buffer API                                                        */
/* ------------------------------------------------------------------------- */

/**
 * @brief  Write a raw segment value to one GRID.
 * @param[in] grid   GRID index, 0..(TM1637_GRID_NUM - 1).
 * @param[in] value  Segment bits (bit 0 = SEG1 ... bit 7 = SEG8).
 * @retval TM1637_OK         Value stored.
 * @retval TM1637_ERR_PARAM  @p grid out of range.
 */
TM1637_Status_t TM1637_SetRaw(uint8_t grid, uint8_t value);

/**
 * @brief  Turn a single segment/LED of a GRID on or off.
 * @param[in] grid  GRID index, 0..(TM1637_GRID_NUM - 1).
 * @param[in] bit   Segment bit, 0..7.
 * @param[in] on    Non-zero = on, 0 = off.
 * @retval TM1637_OK         Segment updated.
 * @retval TM1637_ERR_PARAM  @p grid or @p bit out of range.
 */
TM1637_Status_t TM1637_SetLed(uint8_t grid, uint8_t bit, uint8_t on);

/**
 * @brief  Show a single hexadecimal digit on a GRID.
 * @param[in] grid   GRID index, 0..(TM1637_GRID_NUM - 1).
 * @param[in] digit  Digit to show (low 4 bits used, 0..F).
 * @param[in] dp     Non-zero = also turn on the decimal point / colon.
 * @retval TM1637_OK         Digit stored.
 * @retval TM1637_ERR_PARAM  @p grid out of range.
 */
TM1637_Status_t TM1637_SetDigit(uint8_t grid, uint8_t digit, uint8_t dp);

/**
 * @brief  Show a signed decimal number, right-aligned, on consecutive GRIDs.
 *
 * Values that do not fit are clamped. The DP/colon bit of each GRID is preserved.
 *
 * @param[in] startGrid    First GRID used (0-based).
 * @param[in] numDigits    Number of digit positions, 1..5.
 * @param[in] value        Value to display.
 * @param[in] leadingZero  Non-zero = pad with zeros, 0 = pad with blanks.
 * @retval TM1637_OK         Number stored.
 * @retval TM1637_ERR_PARAM  @p numDigits is 0 or greater than 5, or the range
 *                           exceeds TM1637_GRID_NUM.
 */
TM1637_Status_t TM1637_SetNumber(uint8_t startGrid, uint8_t numDigits,
                                 int16_t value, uint8_t leadingZero);

/**
 * @brief  Show a time in HH:MM format on GRID1..GRID4.
 * @param[in] hour    Hours (taken modulo 100).
 * @param[in] minute  Minutes (taken modulo 100).
 * @param[in] colon   Non-zero = colon on, 0 = colon off.
 * @retval TM1637_OK         Time stored.
 * @retval TM1637_ERR_PARAM  TM1637_GRID_NUM is less than 4.
 */
TM1637_Status_t TM1637_SetTime(uint8_t hour, uint8_t minute, uint8_t colon);

/* ------------------------------------------------------------------------- */
/* Key API                                                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief  Get the debounced key code.
 * @return Key code, or TM1637_KEY_NONE if no key is pressed.
 */
uint8_t TM1637_GetKey(void);

/**
 * @brief  Get the key code of a new key press once, then clear it (event).
 * @return Key code, or TM1637_KEY_NONE if there is no new press.
 */
uint8_t TM1637_GetKeyPressed(void);

/**
 * @brief  Get the number of failed transfers (missing ACK) since initialization.
 * @return Error counter value.
 */
uint32_t TM1637_GetErrorCount(void);

#endif /* TM1637_H_ */
