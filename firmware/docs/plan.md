# Kế hoạch hoàn thành dự án – Badminton Board (STM32F030)

Tài liệu này mô tả các bước theo thứ tự nên làm, từ chuẩn bị đến phát hành. Thông số phần cứng và danh sách hàm xem `README.md`, quy ước code xem `docs/coding_style.md`.

**Phạm vi:** dự án **không dùng soft PWM và hiệu ứng fade LED**. LED chỉ bật/tắt.

**Nguyên tắc xuyên suốt**
- Làm từng giai đoạn nhỏ, **mỗi giai đoạn phải chạy được và kiểm tra được** trước khi sang giai đoạn sau.
- Bring-up từ dưới lên: nền tảng, từng module riêng lẻ, rồi mới ghép vào ứng dụng.
- Có log UART sớm để debug mọi giai đoạn sau.
- Commit Git sau mỗi giai đoạn và **trước mỗi lần Generate Code** của CubeMX.
- Mọi vòng chờ (SPI, CE, DMA) đều có **timeout**.

**Cách đọc:** mỗi giai đoạn gồm *Mục tiêu*, *Việc cần làm* và *Tiêu chí hoàn thành*.

---

## Tổng quan các giai đoạn

| # | Giai đoạn | Phụ thuộc | Kết quả chính |
|---|---|---|---|
| 0 | Chuẩn bị | – | Chip, chân, repo, project build được |
| 1 | Nền tảng | 0 | Clock, tick, timer, GPIO chạy đúng |
| 2 | UART debug | 1 | Có log và nhận lệnh qua UART |
| 3 | Hiển thị | 1, 2 | TM1637 hiển thị số, LED |
| 4 | Đầu vào | 1, 2 | ADC và nút bấm đọc đúng |
| 5 | Ứng dụng đơn board | 3, 4 | Thiết bị chạy độc lập (chưa cần mạng) |
| 6 | Driver nRF24 (SPI) | 1, 2 | Hai module nói chuyện được với nhau |
| 7 | Giao thức mạng | 6 | Master hỏi, slave đáp |
| 8 | Firmware slave | 7 | Slave điều khiển LED, báo chạm |
| 9 | Tích hợp và kiểm thử | 5, 8 | Hệ thống 1 master + 6 slave chạy ổn định |
| 10 | Hoàn thiện và phát hành | 9 | Tài liệu, tag phiên bản |

Giai đoạn 3, 4 và giai đoạn 6 độc lập với nhau, có thể làm song song nếu có nhiều người; một người thì làm theo thứ tự trên.

---

## Giai đoạn 0 – Chuẩn bị

**Mục tiêu:** chốt các quyết định về phần cứng và dựng được dự án trống build thành công.

**Việc cần làm**
- [ ] Xác định **mã chip F030 chính xác** (flash/RAM, số timer, số kênh DMA).
- [ ] Hoàn thành bảng chân (README mục 6): CSN, CE, IRQ, nút bấm, LED, còi, ADC.
- [ ] Kiểm tra **xung đột chân**, đặc biệt PA1 (`TM_CLK`) với kênh ADC1.
- [ ] **Chốt phương án LED vị trí:** nối TM1637 (độ sáng chung) hay nối GPIO (chỉ bật/tắt).
- [ ] **Chốt thư viện:** HAL, LL hay trộn (README mục 8). Với việc đã bỏ soft PWM, HAL là mặc định hợp lý.
- [ ] **Loại bỏ soft PWM:** xóa `SoftPWM.c/.h`, bỏ cấu hình TIM14 trong `.ioc` nếu không dùng.
- [ ] Tạo repo Git, `.gitignore`, cấu trúc thư mục theo README mục 11.
- [ ] Tạo project STM32CubeIDE từ `.ioc`; tick *Generate peripheral initialization as a pair of '.c/.h' files*.
- [ ] Khai báo `App`, `Bsp` trong **Source Location** và `App/Inc`, `Bsp/Inc` trong **Includes**.
- [ ] Đặt **User Label** cho tất cả chân trong `.ioc`.

**Tiêu chí hoàn thành:** project build không lỗi, nạp được vào board, một chân GPIO nhấp nháy LED thử.

---

## Giai đoạn 1 – Nền tảng

**Mục tiêu:** clock, tick và timer chạy đúng với thông số đã tính.

**Việc cần làm**
- [ ] Cấu hình clock: HSI qua PLL, SYSCLK 48 MHz, APB1 /1, flash latency 1.
- [ ] SysTick 1 kHz; viết `Bsp_Millis()` (thay `HAL_GetTick()` trong App).
- [ ] TIM17: bộ đếm tự do 1 MHz và `Bsp_DelayUs()`.
- [ ] TIM16: nhịp 1 kHz, tạo cờ 10 ms và tick 100 ms (`DecreaseCountHalfSecond`, `DecreaseCountSecond`).
- [ ] Đặt mức ưu tiên NVIC theo README mục 5.
- [ ] Kiểm tra bằng **máy hiện sóng hoặc logic analyzer**: toggle một chân trong ISR TIM16.

**Tiêu chí hoàn thành:** đo được TIM16 = 1 kHz, tick 1 ms chính xác, `Bsp_DelayUs(100)` đo được khoảng 100 µs; sai số dưới 1%.

---

## Giai đoạn 2 – UART debug

**Mục tiêu:** có công cụ quan sát và điều khiển firmware từ máy tính.

### 2.1. Cấu hình
- [ ] USART1: PA9 TX, PA10 RX (Pull-up), 115200 8N1.
- [ ] DMA TX (Normal) và DMA RX (Circular); bật ngắt DMA1 channel 2 và 3, ưu tiên 2.

### 2.2. Gửi log (`bsp_log.c`)
- [ ] `Log_Write(const char *data, uint16_t len)`: ring buffer + DMA, buffer đầy thì bỏ chứ không chặn.
- [ ] `Log_Printf(const char *fmt, ...)`: định dạng rồi gọi `Log_Write`; không dùng `%f`.
- [ ] Callback hoàn tất truyền DMA cập nhật con trỏ đuôi và gửi tiếp.
- [ ] Macro `LOG_E`, `LOG_I` theo `LOG_LEVEL`.
- [ ] (Tùy chọn) `_write()` cho `printf` khi bring-up.

### 2.3. Nhận (`uart_comm.c`)
- [ ] `Uart_RxStart()`: nhận DMA vòng tròn.
- [ ] `Uart_RxAvailable()`: số byte chờ; phân biệt được đầy với rỗng nhờ đếm vòng DMA (bật ngắt Transfer Complete, tắt Half Transfer).
- [ ] `Uart_RxRead(uint8_t *byte)`: đọc 1 byte, trả 1/0.
- [ ] `Uart_RxOverflowTaken()`: báo và xóa cờ khi bị ghi đè bộ đệm.
- [ ] Callback lỗi UART khởi động lại nhận.

### 2.4. Lệnh debug (`cmd.c`)
- [ ] `cmd_feed(uint8_t c)`: ghép dòng, giữ trạng thái giữa các lần gọi, bỏ `\r`, xử lý khi gặp `\n`, bỏ dòng quá dài.
- [ ] `cmd_execute(char *line)` và `cmd_reset()`; timeout xóa dòng dở dang.
- [ ] Lệnh mẫu: `status`, `speed <n>`, `pos <mask>`.
- [ ] Trong vòng lặp chính: đọc cạn tối đa 32 byte mỗi lần, kiểm tra `Uart_RxOverflowTaken()`.

**Quy tắc:** không gọi `Log_Printf`, `printf` hay `cmd_*` trong ISR.

**Tiêu chí hoàn thành:** gõ lệnh trong terminal và nhận phản hồi; dán một đoạn văn bản dài không làm treo và nếu tràn thì có log báo; log không làm chậm vòng lặp chính.

---

## Giai đoạn 3 – Hiển thị (TM1637)

**Mục tiêu:** hiển thị số và LED qua TM1637, chỉ cập nhật khi có thay đổi.

**Việc cần làm**
- [ ] PA1/PA2 Open Drain, kéo lên, mức ban đầu High.
- [ ] `tm1637.c`: start/stop/ghi byte, bộ đệm khung, cờ `dirty`, `Tm1637_Task()`, `Tm1637_SetBrightness()`.
- [ ] `led7.c`: `LED7_Display`, `LED7_DisplaySpeed` (hiển thị dạng `x.x`).
- [ ] `led.c`: `LED_Display(mask)` (ghi vào chữ số dành cho LED nếu dùng TM1637, hoặc ghi GPIO nếu nối trực tiếp).
- [ ] Gọi `Tm1637_Task()` định kỳ 10 đến 50 ms.
- [ ] Kiểm tra số chữ số và vị trí dấu chấm/hai chấm theo đúng module.

**Tiêu chí hoàn thành:** hiển thị đúng 0 đến 9999, LED vị trí sáng đúng theo mặt nạ, không nhấp nháy, mỗi lần cập nhật chặn vòng lặp không quá khoảng 1 ms.

---

## Giai đoạn 4 – Đầu vào (ADC và nút bấm)

**Mục tiêu:** đọc ổn định núm chỉnh và các nút.

**Việc cần làm**
- [ ] ADC 12 bit trên chân núm chỉnh (tránh PA1 nếu đã dùng cho `TM_CLK`).
- [ ] Lọc nhiễu ADC (trung bình hoặc hysteresis) để giá trị không nhảy.
- [ ] Tính `SPEED_STEP`, `TIME_STEP` từ `ADC_MAX`, không để số cứng khó hiểu.
- [ ] Gộp ba file `Btn_*` thành `button.c` (Mode, Start, Train 1 đến 8).
- [ ] Chống rung nút (quét mỗi 10 ms, xác nhận ít nhất 2 lần liên tiếp).
- [ ] `BtnX_Pressed()` trả về **một lần cho mỗi lần bấm** (cạnh nhấn).

**Tiêu chí hoàn thành:** xoay núm đủ 20 mức tốc độ và 59 mức thời gian, giá trị không nhảy khi để yên; bấm nút một lần chỉ ra một sự kiện.

---

## Giai đoạn 5 – Ứng dụng đơn board

**Mục tiêu:** thiết bị chạy trọn vẹn một mình, **chưa dùng mạng**.

**Việc cần làm**
- [ ] Đưa `app.c` (bản đã sửa lỗi) vào dự án, thay `HAL_GetTick()` bằng `Bsp_Millis()`.
- [ ] Chuyển trạng thái sang `enum`, đổi tên biến theo `coding_style.md`.
- [ ] Kiểm tra từng trạng thái: `SETUP_SPEED`, `SETUP_TIME`, `PRERUN`, `READY` (đếm 3 giây), `RUN`, `STOP`.
- [ ] Kiểm tra trường hợp biên:
  - [ ] Bấm Start khi chưa chọn vị trí: còi báo lỗi, không chạy.
  - [ ] Chọn đúng 1 vị trí: LED sáng liên tục, không lỗi và không treo.
  - [ ] Hết giờ cài đặt: còi kêu, về màn hình cài đặt.
  - [ ] Bấm Start giữa chừng: dừng và tắt LED, còi.
- [ ] Kiểm tra đơn vị: thời gian bước theo 100 ms, thời gian tập theo giây.
- [ ] Thêm lệnh debug `status` in đủ trạng thái.

**Tiêu chí hoàn thành:** chạy nhiều vòng cài đặt, đếm ngược, tập, dừng liên tiếp mà không treo; thời gian đo bằng đồng hồ khớp giá trị cài đặt (sai số dưới 1%).

---

## Giai đoạn 6 – Driver nRF24L01 (SPI)

**Mục tiêu:** hai module nRF24 trao đổi được gói tin ổn định.

### 6.1. Phần cứng và cấu hình
- [ ] SPI1: mode 0, prescaler 8 (6 Mbps), NSS software, 8 bit MSB first.
- [ ] GPIO `NRF_CSN` (mức đầu High), `NRF_CE` (mức đầu Low), `NRF_IRQ` (Input Pull-up).
- [ ] Nguồn 3,3 V, tụ 10 đến 100 µF + 100 nF sát VCC, dây SPI ngắn.
- [ ] Nếu dùng LL: đặt ngưỡng FIFO nhận 1/4 cho chế độ 8 bit.

### 6.2. Truy cập thanh ghi (`nrf24.c`)
- [ ] `spi_xfer()` có timeout; `nrf_csn_low()`, `nrf_csn_high()`.
- [ ] `Nrf_ReadReg`, `Nrf_WriteReg`, `Nrf_ReadRegBuf`, `Nrf_WriteRegBuf`.
- [ ] `Nrf_ReadStatus()` (dùng lệnh NOP), `Nrf_ClearFlags()`, `Nrf_FlushTx()`, `Nrf_FlushRx()`.
- [ ] `Nrf_SelfTest()`: ghi `RF_CH` rồi đọc lại.
- [ ] Kiểm tra giá trị mặc định sau cấp nguồn: `CONFIG = 0x08`, `SETUP_AW = 0x03`, `RF_CH = 0x02`.
- [ ] Lệnh debug: `nrf test`, `nrf status`.

### 6.3. Cấu hình và gửi/nhận
- [ ] `Nrf_Init()`: đợi ≥ 100 ms sau cấp nguồn, cấu hình CRC 2 byte, dynamic payload, ACK payload, `SETUP_RETR` (ARD 750 µs, ARC 3), tốc độ 2 Mbps.
- [ ] `Nrf_SetChannel()`, `Nrf_SetTxAddr()` (đặt cả `TX_ADDR` và `RX_ADDR_P0`), `Nrf_SetRxAddr()`.
- [ ] `Nrf_EnterTx()`, `Nrf_EnterRx()`.
- [ ] `Nrf_Send()`: xung CE ≥ 10 µs, chờ `TX_DS` hoặc `MAX_RT`, xóa cờ; **gặp `MAX_RT` phải `FlushTx`**.
- [ ] `Nrf_ReadPayloadWidth()`, `Nrf_Receive()`: đọc cả payload và xóa cờ `RX_DR`.
- [ ] `Nrf_WriteAckPayload()` (cho slave), `Nrf_GetLostCount()` (đọc `OBSERVE_TX`).
- [ ] Bắt đầu bằng **polling**; khi chạy ổn mới chuyển sang ngắt IRQ (EXTI cạnh xuống, chỉ đặt cờ trong ISR, xử lý ở vòng lặp chính).

**Tiêu chí hoàn thành:** một module gửi 1000 gói liên tiếp, module kia nhận đủ; tỷ lệ mất gói thấp; đọc được số lần gửi lại; `nrf test` luôn đạt.

---

## Giai đoạn 7 – Giao thức mạng

**Mục tiêu:** master hỏi lần lượt từng slave và nhận phản hồi.

**Việc cần làm**
- [ ] `net_proto.h/.c`: cấu trúc `nrf_pkt_t` (header 4 byte + payload ≤ 28 byte), các loại tin, `seq`.
- [ ] Payload cho từng loại tin: `MSG_SET_TARGET` (`on_time_100ms`, `flags`; **không có `brightness`**), `MSG_EVT_HIT`, `MSG_STATUS`.
- [ ] Địa chỉ riêng 5 byte cho từng slave.
- [ ] `Net_Init(role, node_id)`, `Net_Task()`.
- [ ] Master: `Net_SetTarget()`, `Net_Stop()`, `Net_IsOnline()`, callback `Net_OnHit()`.
- [ ] Vòng hỏi master: id 1 đến 6, gửi, chờ ACK, đọc ACK payload, xử lý `MAX_RT`.
- [ ] Đánh dấu offline sau 3 lần liên tiếp không phản hồi; tự phục hồi khi slave quay lại.
- [ ] Chống lệnh trùng bằng `seq` (gửi lại thì giữ nguyên `seq`).
- [ ] Lệnh debug: `net status`.

**Tiêu chí hoàn thành:** master với 1 slave chạy ổn định, sau đó mở rộng dần lên 6; rút nguồn một slave thì master báo offline trong vài giây và không ảnh hưởng các slave còn lại.

---

## Giai đoạn 8 – Firmware slave

**Mục tiêu:** slave nhận lệnh, điều khiển LED vị trí và báo chạm.

**Việc cần làm**
- [ ] Tách phần dùng chung (driver nRF24, gói tin) cho cả master và slave.
- [ ] Slave nghe trên pipe riêng, nạp ACK payload bằng `Nrf_WriteAckPayload()` **trước** lượt hỏi kế tiếp.
- [ ] Xử lý `MSG_SET_TARGET` (bật LED, thời gian sáng, còi), `MSG_STOP`, `MSG_CONFIG`.
- [ ] Slave **tự đo thời gian phản xạ** (từ lúc LED sáng đến lúc chạm) rồi gửi `MSG_EVT_HIT`.
- [ ] Gửi định kỳ `MSG_STATUS` (trạng thái, điện áp pin, phiên bản firmware).
- [ ] Mất liên lạc với master quá lâu: tự tắt LED, về trạng thái chờ an toàn.

**Tiêu chí hoàn thành:** master ra lệnh, slave sáng đúng lúc, chạm thì master nhận được thời gian phản xạ đúng (đối chiếu bằng thiết bị đo ngoài).

---

## Giai đoạn 9 – Tích hợp và kiểm thử

**Mục tiêu:** hệ thống hoàn chỉnh chạy ổn định lâu dài.

**Việc cần làm**
- [ ] Gắn mạng vào máy trạng thái `app.c`: thay việc bật LED cục bộ bằng `Net_SetTarget()` tới slave tương ứng.
- [ ] Kiểm thử chức năng toàn bộ kịch bản: cài đặt, chạy, dừng, hết giờ.
- [ ] Kiểm thử chịu tải: chạy liên tục nhiều giờ, theo dõi bộ đếm lỗi, mất gói, tràn bộ đệm UART (`Uart_RxOverflowTaken`).
- [ ] Kiểm thử môi trường: nhiễu Wi-Fi 2,4 GHz, khoảng cách tối đa, vật cản; đổi kênh RF nếu cần.
- [ ] Kiểm thử lỗi: rút slave, tắt nguồn giữa chừng, nhấn nút liên tục, nhiễu nguồn.
- [ ] Đo thời gian một vòng hỏi 6 slave và độ trễ toàn hệ thống.
- [ ] Xem mức dùng flash, RAM, stack bằng map file; chừa lề an toàn.
- [ ] Bật `-Wall -Wextra`, xử lý toàn bộ warning.
- [ ] Xem xét watchdog (IWDG) để tự phục hồi khi treo.

**Tiêu chí hoàn thành:** chạy liên tục tối thiểu vài giờ không treo, không mất lệnh, tỷ lệ gói lỗi trong ngưỡng chấp nhận, mọi cảnh báo đã xử lý.

---

## Giai đoạn 10 – Hoàn thiện và phát hành

**Việc cần làm**
- [ ] Rà soát code theo `coding_style.md` (tên biến, hằng số, comment đơn vị, `static`, `volatile`).
- [ ] Dọn code chết (biến không dùng, hàm rỗng, khối comment cũ).
- [ ] Cập nhật `README.md` cho khớp `.ioc` và phần cứng thực tế (xóa các mục TBD).
- [ ] Viết hướng dẫn sử dụng: cài đặt, các lệnh debug, ý nghĩa tiếng bíp.
- [ ] Ghi chú phần cứng: sơ đồ đấu dây, nguồn, lưu ý điện áp.
- [ ] Gắn tag phiên bản Git (ví dụ `v1.0.0`), lưu file `.elf`/`.bin` kèm bản ghi thay đổi.

**Tiêu chí hoàn thành:** người khác clone repo, build và nạp được theo hướng dẫn mà không phải hỏi thêm.

---

## Rủi ro chính và cách giảm

| Rủi ro | Cách giảm |
|---|---|
| nRF24 chập chờn do nguồn yếu | Tụ lớn sát VCC, nguồn 3,3 V đủ dòng, dây SPI ngắn; kiểm tra sớm ở giai đoạn 6 |
| Mất dữ liệu UART do đọc chậm | Đọc đều đặn, tránh chặn vòng lặp chính, có phát hiện tràn |
| FIFO TX nRF bị kẹt sau `MAX_RT` | Luôn xóa cờ và `Nrf_FlushTx()` |
| Thiếu flash/RAM trên F030 | Theo dõi map file từng giai đoạn; cân nhắc LL cho phần lớn; không dùng `printf("%f")` |
| Mất code khi Generate Code | Chỉ viết trong `USER CODE`; commit trước mỗi lần generate |
| Xung đột chân (ví dụ PA1) | Chốt bảng chân ở giai đoạn 0 và đối chiếu lại `.ioc` |
| Nhiễu 2,4 GHz làm rớt gói | Chọn kênh xa Wi-Fi, giữ ACK/retransmit, đo thực tế ở giai đoạn 9 |
| Treo vòng chờ SPI/nRF | Mọi vòng chờ đều có timeout; thêm watchdog |

---

## Danh sách việc cần quyết định sớm

1. Mã chip F030 chính xác.
2. LED vị trí nối TM1637 hay nối GPIO (bật/tắt).
3. Chân của núm ADC (tránh trùng PA1).
4. Thư viện: HAL, LL hay trộn.
5. Slave có LED 7 đoạn và còi hay chỉ có LED và cảm biến chạm.
6. Dùng ngắt IRQ của nRF24 ngay hay bắt đầu bằng polling (khuyến nghị: polling trước).
7. Có cần watchdog và lưu cài đặt vào flash khi mất điện hay không.
