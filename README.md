# MODULE CẢM BIẾN ÁP SUẤT BMP180 - RASPBERRY PI3

GVHD: Bùi Hà Đức
Thành Viên Nhóm: 
Nguyễn Huỳnh Thắng_22146228
Nguyễn Thị Khánh Linh_22146166
Phùng Văn Duy_22146093
-

### I. Giới Thiệu
 Cảm biến BMP180 của Bosch là một trong những cảm biến đo áp suất và nhiệt độ nhỏ gọn, chính xác và được phát triển bởi Bosch Sensortec, một công ty con của Bosch Group. Bosch là một trong những tập đoàn công nghệ hàng đầu thế giới, nổi tiếng với việc phát triển các sản phẩm cảm biến và công nghệ tiên tiến cho các ứng dụng trong các lĩnh vực như tự động hóa, phương tiện giao thông, và điện tử tiêu dùng.

### II. Các Tính Năng Driver BMP180
Các tính năng chính của driver:
-Đọc nhiệt độ: Cung cấp dữ liệu nhiệt độ chính xác tính theo độ C từ cảm biến BMP180.
-Đọc áp suất: Đo áp suất không khí tại vị trí cảm biến, tính bằng đơn vị Pascal (Pa).
-Tính toán độ cao: Dựa trên áp suất đo được, driver có thể tính toán độ cao so với mực nước biển, cung cấp thông tin độ cao tương đối cho các ứng dụng đòi hỏi đo lường trong không gian 3D.
-Hỗ trợ giao tiếp qua ioctl: Driver cung cấp các hàm ioctl đơn giản để giao tiếp với user-space, cho phép người dùng dễ dàng tích hợp vào các ứng dụng và phần mềm của mình.

### III. Các Bước Cấu Hình Và Chạy Dự Án
#### Bước 1: Kích hoạt I2C trên Raspberry Pi
1. Mở terminal và chạy lệnh:
```c
sudo raspi-config
```
2. Vào mục:
```c
Interface Options → I2C → Enable
```
3. Reboot Raspberry Pi của bạn sau khi kích hoạt I2C thành công:
```c
sudo reboot
```
#### Bước 2: Xây dựng Driver Kernel
1. Trong thư mục dự án, chạy lệnh:
```c
make
```
2. Kiểm tra xem thiết bị có ở địa chỉ 0x77 (ví dụ: BMP280) hay không bang lệnh:
```c
dmesg | grep -i 'i2c\|bmp'
```

 Nếu có kết quả trả về giống bên dưới:
[    8.589546] bmp280 1-0077: supply vddd not found, using dummy regulator
[    8.589856] bmp280 1-0077: supply vdda not found, using dummy regulator
[    8.685570] bmp280: probe of 1-0077 failed with error -121
**Lưu ý**: *Driver BMP280 có sẵn trong hệ điều hành của Raspberry Pi và đang chiếm địa chỉ của cảm biến. Bạn cần unbind để gỡ driver BMP280, rồi bind lại driver BMP180 cho địa chỉ 0x77 bằng lệnh sau:*
```c
echo 1-0077 | sudo tee /sys/bus/i2c/drivers/bmp280/unbind
```
3. Tải module (driver BMP180):
```c
sudo insmod ./bmp180_ioctl.ko
```
4. Kiểm tra xem module BMP180 có được nạp đúng cách không:
```c
lsmod | grep bmp180
```
5. Kiểm tra xem thiết bị I2C có được nhận diện đúng không:
```c
sudo i2cdetect -y 1
```
4. Kiểm tra log kernel:
```c
dmesg | grep bmp180
```
#### Bước 3: Biên dịch ứng dụng User-Space
1. Biên dịch ứng dụng user-space từ mã nguồn C:
```c
gcc bmp180_user.c -o run
```
#### Bước 4: Cấu Hình Device Tree Overlay Cho BMP180
1. Tạo file bmp180_overlay.dts
```c
nano bmp180_overlay.dts
```
2. Dán nội dung sau vào file bmp180_overlay.dts
```c
/dts-v1/;
/plugin/;

&{/} {
    fragment@0 {
        target = <&i2c1>;
        __overlay__ {
            bmp180@77 {
                compatible = "bosch,bmp180";
                reg = <0x77>;
                status = "okay";
            };
        };
    };
};
```
3. Biên dịch thành file .dtbo
```c
sudo dtc -@ -I dts -O dtb bmp180_overlay.dts -o bmp180_overlay.dtbo
```
4. Di chuyển .dtbo vào thư mục overlays
```c
sudo cp bmp180_overlay.dtbo /boot/overlays/
```
5. Chỉnh sửa tệp cấu hình boot:
```c
sudo nano /boot/config.txt
```
6. Thêm dòng sau vào cuối tệp:
```c
dtoverlay=bmp180_overlay
```
7. Khởi động lại thiết bị:
```c
sudo reboot
```
#### Bước 5: Chạy chương trình trên User-Space
1. Gỡ driver BMP280 khỏi thiết bị I2C tại địa chỉ 0x77:
```c
echo 1-0077 | sudo tee /sys/bus/i2c/drivers/bmp280/unbind
```   
2. Nạp driver BMP180 vào kernel:
```c
sudo insmod ./bmp180_ioctl.ko
```
3. Chạy chương trình
```c
sudo ./run
```
4. Kết quả khi chạy thử nghiệm:
```c
Temperature: 27.2 °C
Pressure: 100437 Pa
```
Dọn dẹp:
```c
make clean
```
Gỡ bỏ Module Driver:
```c
sudo rmmod bmp180_ioctl
```
#### Chú ý 
1. Kết nối phần cứng: Đảm bảo BMP180 kết nối với SDA1 và SCL1 (I2C1) trên Raspberry Pi, cung cấp 3.3V (không dùng 5V).

2. Địa chỉ I2C: BMP180 sử dụng địa chỉ 0x77. Kiểm tra không có thiết bị khác chiếm cùng địa chỉ (gỡ driver BMP280 nếu có).

3. Gỡ bỏ BMP280: Nếu BMP280 đang sử dụng 0x77, gỡ bỏ driver với:
```c
echo 1-0077 | sudo tee /sys/bus/i2c/drivers/bmp280/unbind
```
Dọn dẹp và gỡ bỏ driver: Dọn dẹp với make clean và gỡ bỏ driver với sudo rmmod bmp180_driver.
```c
make clean
sudo rmmod bmp180_driver
```
Công cụ cần thiết: Cài đặt make, gcc, và i2c-tools nếu chưa có:
```c
sudo apt-get install build-essential i2c-tools
```






