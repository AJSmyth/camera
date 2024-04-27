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

#define check_err(err) if (err < 0) printf("LINE %d: Error: %s\n", __LINE__, strerror(errno))
#define return_on_err(err) if (err < 0) return err

void print_formats(int fd);
int set_image_fmt(int fd);

int main() {
    // open fd
	int fd = open("/dev/video0", O_RDWR);
	if (fd < 0) {
		printf("FERRL: %s\n", strerror(errno));
	}

	print_formats(fd);

    set_image_fmt(fd);

	close(fd);
	return 0;
}

void print_formats(int fd) {
    int index = 0;
    v4l2_fmtdesc *fmt = (v4l2_fmtdesc*) malloc(sizeof(v4l2_fmtdesc));
    while (true) {
        // set the required field in the format structure 
        fmt->type = V4L2_CAP_VIDEO_CAPTURE;
        fmt->mbus_code = 0;
        fmt->index = index;
        index++;

        // call format enumeration, exit when EINVAL (expected) or other error
        int err = ioctl(fd, VIDIOC_ENUM_FMT, fmt);
        if (err < 0 && errno == EINVAL)  {
            printf("Finished Enumerating.\n");
            break;
        }
        else if (err < 0) {
            printf("Error printing formats: %s", strerror(errno));
            break;
        }

        // print the format type
        printf("\tFormat: %s, \t\t\t\tCode %c%c%c%c.\n", fmt->description, 
        (fmt->pixelformat & 0xFF'00'00'00) >> 24,
        (fmt->pixelformat & 0x00'FF'00'00) >> 16,
        (fmt->pixelformat & 0x00'00'FF'00) >> 8,
        (fmt->pixelformat & 0x00'00'00'FF));	
    }
    free(fmt);
}

int set_image_fmt(int fd) {
    // using single planar YUYV 4:2:2
    v4l2_format *fmt = (v4l2_format*)malloc(sizeof(v4l2_format));
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int err = ioctl(fd, VIDIOC_G_FMT, fmt);
    check_err(err);
    return_on_err(err);

    // change to desired format
    if (fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        fmt->fmt.pix.width = 640;
        fmt->fmt.pix.height = 480;
        fmt->fmt.pix.pixelformat = v4l2_fourcc('V','Y','U','Y');
        fmt->fmt.pix.field = V4L2_FIELD_NONE; //progressive (non-interlaced)

        // attempt to write format
        int err = ioctl(fd, VIDIOC_S_FMT, fmt);
        check_err(err);
        return_on_err(err);
    }
    else {
        printf("Not VID_CAPTURE");
    }
    return 0;
}


