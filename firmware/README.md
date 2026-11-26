# Badminton Board – Firmware (STM32F030)

Thiết bị luyện phản xạ cầu lông: các LED vị trí sáng ngẫu nhiên, người chơi chạm/bấm đúng vị trí, hiển thị bằng LED 7 đoạn, có còi. Master điều khiển tối đa 6 slave qua nRF24L01.

> **Về tài liệu này:** thông số được tổng hợp từ quá trình thiết kế. Mục ghi **TBD** là chưa xác định, cần điền theo mạch thực tế. Khi README và file `.ioc` khác nhau, **`.ioc` là nguồn đúng**, hãy cập nhật README cho khớp.
>
> **Thay đổi so với bản trước:** dự án **không dùng soft PWM và hiệu ứng fade LED**. Timer dành cho soft PWM (TIM14) được giải phóng, các file `SoftPWM.c/.h` được loại khỏi dự án. LED chỉ bật/tắt.

---

## 1. Tổng quan phần cứng

| Hạng mục | Giá trị |
|---|---|
| MCU | STM32F030 (mã chip chính xác: **TBD**, ví dụ F030C8T6 hoặc F030K6T6) |
| Core | Cortex-M0, tối đa 48 MHz |
| IDE | STM32CubeIDE |
| Giao tiếp không dây | nRF24L01 qua SPI1 (1 master, 6 slave) |
| Hiển thị | TM1637 (LED 7 đoạn; có thể cả LED vị trí, **TBD**) |
| Debug | USART1, 115200 baud |

> F030 **không có TIM2 và TIM4**. Các timer có thể dùng: TIM1, TIM3, TIM14, TIM15, TIM16, TIM17 (một số mã chip có thêm TIM6, TIM7).

---

## 2. Clock

| Thông số | Giá trị |
|---|---|
| Nguồn clock | HSI 8 MHz qua PLL (HSI/2 × 12) |
| SYSCLK / HCLK | **48 MHz** |
| APB1 prescaler | **/1** → PCLK1 = 48 MHz, **clock timer = 48 MHz** |
| Flash latency | 1 wait state |
| SysTick | 1 kHz (tick 1 ms) |

Nếu APB1 prescaler khác /1 thì clock timer bị nhân đôi, phải tính lại prescaler ở mục 5.

---

## 3. UART (debug / lệnh)

| Thông số | Giá trị |
|---|---|
| Ngoại vi | USART1 |
| Chân | **PA9 = TX**, **PA10 = RX** (AF1) |
| Chế độ | Asynchronous, Transmit & Receive |
| Baud rate | **115200**, 8 bit, không parity, 1 stop |
| Over sampling | 16 samples |
| PA10 | Pull-up |
| DMA TX | Kênh USART1_TX, mode **Normal**, Memory → Peripheral |
| DMA RX | Kênh USART1_RX, mode **Circular**, Peripheral → Memory, tăng địa chỉ bộ nhớ, Byte |
| Bộ đệm nhận | 256 byte (đầy sau khoảng 22 ms ở 115200) |
| Bộ đệm log | 256 byte (ring buffer, đầy thì bỏ log chứ không chặn) |
| NVIC | DMA1 channel 2 và 3 interrupt, preemption 2 |

Đấu dây với mạch USB-UART: TX ↔ RX chéo nhau, **GND chung**, mức **3,3 V**. Terminal: 115200, 8N1.

Kênh DMA mặc định của USART1 trùng với SPI1. Dự án dùng SPI blocking nên không xung đột; nếu sau này SPI dùng DMA thì phải remap kênh.

---

## 4. SPI (nRF24L01)

| Thông số | Giá trị |
|---|---|
| Ngoại vi | SPI1 |
| Chế độ | Master, Full-Duplex |
| CPOL / CPHA | **0 / 0** (SPI mode 0) |
| Data size | 8 bit, **MSB first** |
| NSS | **Software** (CSN điều khiển bằng GPIO) |
| Prescaler | **8** → **6 Mbps** ở PCLK 48 MHz |
| CRC, TI mode | Tắt |
| Nếu dùng LL (8-bit trên F0) | Đặt ngưỡng FIFO nhận = 1/4 (`LL_SPI_RX_FIFO_TH_QUARTER`), nếu không cờ RXNE không lên sau 1 byte |

> Giới hạn của nRF24L01 là 10 Mbps. **Không dùng prescaler 4** (12 Mbps).

Cấu hình nRF24L01:

| Thông số | Giá trị |
|---|---|
| Kênh RF | 77 đến 80 (2477 đến 2480 MHz) |
| Tốc độ vô tuyến | 2 Mbps (hạ 1 Mbps nếu rớt gói) |
| CRC | 2 byte |
| Payload | Dynamic payload, ACK payload bật |
| Auto retransmit | ARD khoảng 750 µs, ARC = 3 |
| Địa chỉ | 5 byte, ví dụ `{0xA5, 0x5A, 0xC3, 0x3C, node_id}` |
| Nguồn | **3,3 V**, tụ 10 đến 100 µF + 100 nF sát chân VCC |

---

## 5. Timer

| Timer | Vai trò | Prescaler | ARR | Kết quả | Ngắt | Ưu tiên |
|---|---|---|---|---|---|---|
| **TIM16** | Nhịp scheduler | 47 | 999 | **1 kHz** (1 ms) | có | 2 |
| **TIM17** | `Bsp_DelayUs()` (TM1637) | 47 | 65535 | Đếm tự do 1 MHz | không | – |
| TIM3, TIM14, TIM15 | Dự phòng | | | | | |

Mức ưu tiên NVIC (Cortex-M0 có 4 mức, 0 đến 3):

| Mức | Nguồn ngắt |
|---|---|
| 1 | SysTick |
| 2 | TIM16, DMA UART, EXTI của nRF IRQ |

Nhịp từ TIM16: cờ **10 ms** (quét nút, ADC, `Tm1637_Task`) và tick **100 ms** (`DecreaseCountHalfSecond`, `DecreaseCountSecond`).

---

## 6. GPIO

### Đã chốt

| Chân | Chức năng | Cấu hình | Mức ban đầu |
|---|---|---|---|
| **PA1** | `TM_CLK` (TM1637 CLK) | Output **Open Drain**, Pull-up, Speed Low | High |
| **PA2** | `TM_DIO` (TM1637 DIO) | Output **Open Drain**, Pull-up, Speed Low | High |
| **PA5** | SPI1_SCK | AF0, Speed High | – |
| **PA6** | SPI1_MISO | AF0, No pull | – |
| **PA7** | SPI1_MOSI | AF0, Speed High | – |
| **PA9** | USART1_TX | AF1 | – |
| **PA10** | USART1_RX | AF1, Pull-up | – |

### Chưa xác định (TBD, cần điền theo mạch)

| Nhãn | Chức năng | Cấu hình đề xuất | Chân |
|---|---|---|---|
| `NRF_CSN` | nRF24 chip select | Output Push-Pull, mức đầu **High** | TBD |
| `NRF_CE` | nRF24 chip enable | Output Push-Pull, mức đầu **Low** | TBD |
| `NRF_IRQ` | nRF24 ngắt (tích cực thấp) | Input Pull-up, hoặc EXTI cạnh xuống | TBD |
| `BTN_MODE` | Nút Mode | Input | TBD |
| `BTN_START` | Nút Start | Input | TBD |
| `BTN_TRAIN_1..8` | Nút vị trí 1 đến 8 | Input | TBD |
| `LED_POS1..8` | 8 LED vị trí (nếu nối GPIO, chỉ bật/tắt) | Output Push-Pull | TBD |
| `BUZZER` | Còi | Output | TBD |
| `ADC_POT` | Núm chỉnh tốc độ/thời gian | Analog, ADC 12 bit | TBD |

Đặt **User Label** cho từng chân trong `.ioc` (ví dụ `NRF_CSN`) để Cube sinh macro `NRF_CSN_Pin`, `NRF_CSN_GPIO_Port` trong `main.h`; code không ghi cứng `GPIOx`/`GPIO_PIN_x`.

### Điểm cần kiểm tra

- **PA1 là kênh ADC1.** Nếu núm chỉnh ADC đang dùng PA1 thì xung đột với `TM_CLK`.
- **PA2 là chân TX mặc định của USART1.** UART đã chuyển sang PA9/PA10 nên không còn xung đột.
- **Mức 5 V:** PA0 đến PA7 chỉ nên nối mức **3,3 V**. Cấp nguồn module TM1637 bằng 3,3 V, hoặc kiểm tra datasheet nếu dùng 5 V.
- **LED vị trí:** nối TM1637 (độ sáng chung, 8 mức) hay nối GPIO của MCU (bật/tắt). Cần chốt phương án.

---

## 7. ADC (núm chỉnh)

| Thông số | Giá trị |
|---|---|
| Độ phân giải | 12 bit (0 đến 4095) |
| Tốc độ (speed) | 20 bước, mỗi bước **205** count → thời gian bước 0,5 s đến 10 s |
| Thời gian tập | 59 bước, mỗi bước **70** count → 10 s đến 300 s (bước 5 s) |

---

## 8. Thư viện ngoại vi (HAL / LL)

Soft PWM đã bỏ nên không còn ngắt tần số cao, lý do chính để dùng LL không còn. Giao diện hàm ở mục 9 và 10 **giống nhau** dù cài đặt bằng HAL hay LL; chỉ phần bên trong khác.

| | HAL | LL |
|---|---|---|
| Ưu | Viết nhanh, ít lỗi cờ/FIFO, nhiều tài liệu | Nhẹ flash/RAM, ngắt nhanh |
| Nhược | Tốn flash hơn | Phải tự xóa cờ ngắt, tự xử lý FIFO SPI, không có callback |
| Khi nên dùng | **Đề xuất mặc định** | Khi flash F030 không đủ, hoặc đã chọn LL từ đầu |

**Quyết định cần chốt (TBD):** HAL hoàn toàn, LL hoàn toàn, hay trộn (chọn riêng từng ngoại vi trong *Project Manager, Advanced Settings*). Mã mẫu trong tài liệu dùng HAL; với LL cần đổi `HAL_GPIO_WritePin` thành `LL_GPIO_SetOutputPin`/`ResetOutputPin`, xử lý ngắt thẳng trong `xxx_IRQHandler`, và `HAL_GetTick()` thành bộ đếm tự viết trong `SysTick_Handler`.

---

## 9. Hàm UART dùng để debug

Đặt trong `Bsp/` (`bsp_log`, `uart_comm`) và `App/` (`cmd`). Mọi hàm gọi từ **vòng lặp chính**, không gọi trong ISR (trừ khi ghi chú khác).

### 9.1. Log (gửi) – `bsp_log.h/.c`

| Hàm | Mô tả | Lưu ý |
|---|---|---|
| `void Log_Write(const char *data, uint16_t len)` | Đưa `len` byte vào ring buffer, DMA gửi nền | Không chặn; buffer đầy thì **bỏ phần còn lại** |
| `void Log_Printf(const char *fmt, ...)` | Định dạng như `printf` rồi gọi `Log_Write` | Dòng tối đa khoảng 80 ký tự; **không dùng `%f`** |
| `LOG_E(fmt, ...)`, `LOG_I(fmt, ...)` | Macro log theo mức (`LOG_LEVEL`) | Tự thêm tiền tố `[E]`/`[I]` và `\r\n`; tắt hẳn khi `LOG_LEVEL` thấp |
| `int _write(int file, char *ptr, int len)` | (Tùy chọn) cho `printf` chuẩn, dùng khi bring-up | Blocking, chậm; không dùng trong ISR |

Nội bộ: `log_kick()` khởi động DMA cho đoạn dữ liệu kế tiếp, và callback hoàn tất truyền (`HAL_UART_TxCpltCallback`) cập nhật con trỏ đuôi rồi gọi tiếp `log_kick()`.

Ví dụ:

```c
Log_Printf("[%lu] state=%u pos=0x%02X\r\n", Bsp_Millis(), state, mask);
LOG_I("nRF status=0x%02X", status);
```

### 9.2. Nhận – `uart_comm.h/.c`

| Hàm | Mô tả | Lưu ý |
|---|---|---|
| `void Uart_RxStart(void)` | Khởi động nhận DMA vòng tròn, đặt con trỏ đọc về 0 | Gọi một lần sau khi khởi tạo; cũng gọi lại khi có lỗi UART |
| `uint16_t Uart_RxAvailable(void)` | Số byte đang chờ đọc | Phân biệt được đầy (đúng 256) với rỗng nhờ đếm vòng DMA |
| `uint8_t Uart_RxRead(uint8_t *byte)` | Đọc 1 byte; trả 1 nếu đọc được, 0 nếu trống | Chỉ gọi từ **một nơi** (vòng lặp chính) |
| `uint8_t Uart_RxOverflowTaken(void)` | Trả 1 nếu từng bị ghi đè bộ đệm, rồi xóa cờ | Khi trả 1 phải gọi `cmd_reset()` và ghi log |

Nội bộ: `rx_get_head()` tính vị trí DMA đang ghi từ thanh ghi đếm còn lại; callback `HAL_UART_RxCpltCallback` tăng bộ đếm số vòng (`s_laps`); `HAL_UART_ErrorCallback` khởi động lại nhận khi có lỗi (ORE...).

Cách dùng trong vòng lặp chính (đọc cạn, giới hạn số byte mỗi lần):

```c
uint8_t c;
uint8_t n = 0;
while ((n++ < 32) && Uart_RxRead(&c)) {
    cmd_feed(c);
}
if (Uart_RxOverflowTaken()) {
    cmd_reset();
    LOG_E("UART RX overflow");
}
```

Điều kiện: vòng lặp chính gọi đoạn này **thường xuyên hơn nhiều** so với thời gian đầy bộ đệm (22 ms ở 115200).

### 9.3. Lệnh debug – `cmd.h/.c`

| Hàm | Mô tả |
|---|---|
| `void cmd_feed(uint8_t c)` | Ghép từng byte thành dòng lệnh, giữ trạng thái giữa các lần gọi; bỏ `\r`, xử lý khi gặp `\n`; dòng quá dài thì bỏ cả dòng |
| `void cmd_execute(char *line)` | Phân tích và chạy một dòng lệnh đầy đủ |
| `void cmd_reset(void)` | Xóa dòng đang dở (khi tràn bộ đệm hoặc timeout) |

Lệnh mẫu cần có:

| Lệnh | Tác dụng |
|---|---|
| `status` | In trạng thái máy trạng thái, tốc độ, thời gian, mặt nạ vị trí |
| `speed <n>` | Đặt thời gian bước (đơn vị 100 ms) |
| `pos <mask>` | Đặt mặt nạ vị trí (hex, ví dụ `0x2A`) |
| `nrf status` | In `STATUS`, `OBSERVE_TX`, `FIFO_STATUS`, kênh RF |
| `nrf test` | Chạy `Nrf_SelfTest()` |
| `net status` | In danh sách slave online/offline, số gói mất |

Nên có timeout (khoảng 500 ms đến 1 s) để xóa dòng dở dang nếu không thấy `\n`.

---

## 10. Hàm SPI dùng để giao tiếp nRF24L01

Đặt trong `Bsp/` (`nrf24`) và `App/` (`net_proto`). Mọi vòng chờ SPI/CE **phải có timeout**.

### 10.1. Hằng số lệnh và thanh ghi (`nrf24.h`)

| Lệnh | Mã | Ý nghĩa |
|---|---|---|
| `R_REGISTER` | `0x00 \| addr` | Đọc thanh ghi |
| `W_REGISTER` | `0x20 \| addr` | Ghi thanh ghi |
| `R_RX_PAYLOAD` | `0x61` | Đọc dữ liệu nhận |
| `W_TX_PAYLOAD` | `0xA0` | Ghi dữ liệu gửi |
| `FLUSH_TX` / `FLUSH_RX` | `0xE1` / `0xE2` | Xóa FIFO |
| `R_RX_PL_WID` | `0x60` | Đọc độ dài payload nhận |
| `W_ACK_PAYLOAD` | `0xA8 \| pipe` | Nạp dữ liệu trả lời kèm ACK |
| `NOP` | `0xFF` | Không làm gì, dùng để đọc `STATUS` |

| Thanh ghi | Địa chỉ | Thanh ghi | Địa chỉ |
|---|---|---|---|
| `CONFIG` | `0x00` | `RX_ADDR_P1` | `0x0B` |
| `EN_AA` | `0x01` | `TX_ADDR` | `0x10` |
| `EN_RXADDR` | `0x02` | `RX_PW_P0` | `0x11` |
| `SETUP_AW` | `0x03` | `FIFO_STATUS` | `0x17` |
| `SETUP_RETR` | `0x04` | `DYNPD` | `0x1C` |
| `RF_CH` | `0x05` | `FEATURE` | `0x1D` |
| `RF_SETUP` | `0x06` | `OBSERVE_TX` | `0x08` |
| `STATUS` | `0x07` | `RX_ADDR_P0` | `0x0A` |

Cờ trong `STATUS`: `RX_DR` (bit 6, đã nhận), `TX_DS` (bit 5, gửi xong), `MAX_RT` (bit 4, hết số lần gửi lại). **Xóa cờ bằng cách ghi 1 vào bit đó.**

### 10.2. Driver – `nrf24.h/.c`

Trạng thái: **[mẫu]** = đã có code mẫu trong quá trình thiết kế, **[cần viết]** = đề xuất, chưa có code.

| Hàm | Mô tả | Trạng thái |
|---|---|---|
| `uint8_t Nrf_ReadReg(uint8_t reg)` | Đọc 1 thanh ghi (kéo CSN thấp, gửi `R_REGISTER`, đọc 1 byte, kéo CSN cao) | [mẫu] |
| `void Nrf_WriteReg(uint8_t reg, uint8_t value)` | Ghi 1 thanh ghi | [mẫu] |
| `uint8_t Nrf_SelfTest(void)` | Ghi `RF_CH` rồi đọc lại; trả 1 nếu khớp (SPI thông) | [mẫu] |
| `void Nrf_ReadRegBuf(uint8_t reg, uint8_t *buf, uint8_t len)` | Đọc thanh ghi nhiều byte (địa chỉ 5 byte) | [cần viết] |
| `void Nrf_WriteRegBuf(uint8_t reg, const uint8_t *buf, uint8_t len)` | Ghi thanh ghi nhiều byte | [cần viết] |
| `uint8_t Nrf_ReadStatus(void)` | Gửi `NOP`, trả `STATUS` | [cần viết] |
| `void Nrf_ClearFlags(uint8_t flags)` | Ghi 1 vào các cờ `RX_DR/TX_DS/MAX_RT` để xóa | [cần viết] |
| `void Nrf_FlushTx(void)`, `void Nrf_FlushRx(void)` | Xóa FIFO; **bắt buộc `FlushTx` sau `MAX_RT`** | [cần viết] |
| `uint8_t Nrf_Init(void)` | Đợi ≥ 100 ms sau cấp nguồn, đặt CSN cao/CE thấp, cấu hình CRC, dynamic payload, ACK payload, `SETUP_RETR`, tốc độ, kênh; trả 1 nếu thành công | [cần viết] |
| `void Nrf_SetChannel(uint8_t ch)` | Đặt `RF_CH` | [cần viết] |
| `void Nrf_SetTxAddr(const uint8_t addr[5])` | Đặt `TX_ADDR` **và** `RX_ADDR_P0` (để nhận auto-ACK) | [cần viết] |
| `void Nrf_SetRxAddr(uint8_t pipe, const uint8_t addr[5])` | Đặt địa chỉ nhận cho một pipe | [cần viết] |
| `void Nrf_EnterTx(void)`, `void Nrf_EnterRx(void)` | Chuyển chế độ (bit `PRIM_RX`, điều khiển CE); chờ ~130 µs | [cần viết] |
| `uint8_t Nrf_Send(const uint8_t *data, uint8_t len)` | Ghi `W_TX_PAYLOAD`, xung CE ≥ 10 µs, chờ `TX_DS` hoặc `MAX_RT` (có timeout), xóa cờ; trả 1 nếu gửi được | [cần viết] |
| `uint8_t Nrf_ReadPayloadWidth(void)` | Gửi `R_RX_PL_WID`, trả độ dài payload | [cần viết] |
| `uint8_t Nrf_Receive(uint8_t *buf, uint8_t maxlen)` | Nếu `RX_DR`: đọc độ dài, đọc payload, xóa cờ; trả số byte (0 nếu không có) | [cần viết] |
| `void Nrf_WriteAckPayload(uint8_t pipe, const uint8_t *data, uint8_t len)` | Nạp dữ liệu trả lời kèm ACK (cho slave) | [cần viết] |
| `uint8_t Nrf_GetLostCount(void)` | Đọc `OBSERVE_TX.ARC_CNT` (số lần gửi lại) | [cần viết] |

Hàm truyền SPI nội bộ (dùng chung cho mọi hàm trên):

```c
static void nrf_csn_low(void);
static void nrf_csn_high(void);
static uint8_t spi_xfer(uint8_t tx);    /* truyền và nhận 1 byte, có timeout */
```

Quy tắc: **giữ CSN thấp suốt một lệnh** (byte lệnh và dữ liệu đi kèm), rồi kéo lên cao. Byte đầu tiên module trả về luôn là `STATUS`.

Giá trị mặc định sau khi cấp nguồn (để kiểm tra SPI): `CONFIG` = `0x08`, `SETUP_AW` = `0x03`, `RF_CH` = `0x02`. Đọc ra toàn `0x00` hoặc `0xFF` nghĩa là SPI chưa thông (sai dây, thiếu nguồn, CSN sai mức).

### 10.3. Giao thức mạng – `net_proto.h/.c`

| Hàm | Mô tả | Vai trò |
|---|---|---|
| `void Net_Init(uint8_t role, uint8_t node_id)` | Khởi tạo vai trò master/slave và địa chỉ | cả hai |
| `void Net_Task(void)` | Chạy vòng hỏi (master) hoặc xử lý gói nhận (slave); gọi định kỳ | cả hai |
| `uint8_t Net_SetTarget(uint8_t id, uint16_t on_time_100ms, uint8_t flags)` | Gửi lệnh bật LED mục tiêu cho slave | master |
| `uint8_t Net_Stop(uint8_t id)` | Gửi lệnh dừng, tắt LED | master |
| `uint8_t Net_IsOnline(uint8_t id)` | Slave còn phản hồi hay không (< 3 lần mất liên tiếp) | master |
| `void Net_OnHit(uint8_t id, uint16_t reaction_ms)` | Callback khi nhận `MSG_EVT_HIT` | master |

Gói tin: header 4 byte (`type`, `node_id`, `seq`, `len`) + payload tối đa 28 byte, tổng ≤ 32 byte.

| Loại tin | Hướng | Nội dung payload |
|---|---|---|
| `MSG_POLL` | master → slave | không có |
| `MSG_SET_TARGET` | master → slave | `on_time_100ms` (u16), `flags` (u8; bit0: kêu còi) |
| `MSG_STOP` | master → slave | không có |
| `MSG_CONFIG` | master → slave | cấu hình (tùy chọn) |
| `MSG_EVT_HIT` | slave → master | `reaction_ms` (u16), `hit_count` (u8) |
| `MSG_STATUS` | slave → master | `state`, `vbat_100mv`, `fw_ver`, `lost_count` |

> Vì không còn PWM nên **bỏ trường `brightness`** khỏi `MSG_SET_TARGET` (nếu dùng TM1637 thì độ sáng là cài đặt chung của chip).

Vòng hỏi của master: lần lượt id 1 đến 6, đặt địa chỉ, gửi, chờ ACK, đọc ACK payload; gặp `MAX_RT` thì xóa cờ, `FlushTx`, tăng bộ đếm mất; sau 3 lần liên tiếp đánh dấu offline.

---

## 11. Cấu trúc thư mục

```
BadmintonBoard/                    <- git repo
├─ docs/                           tài liệu (coding_style.md, sơ đồ trạng thái)
├─ hardware/                       schematic, PCB, BOM
├─ firmware/
│  └─ main_board/                  project STM32CubeIDE
│     ├─ Core/                     Cube sinh, KHÔNG sửa ngoài khối USER CODE
│     │  ├─ Inc/                   main.h, gpio.h, tim.h, usart.h, spi.h ...
│     │  └─ Src/                   main.c, gpio.c, tim.c, usart.c, spi.c,
│     │                            stm32f0xx_it.c ...
│     ├─ Drivers/                  HAL, LL, CMSIS (do Cube quản lý)
│     ├─ App/                      logic ứng dụng, không biết phần cứng
│     │  ├─ Inc/                   app.h, app_config.h, cmd.h, net_proto.h
│     │  └─ Src/                   app.c, cmd.c, net_proto.c
│     ├─ Bsp/                      điều khiển phần cứng
│     │  ├─ Inc/                   led.h, led7.h, tm1637.h, buzzer.h,
│     │  │                         button.h, uart_comm.h, bsp_log.h,
│     │  │                         nrf24.h, bsp_timer.h
│     │  └─ Src/                   (các file .c tương ứng)
│     ├─ Debug/                    output build (không đưa lên Git)
│     └─ main_board.ioc            cấu hình CubeMX
├─ tools/                          script, công cụ test
├─ README.md
├─ plan.md
└─ .gitignore
```

Phân tầng:

```
App  ->  Bsp  ->  Core / Drivers (HAL, LL)  ->  Phần cứng
```

- `App` chỉ gọi `Bsp`, không gọi trực tiếp HAL/LL hay thanh ghi.
- `Bsp` không chứa logic nghiệp vụ.
- Hiển thị: `led7.c` và `led.c` chỉ ghi vào bộ đệm khung của `tm1637.c`; `tm1637.c` gửi cả khung lên chip khi có thay đổi.
- Không đặt tên file trùng file Cube sinh (`adc.c`, `usart.c`...): dùng `bsp_adc.c`, `uart_comm.c`.
- **Đã loại:** `SoftPWM.c/.h`, hiệu ứng fade LED.

---

## 12. Thiết lập dự án trong CubeIDE

Thư mục `App/` và `Bsp/` không tự được biên dịch, cần khai báo:

1. Chuột phải project, **Properties**, **C/C++ General**, **Paths and Symbols**.
2. Tab **Source Location**: Add Folder `App`, `Bsp`.
3. Tab **Includes** (GNU C): thêm `App/Inc`, `Bsp/Inc` (tick *Add to all configurations*).
4. Trong `.ioc`, **Project Manager, Code Generator**: tick *Generate peripheral initialization as a pair of '.c/.h' files per peripheral*.

Chèn code vào file Cube sinh chỉ giữa các cặp `USER CODE BEGIN/END`:

```c
/* USER CODE BEGIN 2 */
Bsp_TimerStart();       /* TIM16 1 kHz, TIM17 1 MHz */
Uart_RxStart();
Nrf_Init();
App_Init();
/* USER CODE END 2 */
while (1)
{
    /* USER CODE BEGIN 3 */
    App_Proc();
    /* USER CODE END 3 */
}
```

**Commit Git trước mỗi lần Generate Code.**

`.gitignore` gợi ý: `Debug/`, `Release/`, `.settings/`, `*.o`, `*.elf`, `*.map`, `*.list`. Đưa lên repo: `Core/`, `App/`, `Bsp/`, `.ioc`, `.project`, `.cproject`, `.mxproject`, file linker `.ld`.

---

## 13. Máy trạng thái của ứng dụng (`app.c`)

```
SETUP_INIT -> SETUP_SPEED <-> SETUP_TIME
                   |              |
                   +---- Start ---+--> PRERUN -> READY (3 s) -> RUN -> STOP -> SETUP_SPEED
```

- Chọn vị trí: nút Train 1 đến 8 (bật/tắt), thao tác được ở cả hai màn hình cài đặt.
- Start khi chưa chọn vị trí nào: còi báo lỗi, không chạy.
- Kết thúc khi hết thời gian cài đặt hoặc bấm Start lần nữa.

---

## 14. Quy ước và việc cần làm

Quy ước lập trình: xem [`docs/coding_style.md`](docs/coding_style.md). Kế hoạch từng bước: xem [`plan.md`](plan.md).

Việc cần làm ngay:

- [ ] Xác định mã chip F030 chính xác và điền vào mục 1.
- [ ] Điền các chân **TBD** ở mục 6.
- [ ] Chốt phương án LED vị trí: nối TM1637 hay nối GPIO (bật/tắt).
- [ ] Chốt thư viện: HAL, LL hay trộn (mục 8).
- [ ] Xóa `SoftPWM.c/.h` khỏi dự án và bỏ cấu hình TIM14 trong `.ioc` nếu không dùng.
- [ ] Đổi `HAL_GetTick()` trong `app.c` thành `Bsp_Millis()` để `App` không phụ thuộc HAL.
