#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>

#define ADXL345_MAGIC 'a'
#define ADXL345_SET_AXIS _IOW(ADXL345_MAGIC, 1, int)
#define ADXL345_GET_AXIS _IOR(ADXL345_MAGIC, 2, int)

int main() {
    int fd = open("/dev/adxl345", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    printf("Device opened successfully\n");

    // Set axis to Y (1)
    int axis = 0;
    if (ioctl(fd, ADXL345_SET_AXIS, &axis) < 0) {
        perror("Failed to set axis");
        close(fd);
        return -1;
    }
    printf("Axis set to %d\n", axis);

    // Get the current axis
    int current_axis;
    if (ioctl(fd, ADXL345_GET_AXIS, &current_axis) < 0) {
        perror("Failed to get axis");
        close(fd);
        return -1;
    }
    printf("Current axis: %d\n", current_axis);

    // Read acceleration data
    int16_t accel;
    if (read(fd, &accel, sizeof(accel)) != sizeof(accel)) {
        perror("Failed to read acceleration data");
        close(fd);
        return -1;
    }

    printf("Acceleration: %d\n", accel);

    close(fd);
    return 0;
}
