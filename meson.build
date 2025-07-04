project('libudev-bsd', 'c',
	version : '0.6.0.2',
	license : 'BSD2CLAUSE',
	default_options : [
		'buildtype=debugoptimized',
		'warning_level=1',
		'c_std=c11',
		'werror=true' ],
	meson_version : '>=0.50.0')

libudevbsd_version = meson.project_version().split('.')

dir_data        = join_paths(get_option('prefix'), get_option('datadir'))
dir_sysconf     = join_paths(get_option('prefix'), get_option('sysconfdir'))
dir_libexec     = join_paths(get_option('prefix'), get_option('libexecdir'))
dir_lib         = join_paths(get_option('prefix'), get_option('libdir'))
dir_src         = join_paths(meson.current_source_dir())

# Compiler setup
cc = meson.get_compiler('c')
cflags = ['-fvisibility=hidden', '-D_GNU_SOURCE']
add_project_arguments(cflags, language: 'c')

# config.h
config_h = configuration_data()
config_h_inc = include_directories('.')
if get_option('buildtype') == 'debug' or get_option('buildtype') == 'debugoptimized'
	config_h.set_quoted('MESON_BUILD_ROOT', meson.current_build_dir())
else
	config_h.set_quoted('MESON_BUILD_ROOT', '')
endif

devinfo_dep = dependency('', required:false)
if cc.has_header('devinfo.h')
	devinfo_dep = cc.find_library('devinfo')
	config_h.set('HAVE_DEVINFO_H', '1')
endif

procstat_inc = '''#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
'''

procstat_dep = cc.find_library('procstat', required: false)
if procstat_dep.found()
	if cc.has_header_symbol('libprocstat.h', 'procstat_open_sysctl', prefix : procstat_inc)
		config_h.set('HAVE_LIBPROCSTAT_H', '1')
	endif
endif

if cc.has_header('dev/hid/hidraw.h')
	config_h.set('HAVE_DEV_HID_HIDRAW_H', '1')
endif

if cc.has_header('linux/input.h')
	config_h.set('HAVE_LINUX_INPUT_H', '1')
endif
if cc.has_header('dev/evdev/input.h')
	config_h.set('HAVE_DEV_EVDEV_INPUT_H', '1')
endif

if cc.has_header('net/if_dl.h')
	config_h.set('HAVE_NET_IF_DL_H', '1')
endif

if cc.has_header('sys/tree.h')
	config_h.set('HAVE_SYS_TREE_H', '1')
endif

if cc.has_function('devname_r')
	config_h.set('HAVE_DEVNAME_R', '1')
endif

if cc.has_function('pipe2')
	config_h.set('HAVE_PIPE2', '1')
endif

if cc.has_function('strchrnul')
	config_h.set('HAVE_STRCHRNUL', '1')
endif

if cc.has_function('strlcat')
	config_h.set('HAVE_STRLCAT', '1')
endif

if cc.has_function('strlcpy')
	config_h.set('HAVE_STRLCPY', '1')
endif

if cc.has_function('sysctlbyname')
	config_h.set('HAVE_SYSCTLBYNAME', '1')
endif

libudevbsd_so_version = '0.0.0'
# Dependencies
thread_dep = dependency('threads')

pkgconfig = import('pkgconfig')

install_headers('libudev.h')
src_libudevbsd = [ 'udev.c',
	'udev-dev.c',
	'udev-dev.h',
	'udev-device.c',
	'udev-device.h',
	'udev-enumerate.c',
	'udev-enumerate.h',
	'udev-filter.c',
	'udev-filter.h',
	'udev-global.h',
	'udev-hwdb.c',
	'udev-list.c',
	'udev-list.h',
	'udev-monitor.c',
	'udev-net.c',
	'udev-net.h',
	'udev-pci.c',
	'udev-pci.h',
	'udev-queue.c',
	'udev-sys.c',
	'udev-sys.h',
	'udev-utils.c',
	'udev-utils.h',
	'utils.c',
	'utils.h'
]

if get_option('enable-gpl')
	config_h.set('ENABLE_GPL', '1')
	src_libudevbsd += [ 'utils-gpl.c',  'utils-gpl.h' ]
endif


deps_libudevbsd = [
	thread_dep,
	devinfo_dep,
	procstat_dep
]

lib_libudevbsd = shared_library('udev',
	src_libudevbsd,
	include_directories : config_h_inc,
	dependencies : deps_libudevbsd,
	version : libudevbsd_so_version,
	install : true
)

pkgconfig.generate(lib_libudevbsd,
	name : 'libudev',
	url : 'https://github.com/kikadf/libudev-bsd',
	description : 'Library to access udev device information',
	version : '243', # XXX - should be a proper version
)

# output files
configure_file(output : 'config.h', install : false, configuration : config_h)
