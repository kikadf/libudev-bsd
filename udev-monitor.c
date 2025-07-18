/*
 * Copyright (c) 2015, 2021 Vladimir Kondratyev <vladimir@kondratyev.su>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "udev-global.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/un.h>

#if defined(__OpenBSD__)
#include <sys/sysctl.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__NetBSD__)
#include <ndevd.h>
#define	DEVD_SOCK_PATH		NDEVD_SOCKET
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#define	DEVD_SOCK_PATH		"/var/run/devd.seqpacket.pipe"
#endif

#define	DEVD_RECONNECT_INTERVAL	1000	/* reconnect after 1 second */

STAILQ_HEAD(udev_monitor_queue_head, udev_monitor_queue_entry);
struct udev_monitor_queue_entry {
	struct udev_device *ud;
	STAILQ_ENTRY(udev_monitor_queue_entry) next;
};

struct udev_monitor {
	int refcount;
	int fds[2];
	struct udev_filter_head filters;
	struct udev *udev;
	struct udev_monitor_queue_head queue;
	pthread_mutex_t mtx;
	pthread_t thread;
#if defined(__OpenBSD__)
	struct udev_list cur_dev_list;
	struct udev_list prev_dev_list;
	int cur_serial;
	int prev_serial;
#endif
};

#if defined(__OpenBSD__)
int mib[] = { CTL_KERN, KERN_AUTOCONF_SERIAL };
extern pthread_mutex_t scan_mtx;
#endif

LIBUDEV_EXPORT struct udev_device *
udev_monitor_receive_device(struct udev_monitor *um)
{
	struct udev_monitor_queue_entry *umqe;
	struct udev_device *ud;
	char buf[1];

	TRC("(%p)", um);
	if (read(um->fds[0], buf, 1) < 0)
		return (NULL);

	if (STAILQ_EMPTY(&um->queue))
		return (NULL);

	pthread_mutex_lock(&um->mtx);
	umqe = STAILQ_FIRST(&um->queue);
	STAILQ_REMOVE_HEAD(&um->queue, next);
	pthread_mutex_unlock(&um->mtx);
	ud = umqe->ud;
	free(umqe);

	return (ud);
}

static int
udev_monitor_send_device(struct udev_monitor *um, const char *syspath,
    int action)
{
	struct udev_monitor_queue_entry *umqe;

	umqe = calloc(1, sizeof(struct udev_monitor_queue_entry));
	if (umqe == NULL)
		return (-1);

	umqe->ud = udev_device_new_common(um->udev, syspath, action);
	if (umqe->ud == NULL) {
		free(umqe);
		return (-1);
	}

	pthread_mutex_lock(&um->mtx);
	STAILQ_INSERT_TAIL(&um->queue, umqe, next);
	pthread_mutex_unlock(&um->mtx);

	if (write(um->fds[1], "*", 1) != 1) {
		pthread_mutex_lock(&um->mtx);
		STAILQ_REMOVE(&um->queue, umqe, udev_monitor_queue_entry, next);
		pthread_mutex_unlock(&um->mtx);
		udev_device_unref(umqe->ud);
		free(umqe);
		return (-1);
	}

	return (0);
}

#if !defined(__OpenBSD__)
#if defined(__NetBSD__)
static int
parse_ndevd_message(struct ndevd_msg msg, char *syspath, size_t syspathlen)
{
	int action;

	action = udev_dev_monitor(msg, syspath, syspathlen);

	return (action);
}
#else
static int
parse_devd_message(char *msg, char *syspath, size_t syspathlen)
{
	int action;

	action = udev_dev_monitor(msg, syspath, syspathlen);
	if (action == UD_ACTION_NONE)
		action = udev_sys_monitor(msg, syspath, syspathlen);
	if (action == UD_ACTION_NONE)
		action = udev_pci_monitor(msg, syspath, syspathlen);
	if (action == UD_ACTION_NONE)
		action = udev_net_monitor(msg, syspath, syspathlen);

	return (action);
}
#endif

static void *
udev_monitor_thread(void *args)
{
	struct udev_monitor *um = args;
#if defined(__NetBSD__)
	struct ndevd_msg event;
	void *ev = &event;
	size_t ev_len = sizeof(event);
#else
	char ev[1024];
	size_t ev_len = sizeof(ev);
#endif
	char syspath[DEV_PATH_MAX];
	struct pollfd fds[2];
	nfds_t nfds;
	ssize_t len;
	int devd_fd = -1, ret, action, timeout;
	sigset_t set;
	const static struct sockaddr_un sa = {
		.sun_family = AF_UNIX,
		.sun_path = DEVD_SOCK_PATH,
	};

	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	fds[0].fd = um->fds[1];
	fds[0].events = 0;
	fds[1].events = POLLIN;

	for (;;) {
		if (devd_fd < 0 &&
		    (devd_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0)) >= 0 &&
		    connect(devd_fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
			close(devd_fd);
			devd_fd = -1;
		}

		if (devd_fd < 0) {
			nfds = 1;
			timeout = DEVD_RECONNECT_INTERVAL;
		} else {
			fds[1].fd = devd_fd;
			nfds = 2;
			timeout = -1;
		}

		ret = poll(fds, nfds, timeout);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret == -1)
			break;

		/* edev_monitor is finishing */
		if (fds[0].revents & POLLHUP)
			break;

		/* connection respawn timer expired */
		if (ret == 0 || devd_fd < 1)
			continue;

		if (fds[1].revents & POLLIN) {
			if ((len = recv(devd_fd, ev, ev_len, MSG_WAITALL))
			    <= 0) {
				close(devd_fd);
				devd_fd = -1;
				continue;
			}
#if defined(__NetBSD__)
			action = parse_ndevd_message(event, syspath, sizeof(syspath));
#else
			/* Replace terminating LF with 0 to make C-string */
			ev[len - 1] = '\0';
			action = parse_devd_message(ev, syspath, sizeof(syspath));
#endif
			if (action != UD_ACTION_NONE &&
			    udev_filter_match(um->udev, &um->filters, syspath))
				udev_monitor_send_device(um, syspath, action);
		}

		if (fds[1].revents & POLLHUP) {
			close(devd_fd);
			devd_fd = -1;
		}
	}

	if (devd_fd >= 0)
		close(devd_fd);

	return (NULL);
}
#else

static int
obsd_enumerate_cb(const char *path, mode_t type, void *arg)
{
	struct udev_monitor *um = arg;
	const char *syspath;
	const struct subsystem_config *sc;
	int devfd = -1;
	syspath = get_syspath_by_devpath(path);
	sc = get_subsystem_config_by_syspath(syspath);
	if (sc && (S_ISLNK(type) || S_ISCHR(type)) &&
            ((devfd = open(syspath, O_RDWR)) != -1)) {
		if (udev_list_insert(&um->cur_dev_list, syspath, NULL) == -1) {
			if (devfd != -1)
				close(devfd);
			return (-1);
		}
	}
	if (devfd != -1)
		close(devfd);
	return (0);
}

static void *
udev_monitor_thread(void *args)
{
	struct udev_monitor *um = args;
	sigset_t set;
	char path[DEV_PATH_MAX] = DEV_PATH_ROOT "/";
	char path_fido[DEV_PATH_MAX] = DEV_PATH_ROOT "/fido/";
	struct scandir_ctx mctx;
	int found;
	struct udev_list_entry *ce, *pe;
	size_t size = sizeof(&um->cur_serial);
	sigfillset(&set);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	mctx = (struct scandir_ctx) {
		.recursive = true,
		.cb = obsd_enumerate_cb,
		.args = um,
	};

	/* scan and fill the initial tree */
	pthread_mutex_lock(&scan_mtx);
	if ((scandir_recursive(path, sizeof(path), &mctx) == 0) &&
		(scandir_recursive(path_fido, sizeof(path_fido), &mctx) == 0)) {
		udev_list_entry_foreach(ce, udev_list_entry_get_first(&um->cur_dev_list)) {
			if (!_udev_list_entry_get_name(ce))
				continue;
			udev_list_insert(&um->prev_dev_list, udev_list_entry_get_name(ce), NULL);
		}
	}
	pthread_mutex_unlock(&scan_mtx);
	for (;;) {
		(void)sysctl(mib, 2, &um->cur_serial, &size, NULL, 0);
		if (um->cur_serial == um->prev_serial) {
			usleep(1000);
			continue;
		}
		/* reinit the current device list */
		udev_list_free(&um->cur_dev_list);
		pthread_mutex_lock(&scan_mtx);
		if ((scandir_recursive(path, sizeof(path), &mctx) == -1) &&
			(scandir_recursive(path_fido, sizeof(path_fido), &mctx) == -1)) 
			printf("failed to scan\n");
		pthread_mutex_unlock(&scan_mtx);
		/* attach */
		udev_list_entry_foreach(ce, udev_list_entry_get_first(&um->cur_dev_list)) {
			found = 0;
			if (!_udev_list_entry_get_name(ce))
				continue;
			if (udev_list_member(&um->prev_dev_list, _udev_list_entry_get_name(ce), NULL))
				found = 1;
			if (!found && udev_filter_match(um->udev, &um->filters, _udev_list_entry_get_name(ce))) {
				udev_monitor_send_device(um, _udev_list_entry_get_name(ce), UD_ACTION_ADD);
				udev_list_insert(&um->prev_dev_list, udev_list_entry_get_name(ce), NULL);
			}
		}
		/* detach */
		udev_list_entry_foreach(pe, udev_list_entry_get_first(&um->prev_dev_list)) {
			found = 0;
			if (!_udev_list_entry_get_name(pe))
				continue;
			if (udev_list_member(&um->cur_dev_list, _udev_list_entry_get_name(pe), NULL))
				found = 1;
			if (!found && udev_filter_match(um->udev, &um->filters, _udev_list_entry_get_name(pe))) {
				udev_monitor_send_device(um, _udev_list_entry_get_name(pe), UD_ACTION_REMOVE);
				udev_list_remove(&um->prev_dev_list, udev_list_entry_get_name(pe), NULL);
			}
		}
		um->prev_serial = um->cur_serial;
	}
	return (NULL);
}
#endif

LIBUDEV_EXPORT struct udev_monitor *
udev_monitor_new_from_netlink(struct udev *udev, const char *name)
{
	struct udev_monitor *um;
#if defined(__OpenBSD__)
	size_t size;
#endif
	
	TRC("(%p, %s)", udev, name);
	um = calloc(1, sizeof(struct udev_monitor));
	if (!um)
		return (NULL);

	if (pipe2(um->fds, O_CLOEXEC) == -1) {
		ERR("pipe2 failed");
		free(um);
		return (NULL);
	}

	um->udev = udev;
	_udev_ref(udev);
	um->refcount = 1;
	udev_filter_init(&um->filters);
	STAILQ_INIT(&um->queue);
#if defined(__OpenBSD__)
	udev_list_init(&um->cur_dev_list);
	udev_list_init(&um->prev_dev_list);
	size = sizeof(&um->cur_serial);
	(void)sysctl(mib, 2, &um->cur_serial, &size, NULL, 0);
	um->prev_serial = um->cur_serial;
#endif
	pthread_mutex_init(&um->mtx, NULL);

	return (um);
}

LIBUDEV_EXPORT int
udev_monitor_filter_add_match_subsystem_devtype(struct udev_monitor *um,
    const char *subsystem, const char *devtype)
{
	TRC("(%p, %s, %s)", um, subsystem, devtype);
	return (udev_filter_add(&um->filters, UDEV_FILTER_TYPE_SUBSYSTEM, 0,
	    subsystem, devtype));
}

LIBUDEV_EXPORT int
udev_monitor_filter_add_match_tag(struct udev_monitor *um, const char *tag)
{
	TRC("(%p, %s)", um, tag);
	return (udev_filter_add(&um->filters, UDEV_FILTER_TYPE_TAG, 0, tag, 0));
}

LIBUDEV_EXPORT int
udev_monitor_enable_receiving(struct udev_monitor *um)
{

	TRC("(%p)", um);

	if (pthread_create(&um->thread, NULL, udev_monitor_thread, um) != 0) {
		ERR("thread_create failed");
		return (-1);
	}

	return (0);
}

LIBUDEV_EXPORT int
udev_monitor_get_fd(struct udev_monitor *um)
{

	/* TRC("(%p)", um); */
	return (um->fds[0]);
}

LIBUDEV_EXPORT struct udev_monitor *
udev_monitor_ref(struct udev_monitor *um)
{

	TRC("(%p) refcount=%d", um, um->refcount);
	++um->refcount;
	return (um);
}

static void
udev_monitor_queue_drop(struct udev_monitor_queue_head *umqh)
{
	struct udev_monitor_queue_entry *umqe;

	while (!STAILQ_EMPTY(umqh)) {
		umqe = STAILQ_FIRST(umqh);
		STAILQ_REMOVE_HEAD(umqh, next);
		udev_device_unref(umqe->ud);
		free(umqe);
	}
}

LIBUDEV_EXPORT void
udev_monitor_unref(struct udev_monitor *um)
{
	TRC("(%p) refcount=%d", um, um->refcount);
	if (--um->refcount == 0) {
		close(um->fds[0]);
		pthread_cancel(um->thread);
		pthread_join(um->thread, NULL);
		close(um->fds[1]);
		udev_filter_free(&um->filters);
#if defined(__OpenBSD__)
		udev_list_free(&um->cur_dev_list);
		udev_list_free(&um->prev_dev_list);
#endif
		udev_monitor_queue_drop(&um->queue);
		pthread_mutex_destroy(&um->mtx);
		_udev_unref(um->udev);
		free(um);
	}
}

LIBUDEV_EXPORT
struct udev *udev_monitor_get_udev(struct udev_monitor *um)
{

	TRC();
	return (um->udev);
}

LIBUDEV_EXPORT int
udev_monitor_set_receive_buffer_size(struct udev_monitor *um, int size)
{
	TRC("(%d)", size);
	UNIMPL();
	return (0);
}

LIBUDEV_EXPORT int
udev_monitor_filter_update(struct udev_monitor *um)
{
	TRC();
	UNIMPL();
	return (0);
}

LIBUDEV_EXPORT int
udev_monitor_filter_remove(struct udev_monitor *um)
{
	TRC();
	UNIMPL();
	return (0);
}
