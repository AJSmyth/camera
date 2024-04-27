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
#include <libv4l2.h>
#include <errno.h>
#include <cassert>

#define check_err(err) if (err < 0) printf("LINE %d: Error: %s\n", __LINE__, strerror(errno))
#define return_on_err(err) if (err < 0) exit(EXIT_FAILURE)


const int MIN_BUF_COUNT = 3;

void print_formats(int fd);
int set_image_fmt(int fd);

typedef struct {
    void *start;
    size_t length;
} buffer;

int main() {
    // open fd
	int fd = open("/dev/video0", O_RDWR);
	if (fd < 0) {
		printf("FERRL: %s\n", strerror(errno));
	}
	//print_formats(fd);
    set_image_fmt(fd);

    // check capabilities for CAP_STREAMING
    v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    int err = ioctl(fd, VIDIOC_QUERYCAP, &cap);
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

    // store an array of buffer pointers and sizes to munmap
    buffer *buffers = (buffer*)calloc(req_buf.count, sizeof(buffer));
    assert(buffers != NULL);

    for (unsigned int i = 0; i < req_buf.count; i++) {
        v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.index = i;
        err = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        check_err(err);
        return_on_err(err);

        // map buffer
        void* mm_buf = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (MAP_FAILED == mm_buf) {
            printf("Map Failed\n");
            exit(EXIT_FAILURE);
        }

        buffers[i].start = mm_buf;
        buffers[i].length = buf.length;
    }

    // un-map buffers
    for (unsigned int i = 0; i < req_buf.count; i++) {
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
        fmt.fmt.pix.pixelformat = v4l2_fourcc('V','Y','U','Y');
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


