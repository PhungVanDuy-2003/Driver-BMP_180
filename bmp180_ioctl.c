// === Trình điều khiển kernel cho cảm biến BMP180 ===
// Hỗ trợ: đo nhiệt độ, áp suất, độ cao, dự báo thời tiết cơ bản, theo dõi áp suất

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/delay.h>

// === Định nghĩa mã lệnh ioctl ===
#define BMP180_GET_TEMP_REAL      _IOR(0xB1, 2, int)       // Lấy nhiệt độ (0.1°C)
#define BMP180_GET_PRESSURE       _IOR(0xB1, 3, int)       // Lấy áp suất (Pa)
#define BMP180_GET_ALTITUDE       _IOR(0xB1, 4, int)       // Tính độ cao (m)
#define BMP180_GET_FORECAST       _IOR(0xB1, 5, char[32])  // Dự báo thời tiết đơn giản
#define BMP180_GET_PRESSURE_TREND _IOR(0xB1, 6, int[2])    // Theo dõi xu hướng áp suất

// === Các thanh ghi của BMP180 ===
#define BMP180_REG_CONTROL   0xF4
#define BMP180_REG_RESULT    0xF6
#define BMP180_CMD_TEMP      0x2E
#define BMP180_CMD_PRESSURE  0x34
#define BMP180_REG_CALIB     0xAA

static struct i2c_client *bmp180_client;

// === Các hằng hiệu chuẩn đọc từ EEPROM ===
static s16 ac1, ac2, ac3, b1, b2, mc, md;
static u16 ac4, ac5, ac6;
static int b5;
static int last_pressure = -1;  // Áp suất lần trước (dùng cho dự báo và theo dõi)

// Đọc nhiều byte từ cảm biến thông qua I2C
static int bmp180_read_bytes(u8 reg, u8 *buf, u8 len)
{
    struct i2c_msg msgs[2] = {
        { .addr = bmp180_client->addr, .flags = 0, .len = 1, .buf = &reg },
        { .addr = bmp180_client->addr, .flags = I2C_M_RD, .len = len, .buf = buf },
    };
    return i2c_transfer(bmp180_client->adapter, msgs, 2);
}

// Đọc dữ liệu hiệu chuẩn từ EEPROM của cảm biến
static int bmp180_read_calib_data(void)
{
    u8 buf[22];
    if (bmp180_read_bytes(BMP180_REG_CALIB, buf, 22) < 0)
        return -EIO;

    ac1 = (buf[0] << 8) | buf[1];
    ac2 = (buf[2] << 8) | buf[3];
    ac3 = (buf[4] << 8) | buf[5];
    ac4 = (buf[6] << 8) | buf[7];
    ac5 = (buf[8] << 8) | buf[9];
    ac6 = (buf[10] << 8) | buf[11];
    b1  = (buf[12] << 8) | buf[13];
    b2  = (buf[14] << 8) | buf[15];
    mc  = (buf[18] << 8) | buf[19];
    md  = (buf[20] << 8) | buf[21];

    printk(KERN_INFO "bmp180: Calibration data loaded.\n");
    return 0;
}

// Đo nhiệt độ (trả về đơn vị 0.1 độ C)
static int bmp180_get_temperature(void)
{
    u8 raw[2];
    int ut, x1, x2, t;

    i2c_smbus_write_byte_data(bmp180_client, BMP180_REG_CONTROL, BMP180_CMD_TEMP);
    msleep(5);

    bmp180_read_bytes(BMP180_REG_RESULT, raw, 2);
    ut = (raw[0] << 8) | raw[1];

    x1 = ((ut - ac6) * ac5) >> 15;
    x2 = (mc << 11) / (x1 + md);
    b5 = x1 + x2;
    t = (b5 + 8) >> 4;

    return t;
}

// Đo áp suất không khí (Pa)
static int bmp180_read_pressure(void)
{
    u8 raw[3];
    int up, x1, x2, x3, b3, b6, p;
    unsigned int b4, b7;

    i2c_smbus_write_byte_data(bmp180_client, BMP180_REG_CONTROL, BMP180_CMD_PRESSURE);
    msleep(8);

    bmp180_read_bytes(BMP180_REG_RESULT, raw, 3);
    up = ((raw[0] << 16) | (raw[1] << 8) | raw[2]) >> 8;

    b6 = b5 - 4000;
    x1 = (b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = (ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((int)ac1 * 4 + x3) + 2) >> 2;

    x1 = (ac3 * b6) >> 13;
    x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = (ac4 * (unsigned int)(x3 + 32768)) >> 15;
    b7 = ((unsigned int)up - b3) * 50000;

    if (b7 < 0x80000000)
        p = (b7 << 1) / b4;
    else
        p = (b7 / b4) << 1;

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p += (x1 + x2 + 3791) >> 4;

    return p;
}

// Tính độ cao tương đối dựa vào áp suất
static int bmp180_calculate_altitude(int pressure)
{
    return (101325 - pressure) / 100; // Đơn giản: 100 Pa ≈ 1 m
}

// Xử lý lệnh ioctl từ user-space
static long bmp180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int value;
    char forecast[32];
    int pressure_trend[2];

    switch (cmd) {
    case BMP180_GET_TEMP_REAL:
        value = bmp180_get_temperature();
        if (copy_to_user((int __user *)arg, &value, sizeof(int)))
            return -EFAULT;
        break;

    case BMP180_GET_PRESSURE:
        bmp180_get_temperature();
        value = bmp180_read_pressure();
        if (copy_to_user((int __user *)arg, &value, sizeof(int)))
            return -EFAULT;
        break;

    case BMP180_GET_ALTITUDE:
        bmp180_get_temperature();
        value = bmp180_read_pressure();
        value = bmp180_calculate_altitude(value);
        if (copy_to_user((int __user *)arg, &value, sizeof(int)))
            return -EFAULT;
        break;

    case BMP180_GET_FORECAST:
        bmp180_get_temperature();
        value = bmp180_read_pressure();

        if (last_pressure == -1) {
            snprintf(forecast, sizeof(forecast), "Không đủ dữ liệu");
        } else if (value > last_pressure + 30) {
            snprintf(forecast, sizeof(forecast), "Thời tiết cải thiện (nắng)");
        } else if (value < last_pressure - 30) {
            snprintf(forecast, sizeof(forecast), "Khả năng mưa/thời tiết xấu");
        } else {
            snprintf(forecast, sizeof(forecast), "Thời tiết ổn định");
        }

        last_pressure = value;
        if (copy_to_user((char __user *)arg, forecast, sizeof(forecast)))
            return -EFAULT;
        break;

    case BMP180_GET_PRESSURE_TREND:
        bmp180_get_temperature();
        value = bmp180_read_pressure();

        pressure_trend[0] = last_pressure;
        pressure_trend[1] = value;
        last_pressure = value;

        if (copy_to_user((int __user *)arg, pressure_trend, sizeof(pressure_trend)))
            return -EFAULT;
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

// Đăng ký file /dev/bmp180
static const struct file_operations bmp180_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = bmp180_ioctl,
};

// Thiết bị đặc biệt kiểu misc
static struct miscdevice bmp180_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "bmp180",
    .fops = &bmp180_fops,
};

// Gắn driver khi kernel phát hiện thiết bị I2C phù hợp
static int bmp180_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    bmp180_client = client;

    if (bmp180_read_calib_data() < 0)
        return -EIO;
    
    printk(KERN_INFO "Initializing BMP180 Driver\n");
    printk(KERN_INFO "BMP180 driver installed\n");
    return misc_register(&bmp180_misc);
}

// Gỡ thiết bị khi tháo driver
static void bmp180_remove(struct i2c_client *client)
{
    misc_deregister(&bmp180_misc);
    printk(KERN_INFO "Exiting BMP180 driver\n");
    printk(KERN_INFO "BMP180 driver removed\n");
}

// Danh sách thiết bị I2C hỗ trợ
static const struct i2c_device_id bmp180_id[] = {
    { "bmp180", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bmp180_id);

// Khai báo driver I2C
static struct i2c_driver bmp180_driver = {
    .driver = {
        .name = "bmp180",
    },
    .probe = bmp180_probe,
    .remove = bmp180_remove,
    .id_table = bmp180_id,
};

module_i2c_driver(bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("22146228_Nguyen Huynh Thang");
MODULE_DESCRIPTION("Trình điều khiển BMP180 với hỗ trợ nhiệt độ, áp suất, độ cao, dự báo thời tiết và giám sát môi trường");

