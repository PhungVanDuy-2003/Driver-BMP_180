#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdlib.h> // dùng system("clear")

#define BMP180_GET_TEMP_REAL      _IOR(0xB1, 2, int)
#define BMP180_GET_PRESSURE       _IOR(0xB1, 3, int)
#define BMP180_GET_ALTITUDE       _IOR(0xB1, 4, int)
#define BMP180_GET_FORECAST       _IOR(0xB1, 5, char[32])
#define BMP180_GET_PRESSURE_TREND _IOR(0xB1, 6, int[2])

void print_timestamp()
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("Thời gian: %02d/%02d/%04d %02d:%02d:%02d\n",
           t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
           t->tm_hour, t->tm_min, t->tm_sec);
}

void print_header()
{
    printf("=============================================\n");
    printf("Thiết bị: BMP180 - Bộ đo nhiệt độ và áp suất\n");
    printf("Phiên bản: 1.0 - Cập nhật mỗi 5 giây\n");
    printf("=============================================\n");
}

int main()
{
    int fd, value;
    char forecast[32];
    int pressure_trend[2];
    static int last_altitude = -1;
    int delta_alt = 0;

    // Mở thiết bị BMP180
    fd = open("/dev/bmp180", O_RDWR);
    if (fd < 0) {
        perror("Không thể mở /dev/bmp180");
        return 1;
    }

    while (1) {
        system("clear");        //  Xoá màn hình
        print_header();         // In thông tin thiết bị & phiên bản
        print_timestamp();      // In ngày giờ

        // Nhiệt độ
        if (ioctl(fd, BMP180_GET_TEMP_REAL, &value) == 0)
            printf("Nhiệt độ: %d.%d°C\n", value / 10, value % 10);
        else
            perror("Lỗi đo nhiệt độ");

        // Áp suất
        if (ioctl(fd, BMP180_GET_PRESSURE, &value) == 0)
            printf("Áp suất: %d Pa\n", value);
        else
            perror("Lỗi đo áp suất");

        // Độ cao và chênh lệch
        if (ioctl(fd, BMP180_GET_ALTITUDE, &value) == 0) {
            printf("Độ cao: %d m (So với mực nước biển: 101325 Pa)\n", value);
            //printf("(So với mực nước biển: 101325 Pa)\n");

            if (last_altitude != -1) {
                delta_alt = value - last_altitude;
                printf("Chênh lệch độ cao: %+d m so với lần trước\n", delta_alt);
            }
            last_altitude = value;
        } else {
            perror("Lỗi đo độ cao");
        }

        // Dự báo thời tiết
        if (ioctl(fd, BMP180_GET_FORECAST, &forecast) == 0)
            printf("Dự báo thời tiết: %s\n", forecast);
        else
            perror("Lỗi dự báo thời tiết");

        // Xu hướng áp suất
        if (ioctl(fd, BMP180_GET_PRESSURE_TREND, &pressure_trend) == 0)
            printf("Áp suất: Trước = %d Pa | Hiện tại = %d Pa\n",
                   pressure_trend[0], pressure_trend[1]);
        else
            perror("Lỗi theo dõi áp suất");

        // Tạm dừng 5 giây
        sleep(5);
    }

    close(fd);
    return 0;
}
