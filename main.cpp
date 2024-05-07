#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/i2c-dev.h>
#include <libv4l2.h>
#include <errno.h>
#include <cassert>

#include "gpio.h"
#define DEBUG
#define check_err(err) if (err < 0) printf("LINE %d: Error: %s\n", __LINE__, strerror(errno))
#define return_on_err(err) if (err < 0) exit(EXIT_FAILURE)  //TODO: make this less intense
#define read_camera_reg(addr) read_camera_reg_fd(i2c_fd, addr)
#ifdef DEBUG
#define debug_print(f,...) printf(f, __VA_ARGS__)
#else
#define debug_print(...)
#endif

#define PIN_SEL 4 //BCM Pin 7
#define PIN_OE 17 //BCM Pin 11
#define ADDR_CAM 0x36
#define ADDR_MUX 0x70


const int MIN_BUF_COUNT = 3;

void print_formats(int fd);
int set_image_fmt(int fd);
uint8_t read_camera_reg_fd(int fd, uint16_t addr);
int gpio_setup(void);
void enable_cam_a(void);
int i2c_sel_cam_a(int fd);
void enable_cam_b(void);
int i2c_sel_cam_b(int fd);
void disable_cams(void);

typedef struct {
    void *start;
    size_t length;
} buffer;

int main() {
    // open i2c fd
    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    int err = gpio_setup();
    check_err(err);
    enable_cam_a();
    i2c_sel_cam_a(i2c_fd);

    // open camera fd
	int fd = open("/dev/video0", O_RDWR);
	if (fd < 0) {
		printf("FERRL: %s\n", strerror(errno));
	}
	//print_formats(fd);

    buffer *buffers = (buffer*)calloc(6, sizeof(buffer));
    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (int cam = 0; cam < 2; ++cam) {
        if (cam % 2 == 0) {
            enable_cam_a();
            i2c_sel_cam_a(i2c_fd);
        }
        else {
            enable_cam_b();
            i2c_sel_cam_b(i2c_fd);
        }
        sleep(1);
        set_image_fmt(fd);
    }
    for (int cam = 0; cam < 2; ++cam) {
        if (cam % 2 == 0) {
            enable_cam_a();
            i2c_sel_cam_a(i2c_fd);
        }
        else {
            enable_cam_b();
            i2c_sel_cam_b(i2c_fd);
        }
        sleep(1);
        // check capabilities for CAP_STREAMING
        v4l2_capability cap;
        memset(&cap, 0, sizeof(cap));
        err = ioctl(fd, VIDIOC_QUERYCAP, &cap);
        check_err(err);
        return_on_err(err);
        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
            printf("No Streaming Capabilities!\n");
            exit(EXIT_FAILURE);
        }

        // request buffers
        v4l2_requestbuffers req_buf;
        memset(&req_buf, 0, sizeof(req_buf));
        req_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req_buf.memory = V4L2_MEMORY_MMAP;
        req_buf.count = MIN_BUF_COUNT;
        err = ioctl(fd, VIDIOC_REQBUFS, &req_buf);
        check_err(err);
        return_on_err(err);

        // check buffers
        if (3 != req_buf.count) {
            printf("Invalid buffer count. Found %d\n", req_buf.count);
            exit(EXIT_FAILURE);
        }
    }
    for (int cam = 0; cam < 2; ++cam) {
        if (cam % 2 == 0) {
            enable_cam_a();
            i2c_sel_cam_a(i2c_fd);
        }
        else {
            enable_cam_b();
            i2c_sel_cam_b(i2c_fd);
        }
        sleep(1);

        // store an array of buffer pointers and sizes to munmap
        assert(buffers != NULL);
        int size = 0;
        for (unsigned int i = 0; i < 3; i++) {
            v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.index = i;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            err = ioctl(fd, VIDIOC_QUERYBUF, &buf);
            check_err(err);
            return_on_err(err);
            size = buf.bytesused;

            // map buffer
            void* mm_buf = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
            if (MAP_FAILED == mm_buf) {
                printf("Map Failed\n");
                exit(EXIT_FAILURE);
            }

            buffers[i+cam*3].start = mm_buf;
            buffers[i+cam*3].length = buf.length;
        }
    }
    for (int cam = 0; cam < 2; ++cam) {
        if (cam % 2 == 0) {
            enable_cam_a();
            i2c_sel_cam_a(i2c_fd);
        }
        else {
            enable_cam_b();
            i2c_sel_cam_b(i2c_fd);
        }
        sleep(1);

        printf("Cam%d", cam);

    }

    int file = open("output.yuy", O_RDWR | O_CREAT);
    for (int i = 0; i < 20; ++i) { 
        int buffer_idx;
        if (i % 2 == 0) {
            enable_cam_a();
            i2c_sel_cam_a(i2c_fd);
            buffer_idx = 0;
        }
        else {
            enable_cam_b();
            i2c_sel_cam_b(i2c_fd);
            buffer_idx = 3;
        }
        usleep(1000);

        // queue buffer
        v4l2_buffer bufd_2 = {0};
        bufd_2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufd_2.memory = V4L2_MEMORY_MMAP;
        bufd_2.index = buffer_idx % 3;

        err = ioctl(fd, VIDIOC_QBUF, &bufd_2);
        check_err(err);
        return_on_err(err);

        err = ioctl(fd, VIDIOC_STREAMON, &type);
        check_err(err);
        return_on_err(err);

        //printf("Exposure: %x%x, AGC: %x%x\n", read_camera_reg(0x3501), read_camera_reg(0x3502), read_camera_reg(0x350A), read_camera_reg(0x350B));

        // wait for data
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0};
        tv.tv_sec = 2;
        err = select(fd+1, &fds, NULL, NULL, &tv);
        check_err(err);
        return_on_err(err);
        
        // deque buffer
        v4l2_buffer bufd_1 = {0};
        bufd_1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufd_1.memory = V4L2_MEMORY_MMAP;
        bufd_1.index = 0;
        
        err = ioctl(fd, VIDIOC_DQBUF, &bufd_1);
        check_err(err);
        return_on_err(err);


        write(file, buffers[buffer_idx].start, buffers[buffer_idx].length);

        err = ioctl(fd, VIDIOC_STREAMOFF, &type);
        check_err(err);
        return_on_err(err);
    }

    // stop streaming
    err = ioctl(fd, VIDIOC_STREAMOFF, &type);
    check_err(err);
    return_on_err(err);

    // write to file

    // un-map buffers
    for (unsigned int i = 0; i < 6; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }
    free(buffers);

	close(fd);
	return 0;
}

void print_formats(int fd) {
    int index = 0;
    v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    while (true) {
        // set the required field in the format structure 
        fmt.type = V4L2_CAP_VIDEO_CAPTURE;
        fmt.mbus_code = 0;
        fmt.index = index;
        index++;

        // call format enumeration, exit when EINVAL (expected) or other error
        int err = ioctl(fd, VIDIOC_ENUM_FMT, &fmt);
        if (err < 0 && EINVAL == errno)  {
            printf("Finished Enumerating.\n");
            break;
        }
        else if (err < 0) {
            printf("Error printing formats: %s", strerror(errno));
            break;
        }

        // print the format type
        printf("\tFormat: %s, \t\t\t\tCode %c%c%c%c.\n", fmt.description, 
        (fmt.pixelformat & 0xFF'00'00'00) >> 24,
        (fmt.pixelformat & 0x00'FF'00'00) >> 16,
        (fmt.pixelformat & 0x00'00'FF'00) >> 8,
        (fmt.pixelformat & 0x00'00'00'FF));	
    }
}

int set_image_fmt(int fd) {
    // using single planar YUYV 4:2:2
    v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int err = ioctl(fd, VIDIOC_G_FMT, &fmt);
    check_err(err);
    return_on_err(err);

    // change to desired format
    if (V4L2_BUF_TYPE_VIDEO_CAPTURE == fmt.type) {
        fmt.fmt.pix.width = 640;
        fmt.fmt.pix.height = 480;
        fmt.fmt.pix.pixelformat = v4l2_fourcc('G','B','1','0');
        fmt.fmt.pix.field = V4L2_FIELD_NONE; //progressive (non-interlaced)

        // attempt to write format
        int err = ioctl(fd, VIDIOC_S_FMT, &fmt);
        check_err(err);
        return_on_err(err);
    }
    else {
        printf("Not VID_CAPTURE\n");
    }
    return 0;
}

uint8_t read_camera_reg_fd(int fd, uint16_t addr) {
    int err = ioctl(fd, I2C_SLAVE, ADDR_CAM);
    check_err(err);
    return_on_err(err);
    uint8_t buf[2] = {addr >> 8, addr & 0xff};
    err = write(fd, buf, 2);
    check_err(err);
    return_on_err(err);

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	struct timeval tv = {0};
	tv.tv_sec = 2;
	err = select(fd+1, &fds, NULL, NULL, &tv);
    check_err(err);
    return_on_err(err);

    read(fd, buf, 1);
    check_err(err);
    return_on_err(err);
    return buf[0];
}

int gpio_setup(void) {
    int err = gpioInitialise();
    if (0 != err) {
        return -1;
    }
    gpioSetMode(PIN_SEL, PI_OUTPUT);
    gpioSetMode(PIN_OE, PI_OUTPUT);
    sleep(1);
    if (gpioGetMode(PIN_SEL) != PI_OUTPUT && gpioGetMode(PIN_OE) != PI_OUTPUT) {
        perror("Failed to set GPIO to output");
        return -1;
    }
    return 0;
}

//UC-444 Specific methods
void enable_cam_a(void) {
    gpioWrite(PIN_SEL, 0);
    gpioWrite(PIN_OE, 0);
}

int i2c_sel_cam_a(int fd) {
    int err = ioctl(fd, I2C_SLAVE, ADDR_MUX);
    check_err(err);
    if (err < 0) return -1; 
    
    uint8_t data = 0x01;
    return write(fd, &data, 1);
}

void enable_cam_b(void) {
    gpioWrite(PIN_SEL, 1);
    gpioWrite(PIN_OE, 0);
}

int i2c_sel_cam_b(int fd) {
    int err = ioctl(fd, I2C_SLAVE, ADDR_MUX);
    check_err(err);
    if (err < 0) return -1; 
    
    uint8_t data = 0x02;
    return write(fd, &data, 1);
}

void disable_cams(void) {
    gpioWrite(PIN_OE, 1);
}
