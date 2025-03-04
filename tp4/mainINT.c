#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define ADXL345_MAGIC 'a'
#define ADXL345_SET_AXIS _IOW(ADXL345_MAGIC, 1, int)
#define ADXL345_GET_AXIS _IOR(ADXL345_MAGIC, 2, int)

#define DEVICE_PATH "/dev/adxl345"

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} adxl345_sample_t;

int main() {
    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    printf("Device opened successfully\n");

    // Configure all 6 registers (X0, X1, Y0, Y1, Z0, Z1)
    for (int axis = 0; axis < 3; axis++) {
        if (ioctl(fd, ADXL345_SET_AXIS, &axis) < 0) {
            perror("Failed to set axis");
            close(fd);
            return -1;
        }
        printf("Axis %d configured\n", axis);
    }

    // Get the current axis setting (just for confirmation)
    int current_axis;
    if (ioctl(fd, ADXL345_GET_AXIS, &current_axis) < 0) {
        perror("Failed to get axis");
        close(fd);
        return -1;
    }
    printf("Current axis: %d\n", current_axis);

    // Read all 6 registers (X, Y, Z) in one go
    adxl345_sample_t sample;
    if (read(fd, &sample, sizeof(sample)) != sizeof(sample)) {
        perror("Failed to read acceleration data");
        close(fd);
        return -1;
    }

    // Print the acceleration values
    printf("Acceleration X: %d\n", sample.x);
    printf("Acceleration Y: %d\n", sample.y);
    printf("Acceleration Z: %d\n", sample.z);

    close(fd);
    return 0;
}
