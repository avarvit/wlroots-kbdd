project(
	'wlroots-kbdd',
	'c',
	version: '0.19.1',
	license: 'MIT',
	meson_version: '>=0.59.0',
	default_options: [
		'c_std=' + (meson.version().version_compare('>=1.3.0') ? 'c23,c11' : 'c11'),
		'warning_level=2',
		'werror=true',
	],
)

version = meson.project_version().split('-')[0]
version_major = version.split('.')[0]
version_minor = version.split('.')[1]
versioned_name = '@0@-@1@.@2@'.format(meson.project_name(), version_major, version_minor)

little_endian = target_machine.endian() == 'little'
big_endian = target_machine.endian() == 'big'

# stolen from wlroots' meson.build, probably not needed here
add_project_arguments([
	'-D_POSIX_C_SOURCE=200809L',
	'-DWLR_USE_UNSTABLE',
	'-DWLR_LITTLE_ENDIAN=@0@'.format(little_endian.to_int()),
	'-DWLR_BIG_ENDIAN=@0@'.format(big_endian.to_int()),
], language: 'c')

cc = meson.get_compiler('c')

add_project_arguments(cc.get_supported_arguments([
	'-Wundef',
	'-Wlogical-op',
	'-Wmissing-include-dirs',
	'-Wold-style-definition',
	'-Wpointer-arith',
	'-Winit-self',
	'-Wstrict-prototypes',
	'-Wimplicit-fallthrough=2',
	'-Wendif-labels',
	'-Wstrict-aliasing=2',
	'-Woverflow',
	'-Wmissing-prototypes',
	'-Walloca',
	'-Wno-missing-braces',
	'-Wno-missing-field-initializers',
	'-Wno-unused-parameter',
]), language: 'c')

# Compute the relative path used by compiler invocations.
source_root = meson.current_source_dir().split('/')
build_root = meson.global_build_root().split('/')
relative_dir_parts = []
i = 0
in_prefix = true
foreach p : build_root
	if i >= source_root.length() or not in_prefix or p != source_root[i]
		in_prefix = false
		relative_dir_parts += '..'
	endif
	i += 1
endforeach
i = 0
in_prefix = true
foreach p : source_root
	if i >= build_root.length() or not in_prefix or build_root[i] != p
		in_prefix = false
		relative_dir_parts += p
	endif
	i += 1
endforeach
relative_dir = join_paths(relative_dir_parts) + '/'

# Strip relative path prefixes from the code if possible, otherwise hide them.
if cc.has_argument('-fmacro-prefix-map=/prefix/to/hide=')
	add_project_arguments(
		'-fmacro-prefix-map=@0@='.format(relative_dir),
		language: 'c',
	)
else
	add_project_arguments(
		'-D_WLR_REL_SRC_DIR="@0@"'.format(relative_dir),
		language: 'c',
	)
endif

features = {
}
internal_features = {
}
internal_config = configuration_data()

wayland_project_options = ['tests=false', 'documentation=false']
wayland_server = dependency('wayland-server',
	version: '>=1.23',
	fallback: 'wayland',
	default_options: wayland_project_options,
)

xkbcommon = dependency(
	'xkbcommon',
	fallback: 'libxkbcommon',
	default_options: [
		'enable-tools=false',
		'enable-x11=false',
		'enable-docs=false',
		'enable-xkbregistry=false',
	],
)

wlroots = dependency('wlroots-0.19',
        version: '>=0.18',
        fallback: 'wlroots',
)


glib = dependency('glib-2.0',
        version: '>=2.74',
        fallback: 'glib',
)

gio = dependency('gio-2.0',
        version: '>=2.74',
        fallback: 'gio',
)

wlr_kbdd_files = []
wlr_kbdd_deps = [
	wayland_server,
	xkbcommon,
        wlroots,
        glib,
        gio,
]

subdir('types')
subdir('backend')
subdir('kbdd')
subdir('dbus_server')
subdir('dbus_client')
#subdir('include')

wlr_kbdd_inc = include_directories('include')

symbols_file = 'wlroots_kbdd.syms'
symbols_flag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), symbols_file)
lib_wlroots_kbdd = library(
	versioned_name, wlr_kbdd_files,
	dependencies: wlr_kbdd_deps,
	include_directories: [wlr_kbdd_inc],
	install: true,
	link_args: symbols_flag,
	link_depends: symbols_file,
)

wlr_kbdd_vars = {}
foreach name, have : features
	wlr_kbdd_vars += { 'have_' + name.underscorify(): have.to_string() }
endforeach

wlroots_kbdd = declare_dependency(
	link_with: lib_wlroots_kbdd,
	dependencies: wlr_kbdd_deps,
	include_directories: wlr_kbdd_inc,
	variables: wlr_kbdd_vars,
)

meson.override_dependency(versioned_name, wlroots_kbdd)

summary(features + internal_features, bool_yn: true)

pkgconfig = import('pkgconfig')
pkgconfig.generate(
	lib_wlroots_kbdd,
	name: versioned_name,
	description: 'Kbdd add-on layer to wlroots',
	subdirs: versioned_name,
	url: 'https://github.com/avarvit/wlroots-kbdd',
	variables: wlr_kbdd_vars,
)
