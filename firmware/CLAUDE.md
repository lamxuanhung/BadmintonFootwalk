# CLAUDE.md – Hướng dẫn cho trợ lý AI (Badminton Board, STM32F030)

File này mô tả bối cảnh, quy tắc và các lưu ý kỹ thuật của dự án. Đọc kỹ trước khi viết hay sửa code. Bản tiếng Anh: `CLAUDE_EN.md` (hai file phải luôn có nội dung tương đương).

Tài liệu liên quan: `README.md` (thông số phần cứng, danh sách hàm), `plan.md` (kế hoạch từng giai đoạn), `docs/coding_style.md` (quy ước đặt tên và định dạng).

---

## 1. Tổng quan dự án

Thiết bị luyện phản xạ cầu lông: các LED vị trí sáng ngẫu nhiên, người chơi chạm/bấm đúng vị trí, hiển thị bằng LED 7 đoạn, có còi. Một master điều khiển tối đa 6 slave qua nRF24L01.

- MCU: **STM32F030** (Cortex-M0, 48 MHz). Mã chip chính xác: **chưa xác định**.
- IDE: **STM32CubeIDE** (cấu hình bằng file `.ioc`).
- Ngôn ngữ: **C**.
- Debug: USART1 115200 baud.
- Không dùng soft PWM và hiệu ứng fade LED. LED chỉ bật/tắt.

---

## 2. Thông số phần cứng chính (tóm tắt)

| Hạng mục | Giá trị |
|---|---|
| Clock | HSI qua PLL, SYSCLK/HCLK **48 MHz**, APB1 /1, clock timer 48 MHz |
| SysTick | 1 kHz |
| UART | USART1, **PA9 TX / PA10 RX**, 115200 8N1, DMA TX Normal, DMA RX **Circular** |
| SPI | SPI1, **PA5 SCK / PA6 MISO / PA7 MOSI**, Master, mode 0, 8 bit MSB, NSS software, prescaler **8** (6 Mbps) |
| TM1637 | **PA1 = CLK, PA2 = DIO**, Open Drain, Pull-up (bit-bang, không dùng I2C) |
| TIM16 | 1 kHz (PSC 47, ARR 999): nhịp scheduler |
| TIM17 | Bộ đếm tự do 1 MHz (PSC 47, ARR 65535): `Bsp_DelayUs()` |
| ADC | 12 bit; tốc độ 20 bước (205 count/bước), thời gian 59 bước (70 count/bước) |
| nRF24L01 | Kênh 77 đến 80, 2 Mbps, CRC 2 byte, dynamic payload, ACK payload, ARD 750 µs, ARC 3 |

Các chân chưa xác định (**TBD**): `NRF_CSN`, `NRF_CE`, `NRF_IRQ`, nút bấm, LED vị trí, còi, chân ADC. Chi tiết xem `README.md`.

---

## 3. Cấu trúc thư mục và phân tầng

```
firmware/main_board/
├─ Core/      Cube sinh ra, KHÔNG sửa ngoài khối USER CODE
├─ Drivers/   HAL, LL, CMSIS (Cube quản lý)
├─ App/       logic ứng dụng (Inc/, Src/)  - không biết phần cứng
├─ Bsp/       điều khiển phần cứng (Inc/, Src/)
└─ main_board.ioc
```

Phân tầng: `App -> Bsp -> Core/Drivers (HAL, LL) -> Phần cứng`.

- `App` **chỉ gọi `Bsp`**; không include header HAL/LL, không truy cập thanh ghi.
- `Bsp` không chứa logic nghiệp vụ.
- `led7.c` và `led.c` chỉ ghi vào bộ đệm khung của `tm1637.c`; `tm1637.c` gửi cả khung khi có thay đổi.
- Không đặt tên file trùng file Cube sinh (`adc.c`, `usart.c`, `spi.c`, `tim.c`, `gpio.c`): dùng `bsp_adc.c`, `uart_comm.c`...
- Code của dự án nằm trong `App/` và `Bsp/`, **không nhét vào `Core/`**.
- Thư mục `App/`, `Bsp/` phải được khai báo trong CubeIDE (Source Location và Includes), nếu không file sẽ không biên dịch.

---

## 4. Quy tắc bắt buộc

### Phải làm
1. **Chỉ viết giữa các cặp `USER CODE BEGIN/END`** khi sửa file Cube sinh (`main.c`, `stm32f0xx_it.c`...). Code ngoài khối sẽ mất khi Generate Code.
2. **Mọi hàm, struct, enum, macro công khai có comment Doxygen** (xem mục 6).
3. Mọi vòng chờ (SPI, CE, DMA, cờ) phải có **timeout**.
4. Biến dùng chung với ISR phải là **`volatile`**.
5. Dùng kiểu có kích thước: `uint8_t`, `uint16_t`, `uint32_t`.
6. Hằng số đặt tên bằng `#define`/`enum`, ghi **đơn vị** (ví dụ `step_time_100ms`, `run_time_s`).
7. Hàm và biến chỉ dùng nội bộ file thì khai báo **`static`**.
8. Kiểm tra giới hạn mảng và chia cho 0 trước khi dùng.
9. Kiểm tra giá trị trả về của các hàm có thể thất bại.
10. Đặt User Label cho chân trong `.ioc` và dùng macro `XXX_Pin`, `XXX_GPIO_Port`; không ghi cứng `GPIOx`/`GPIO_PIN_x`.

### Không được làm
1. Không sửa trong `Drivers/`.
2. Không gọi `Log_Printf`, `printf`, `HAL_Delay`, hàm blocking lâu **trong ISR**.
3. Không dùng `printf("%f")` (tốn flash trên F030).
4. Không dùng `%` hay chia trong ISR khi tránh được (Cortex-M0 không có lệnh chia phần cứng).
5. Không để "số ma thuật" trong code.
6. Không tạo hàm trùng tên callback HAL (`HAL_..._Callback`) ở nhiều file: mỗi callback chỉ định nghĩa **một lần**, phân biệt theo `Instance`.
7. Không giả định thông tin đang là TBD (xem mục 9); hãy hỏi lại.
8. Không thêm lại soft PWM/fade LED.

---

## 5. Quy ước đặt tên (tóm tắt)

| Loại | Quy ước | Ví dụ |
|---|---|---|
| File | `snake_case`, chữ thường | `uart_comm.c` |
| Hàm công khai | `Module_Action()` | `Uart_RxRead()`, `Nrf_Send()` |
| Hàm nội bộ | `static`, `snake_case` | `static uint8_t spi_xfer(uint8_t tx)` |
| Biến toàn cục | tiền tố `g_` | `g_app_state` |
| Biến `static` trong file | tiền tố `s_` | `s_rx_tail` |
| Macro / hằng | `UPPER_SNAKE_CASE` | `RX_DMA_SIZE` |
| Enum | `MODULE_NAME_VALUE` | `APP_STATE_RUN` |
| Kiểu | hậu tố `_t` | `nrf_pkt_t` |
| Boolean | `is_`, `has_` | `is_locked` |

Header luôn có include guard. Chi tiết: `docs/coding_style.md`.

---

## 6. Chuẩn comment Doxygen

Mọi code mới phải có comment Doxygen. Dùng kiểu `/** ... */` với lệnh `@`. Ngôn ngữ comment: **tiếng Anh** (đề xuất; đổi sang tiếng Việt nếu nhóm thống nhất, nhưng phải nhất quán toàn dự án).

### 6.1. Đầu mỗi file (`.c` và `.h`)

```c
/**
 * @file    uart_comm.c
 * @brief   UART receive driver using circular DMA.
 * @details Received bytes are written by DMA into a ring buffer and read
 *          by the main loop with Uart_RxRead().
 * @author  <tên tác giả>
 * @date    <yyyy-mm-dd>
 */
```

### 6.2. Hàm (đặt ở file `.h` cho hàm công khai, ở `.c` cho hàm `static`)

```c
/**
 * @brief   Read one byte from the UART receive buffer.
 * @details Non-blocking. Must be called from one context only (main loop),
 *          because it modifies the read pointer.
 *
 * @param[out] byte  Pointer that receives the byte read. Must not be NULL.
 *
 * @retval 1  A byte was read and stored in @p byte.
 * @retval 0  The buffer is empty; @p byte is unchanged.
 *
 * @note    Call often enough: the 256-byte buffer fills in about 22 ms
 *          at 115200 baud.
 * @warning Do not call from an ISR.
 */
uint8_t Uart_RxRead(uint8_t *byte);
```

Quy tắc:
- `@brief` bắt buộc, một câu, bắt đầu bằng động từ.
- `@param[in]`, `@param[out]` hoặc `@param[in,out]` cho **từng** tham số, nêu **đơn vị** và giới hạn (ví dụ "unit: 100 ms, range 5..100").
- `@return` (hoặc `@retval` cho từng giá trị) với hàm có trả về.
- `@note`, `@warning` cho điều kiện gọi, ràng buộc ISR/thời gian, tác dụng phụ.
- `@see` để chỉ hàm liên quan khi cần.

### 6.3. Struct, enum, macro, biến

```c
/** @brief Message types of the nRF network protocol. */
typedef enum {
    MSG_NONE = 0,    /**< No message. */
    MSG_POLL,        /**< Master -> slave: poll only. */
    MSG_SET_TARGET   /**< Master -> slave: light the target LED. */
} msg_type_t;

/** Number of bytes in the UART receive DMA buffer. */
#define RX_DMA_SIZE  256U

static volatile uint16_t s_rx_tail;  /**< Next read index, main loop only. */
```

### 6.4. Nhóm module (tùy chọn)

```c
/** @defgroup BSP_UART  UART debug and receive
 *  @{ */
/* ... các khai báo ... */
/** @} */
```

### 6.5. Khi nào cập nhật comment
- Đổi chữ ký hàm, đơn vị, điều kiện gọi, hoặc hành vi thì **cập nhật comment trong cùng commit**.
- Không viết comment lặp lại câu lệnh; giải thích **vì sao** và các ràng buộc.

---

## 7. Các module chính và hàm công khai

### UART debug (`Bsp/bsp_log`, `Bsp/uart_comm`, `App/cmd`)

| Hàm | Vai trò |
|---|---|
| `Log_Write`, `Log_Printf`, `LOG_E`, `LOG_I` | Gửi log qua ring buffer + DMA, không chặn; đầy thì bỏ |
| `Uart_RxStart`, `Uart_RxAvailable`, `Uart_RxRead`, `Uart_RxOverflowTaken` | Nhận qua DMA vòng tròn; phát hiện tràn bằng đếm vòng DMA |
| `cmd_feed`, `cmd_execute`, `cmd_reset` | Ghép dòng lệnh (giữ trạng thái giữa các lần gọi) và chạy lệnh debug |

Vòng lặp chính đọc cạn tối đa 32 byte mỗi lần, và gọi `cmd_reset()` khi `Uart_RxOverflowTaken()` trả 1.

### SPI / nRF24L01 (`Bsp/nrf24`, `App/net_proto`)

| Nhóm | Hàm |
|---|---|
| Thanh ghi | `Nrf_ReadReg`, `Nrf_WriteReg`, `Nrf_ReadRegBuf`, `Nrf_WriteRegBuf`, `Nrf_ReadStatus`, `Nrf_ClearFlags`, `Nrf_FlushTx`, `Nrf_FlushRx`, `Nrf_SelfTest` |
| Cấu hình | `Nrf_Init`, `Nrf_SetChannel`, `Nrf_SetTxAddr`, `Nrf_SetRxAddr`, `Nrf_EnterTx`, `Nrf_EnterRx` |
| Gửi/nhận | `Nrf_Send`, `Nrf_ReadPayloadWidth`, `Nrf_Receive`, `Nrf_WriteAckPayload`, `Nrf_GetLostCount` |
| Mạng | `Net_Init`, `Net_Task`, `Net_SetTarget`, `Net_Stop`, `Net_IsOnline`, `Net_OnHit` |

Trong đó `Nrf_ReadReg`, `Nrf_WriteReg`, `Nrf_SelfTest` đã có code mẫu; phần còn lại là đề xuất, chưa viết.

Gói tin: header 4 byte (`type`, `node_id`, `seq`, `len`) + payload ≤ 28 byte (tổng ≤ 32). `MSG_SET_TARGET` **không có** trường `brightness`.

---

## 8. Lưu ý kỹ thuật dễ sai

### Chip và clock
- **F030 không có TIM2 và TIM4.** Timer dùng được: TIM1, 3, 14, 15, 16, 17 (một số mã có TIM6, 7).
- F0 không có `HAL_RCC_GetPCLK2Freq()` kiểu F1/F4; clock timer lấy theo APB1.
- Nếu APB1 prescaler khác /1 thì clock timer bị nhân đôi: phải tính lại prescaler.
- PA1 là kênh ADC1 (xung đột tiềm tàng với `TM_CLK`). PA0 đến PA7 chỉ nối mức **3,3 V**.

### SPI và nRF24L01
- Giới hạn SPI của nRF24 là 10 Mbps: **không dùng prescaler 4** (12 Mbps).
- Nếu dùng LL ở chế độ 8 bit trên F0: đặt ngưỡng FIFO nhận 1/4, nếu không RXNE không lên và code treo.
- **Giữ CSN thấp suốt một lệnh**, rồi kéo lên cao. Byte đầu nRF trả về luôn là `STATUS`.
- Xóa cờ `STATUS` bằng cách **ghi 1** vào bit đó.
- Khi gặp `MAX_RT`: xóa cờ **và `FLUSH_TX`**, nếu không FIFO TX kẹt.
- Trước mỗi lần hỏi slave: đặt `TX_ADDR` **và** `RX_ADDR_P0`.
- ACK payload của slave phải nạp **trước** lượt hỏi kế tiếp (master nhận ở lượt sau).
- Slave **tự đo thời gian phản xạ** rồi báo lên, vì độ trễ sóng làm sai số.
- Đợi ≥ 100 ms sau cấp nguồn trước khi giao tiếp. Nguồn **3,3 V**, tụ sát VCC.
- Giá trị mặc định để kiểm tra SPI: `CONFIG = 0x08`, `SETUP_AW = 0x03`, `RF_CH = 0x02`.

### UART và DMA
- Bộ đệm nhận 256 byte đầy sau khoảng **22 ms** ở 115200. Đọc chậm thì DMA **ghi đè mà không báo lỗi**.
- Chỉ so sánh hai con trỏ thì **không phân biệt được đầy với rỗng**: dùng bộ đếm vòng DMA.
- `Uart_RxRead` chỉ được gọi từ **một nơi**.
- Kênh DMA mặc định của USART1 trùng SPI1: nếu sau này SPI dùng DMA phải remap.

### TM1637
- Giao thức giống I2C nhưng **không phải I2C** (không có địa chỉ, gửi LSB trước): bit-bang bằng GPIO.
- Nửa chu kỳ khoảng 3 đến 5 µs. Chỉ gửi khi dữ liệu đổi (mỗi lần gửi khoảng 1 ms).

### HAL và LL
- Giao diện hàm giống nhau dù dùng HAL hay LL; chỉ phần bên trong khác.
- Với LL: **phải tự xóa cờ ngắt** trong `xxx_IRQHandler`, không có callback HAL.
- Mặc định: **HAL** (đã bỏ soft PWM nên không còn ngắt tần số cao). Thư viện cuối cùng chưa chốt, xem mục 9.

### Ứng dụng (`app.c`)
- Đơn vị: thời gian bước tính theo **100 ms**, thời gian tập tính theo **giây** (uint16_t, tối đa 300).
- Start khi chưa chọn vị trí nào: còi báo lỗi, **không chạy** (tránh chia cho 0 ở `rand() % count`).
- `App` không dùng `HAL_GetTick()`; dùng `Bsp_Millis()`.

---

## 9. Thông tin chưa chốt (hỏi lại, không tự giả định)

1. Mã chip F030 chính xác.
2. LED vị trí nối TM1637 (độ sáng chung) hay nối GPIO (bật/tắt).
3. Các chân TBD (mục 2) và chân núm ADC.
4. Thư viện: HAL, LL hay trộn.
5. Slave có LED 7 đoạn và còi hay chỉ có LED và cảm biến chạm.
6. Dùng ngắt IRQ nRF24 ngay hay bắt đầu bằng polling (khuyến nghị: polling trước).
7. Có dùng watchdog và lưu cài đặt vào flash hay không.
8. Ngôn ngữ comment trong code (mặc định đề xuất: tiếng Anh).

Khi cần dữ liệu thuộc các mục trên, **hỏi người dùng** thay vì đoán.

---

## 10. Quy trình làm việc

1. Đọc `README.md` và `plan.md` để biết giai đoạn hiện tại.
2. **Commit Git trước mỗi lần Generate Code** của CubeMX.
3. Làm từng module nhỏ, kiểm tra được rồi mới ghép.
4. Sau khi viết code: kiểm tra các điểm ở mục 4, comment Doxygen đầy đủ, build không warning (`-Wall -Wextra`).
5. Nếu đổi thông số phần cứng hoặc giao diện hàm, cập nhật **cả** `README.md`, `plan.md`, `CLAUDE.md` và `CLAUDE_EN.md`.
6. Khi sửa file Cube sinh, nói rõ đã sửa trong khối `USER CODE` nào.
7. Trả lời bằng **tiếng Việt** trừ khi người dùng yêu cầu khác; tên hàm, biến và comment trong code theo quy ước ở mục 5 và 6.
8. Nếu một yêu cầu mâu thuẫn với quy tắc ở mục 4, nêu rõ vấn đề và đề xuất cách thay thế thay vì làm im lặng.
