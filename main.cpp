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

void print_formats(int fd);

int main() {
	int fd = open("/dev/video0", O_RDWR);
	if (fd < 0) {
		printf("FERRL: %s\n", strerror(errno));
	}
	v4l2_input *inp = (v4l2_input*)malloc(sizeof(v4l2_input));
	inp->index = 0;
	int err = ioctl(fd, VIDIOC_ENUMINPUT, inp); 
	if (err == 0) {
		printf("Type %d\n", inp->type);
	}
	else {
		printf("ERR: %s\n", strerror(errno));
	}

	print_formats(fd);

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
}
