/* ntsync-test.c — TASK-044 device acceptance for /dev/ntsync (4.19 backport)
 * Standalone (no kselftest harness / pthread); UAPI inlined to avoid header games.
 * Covers: sem release/wait/consume, mutex recursive acquire/unlock/ownership,
 *         event manual/auto reset-set, WAIT_ANY/WAIT_ALL, timeouts, READ ioctls.
 * Build (WSL): aarch64-linux-android24-clang -static -O2 -o ntsync-test ntsync-test.c
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>

struct ntsync_sem_args   { __u32 count; __u32 max; };
struct ntsync_mutex_args { __u32 owner; __u32 count; };
struct ntsync_event_args { __u32 manual; __u32 signaled; };
struct ntsync_wait_args  {
	__u64 timeout; __u64 objs; __u32 count; __u32 index;
	__u32 flags; __u32 owner; __u32 alert; __u32 pad;
};

/* _IOC-style encode, independent of libc headers: dir 1=W,2=R,3=WR */
#define NT_IOC(dir, nr, size) \
	((((dir) & 3u) << 30) | (((size) & 0x3fffu) << 16) | (('N' & 0xffu) << 8) | (nr))

#define NTSYNC_IOC_CREATE_SEM     NT_IOC(1, 0x80, sizeof(struct ntsync_sem_args))
#define NTSYNC_IOC_WAIT_ANY       NT_IOC(3, 0x82, sizeof(struct ntsync_wait_args))
#define NTSYNC_IOC_WAIT_ALL       NT_IOC(3, 0x83, sizeof(struct ntsync_wait_args))
#define NTSYNC_IOC_CREATE_MUTEX   NT_IOC(1, 0x84, sizeof(struct ntsync_mutex_args))
#define NTSYNC_IOC_MUTEX_UNLOCK   NT_IOC(3, 0x85, sizeof(struct ntsync_mutex_args))
#define NTSYNC_IOC_CREATE_EVENT   NT_IOC(1, 0x87, sizeof(struct ntsync_event_args))
#define NTSYNC_IOC_SEM_RELEASE    NT_IOC(3, 0x81, sizeof(__u32))
#define NTSYNC_IOC_EVENT_SET      NT_IOC(2, 0x88, sizeof(__u32))
#define NTSYNC_IOC_EVENT_RESET    NT_IOC(2, 0x89, sizeof(__u32))
#define NTSYNC_IOC_SEM_READ       NT_IOC(2, 0x8b, sizeof(struct ntsync_sem_args))
#define NTSYNC_IOC_MUTEX_READ     NT_IOC(2, 0x8c, sizeof(struct ntsync_mutex_args))
#define NTSYNC_IOC_EVENT_READ     NT_IOC(2, 0x8d, sizeof(struct ntsync_event_args))

#define WAIT_INFINITE 0xffffffffffffffffULL

static int dev = -1;
static int npass, nfail;

#define CHECK(cond, name) do { \
	if (cond) { npass++; printf("PASS %s\n", name); } \
	else { nfail++; printf("FAIL %s (errno=%d %s)\n", name, errno, strerror(errno)); } \
} while (0)

/* bionic ioctl returns -1 with errno on failure (not negative errno) */
#define EXPECT_ETIMEDOUT(name) do { \
	if (ret == -1 && errno == ETIMEDOUT) { npass++; printf("PASS %s\n", name); } \
	else { nfail++; printf("FAIL %s (ret=%d errno=%d %s)\n", name, ret, errno, strerror(errno)); } \
} while (0)

/* wait on ntsync object fd(s), nonblocking; returns ioctl ret, index in *idx */
static int wait_objs(__u32 *fds, __u32 count, int all, __u32 owner, __u32 *idx)
{
	struct ntsync_wait_args wa = {
		.timeout = 0, .objs = (__u64)(uintptr_t)fds,
		.count = count, .owner = owner, .alert = 0, .flags = 0,
	};
	long ret = ioctl(dev, all ? NTSYNC_IOC_WAIT_ALL : NTSYNC_IOC_WAIT_ANY, &wa);
	*idx = wa.index;
	return (int)ret;
}

int main(void)
{
	struct ntsync_sem_args sa;
	struct ntsync_mutex_args ma;
	struct ntsync_event_args ea;
	__u32 fds[2], idx = 0xdeadbeef;
	__u32 one = 1;
	int sem_fd, mtx_fd, evt_fd, evt2_fd;
	int ret;
	__u32 owner_me = 0x1234, owner_other = 0x9999;

	dev = open("/dev/ntsync", O_RDWR | O_CLOEXEC);
	CHECK(dev >= 0, "open /dev/ntsync");
	if (dev < 0)
		return 1;

	/* --- semaphore --- */
	memset(&sa, 0, sizeof(sa));
	sa.max = 1;
	ret = ioctl(dev, NTSYNC_IOC_CREATE_SEM, &sa);
	CHECK(ret >= 0, "create sem(count=0,max=1)");
	sem_fd = ret;

	ret = wait_objs(( __u32[]){ (__u32)sem_fd }, 1, 0, owner_me, &idx);
	EXPECT_ETIMEDOUT("wait empty sem -> ETIMEDOUT");

	ret = ioctl(sem_fd, NTSYNC_IOC_SEM_RELEASE, &one);
	CHECK(ret == 0, "sem release +1 (prev=0)");

	memset(&sa, 0, sizeof(sa));
	ret = ioctl(sem_fd, NTSYNC_IOC_SEM_READ, &sa);
	CHECK(ret == 0 && sa.count == 1, "sem read count==1");

	ret = wait_objs((__u32[]){ (__u32)sem_fd }, 1, 0, owner_me, &idx);
	CHECK(ret == 0 && idx == 0, "wait full sem -> 0, index=0");

	memset(&sa, 0, sizeof(sa));
	ret = ioctl(sem_fd, NTSYNC_IOC_SEM_READ, &sa);
	CHECK(ret == 0 && sa.count == 0, "sem consumed back to 0");

	ret = wait_objs((__u32[]){ (__u32)sem_fd }, 1, 0, owner_me, &idx);
	EXPECT_ETIMEDOUT("wait consumed sem -> ETIMEDOUT");

	/* --- mutex --- */
	memset(&ma, 0, sizeof(ma));
	ma.owner = owner_me; ma.count = 1;
	ret = ioctl(dev, NTSYNC_IOC_CREATE_MUTEX, &ma);
	CHECK(ret >= 0, "create mutex(owner=me,count=1)");
	mtx_fd = ret;

	ret = wait_objs((__u32[]){ (__u32)mtx_fd }, 1, 0, owner_me, &idx);
	CHECK(ret == 0, "recursive acquire by owner");

	memset(&ma, 0, sizeof(ma));
	ret = ioctl(mtx_fd, NTSYNC_IOC_MUTEX_READ, &ma);
	CHECK(ret == 0 && ma.owner == owner_me && ma.count == 2, "mutex read owner/count==me/2");

	ret = ioctl(mtx_fd, NTSYNC_IOC_MUTEX_UNLOCK, &ma);
	CHECK(ret == 0, "mutex unlock 1");
	ret = ioctl(mtx_fd, NTSYNC_IOC_MUTEX_UNLOCK, &ma);
	CHECK(ret == 0, "mutex unlock 2 (fully released)");

	memset(&ma, 0, sizeof(ma));
	ma.owner = owner_other; ma.count = 1;
	ret = ioctl(dev, NTSYNC_IOC_CREATE_MUTEX, &ma);
	CHECK(ret >= 0, "create mutex(other)");
	ret = ioctl(mtx_fd, NTSYNC_IOC_MUTEX_UNLOCK, &ma);   /* wrong owner -> EINVAL */
	CHECK(ret < 0, "unlock by wrong owner rejected");

	/* --- event --- */
	memset(&ea, 0, sizeof(ea));
	ea.manual = 1; ea.signaled = 1;
	ret = ioctl(dev, NTSYNC_IOC_CREATE_EVENT, &ea);
	CHECK(ret >= 0, "create event(manual,signaled)");
	evt_fd = ret;

	ret = wait_objs((__u32[]){ (__u32)evt_fd }, 1, 0, owner_me, &idx);
	CHECK(ret == 0, "wait signaled manual event");
	ret = wait_objs((__u32[]){ (__u32)evt_fd }, 1, 0, owner_me, &idx);
	CHECK(ret == 0, "manual event stays signaled");

	ret = ioctl(evt_fd, NTSYNC_IOC_EVENT_RESET, &one);
	CHECK(ret == 0, "event reset (prev=1)");
	ret = wait_objs((__u32[]){ (__u32)evt_fd }, 1, 0, owner_me, &idx);
	EXPECT_ETIMEDOUT("wait reset event -> ETIMEDOUT");
	ret = ioctl(evt_fd, NTSYNC_IOC_EVENT_SET, &one);
	CHECK(ret == 0, "event set (prev=0)");

	memset(&ea, 0, sizeof(ea));
	ea.manual = 0; ea.signaled = 0;
	ret = ioctl(dev, NTSYNC_IOC_CREATE_EVENT, &ea);
	CHECK(ret >= 0, "create event(auto,nonsignaled)");
	evt2_fd = ret;
	ret = ioctl(evt2_fd, NTSYNC_IOC_EVENT_SET, &one);
	CHECK(ret == 0, "auto event set");
	ret = wait_objs((__u32[]){ (__u32)evt2_fd }, 1, 0, owner_me, &idx);
	CHECK(ret == 0, "wait auto event -> 0");
	ret = wait_objs((__u32[]){ (__u32)evt2_fd }, 1, 0, owner_me, &idx);
	EXPECT_ETIMEDOUT("auto-reset consumed -> ETIMEDOUT");

	/* --- WAIT_ALL --- */
	fds[0] = (__u32)sem_fd; fds[1] = (__u32)evt_fd;
	ret = wait_objs(fds, 2, 1, owner_me, &idx);
	/* sem is empty here -> expect ETIMEDOUT */
	EXPECT_ETIMEDOUT("WAIT_ALL (sem empty) -> ETIMEDOUT");
	one = 1;	/* SEM_RELEASE reads the delta from *arg; earlier ioctls clobbered it */
	ret = ioctl(sem_fd, NTSYNC_IOC_SEM_RELEASE, &one);
	CHECK(ret == 0, "sem refill");
	ret = wait_objs(fds, 2, 1, owner_me, &idx);
	CHECK(ret == 0, "WAIT_ALL both signaled -> 0");

	/* --- invalid owner rejection (upstream hardening) --- */
	{
		struct ntsync_wait_args wa;
		memset(&wa, 0, sizeof(wa));
		wa.objs = (__u64)(uintptr_t)fds; wa.count = 1;
		ret = (int)ioctl(dev, NTSYNC_IOC_WAIT_ANY, &wa);
		CHECK(ret == -1 && errno == EINVAL, "wait with zero owner -> EINVAL");
	}

	printf("\n=== ntsync-test: %d passed, %d failed ===\n", npass, nfail);
	close(sem_fd); close(mtx_fd); close(evt_fd); close(evt2_fd); close(dev);
	return nfail ? 1 : 0;
}
