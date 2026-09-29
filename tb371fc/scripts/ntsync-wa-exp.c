/* ntsync-wa-exp.c — WAIT_ALL isolation experiment */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>

struct ntsync_sem_args   { __u32 count; __u32 max; };
struct ntsync_event_args { __u32 manual; __u32 signaled; };
struct ntsync_wait_args  {
	__u64 timeout; __u64 objs; __u32 count; __u32 index;
	__u32 flags; __u32 owner; __u32 alert; __u32 pad;
};
#define NT_IOC(dir, nr, size) \
	((((dir) & 3u) << 30) | (((size) & 0x3fffu) << 16) | (('N' & 0xffu) << 8) | (nr))
#define NTSYNC_IOC_CREATE_SEM   NT_IOC(1, 0x80, sizeof(struct ntsync_sem_args))
#define NTSYNC_IOC_WAIT_ANY     NT_IOC(3, 0x82, sizeof(struct ntsync_wait_args))
#define NTSYNC_IOC_WAIT_ALL     NT_IOC(3, 0x83, sizeof(struct ntsync_wait_args))
#define NTSYNC_IOC_CREATE_EVENT NT_IOC(1, 0x87, sizeof(struct ntsync_event_args))

static int run(int all, __u64 timeout_ns, int sem_count)
{
	struct ntsync_sem_args sa = { .count = sem_count, .max = 1 };
	struct ntsync_event_args ea = { .manual = 1, .signaled = 1 };
	struct ntsync_wait_args wa;
	__u32 fds[2];
	int dev, sem_fd, evt_fd, ret;

	dev = open("/dev/ntsync", O_RDWR | O_CLOEXEC);
	if (dev < 0) { printf("open failed errno=%d\n", errno); return 1; }
	sem_fd = ioctl(dev, NTSYNC_IOC_CREATE_SEM, &sa);
	evt_fd = ioctl(dev, NTSYNC_IOC_CREATE_EVENT, &ea);
	if (sem_fd < 0 || evt_fd < 0) { printf("create failed\n"); return 1; }

	fds[0] = (__u32)sem_fd; fds[1] = (__u32)evt_fd;
	memset(&wa, 0, sizeof(wa));
	wa.timeout = timeout_ns;
	wa.objs = (__u64)(uintptr_t)fds;
	wa.count = 2;
	wa.owner = 0x1234;
	ret = (int)ioctl(dev, all ? NTSYNC_IOC_WAIT_ALL : NTSYNC_IOC_WAIT_ANY, &wa);
	printf("%s sem=%d timeout=%lluns -> ret=%d index=%u errno=%d\n",
	       all ? "WAIT_ALL" : "WAIT_ANY", sem_count,
	       (unsigned long long)timeout_ns, ret, wa.index, ret ? errno : 0);
	close(sem_fd); close(evt_fd); close(dev);
	return 0;
}

int main(int argc, char **argv)
{
	int all = (argc > 1) ? atoi(argv[1]) : 1;
	__u64 t = (argc > 2) ? strtoull(argv[2], 0, 0) : 100000000ULL;
	int sc = (argc > 3) ? atoi(argv[3]) : 1;
	return run(all, t, sc);
}
