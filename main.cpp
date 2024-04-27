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

int main() {
	int fd = open("/dev/video0", O_RDWR);
	if (fd < 0) {
		printf("FERRL: %s", strerror(errno));
	}
	v4l2_input *inp = (v4l2_input*)malloc(sizeof(v4l2_input));
	inp->index = 0;
	int err = ioctl(fd, VIDIOC_ENUMINPUT, inp); 
	if (err == 0) {
		printf("Type %d", inp->type);
	}
	else {
		printf("ERR: %s", strerror(errno));
	}
	close(fd);
	return 0;
}
