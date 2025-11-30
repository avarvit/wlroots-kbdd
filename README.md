# `wlroots-kbdd`

A RaspberryPi-specific wlroots interception library to enable
keyboard layout reporting and switching via DBus commands.
Currently supported compositors include only labwc.

## Choose your distribution
The current HEAD and the installation instructions are for Trixie.
If you are running on Bookworm, checkout the "bookworm" tag before
building as follows, and re-check README.md (this file), since the
dependencies for Bookworm are slightly different from the ones for
Trixie.

Here is how to checkout the "bookworm" tag:
```
$ git checkout bookworm
```

## Install
Note: to test things, it might be best to switch into "Boot into Text
Console" mode (using `raspi-config`). You can switch back into "Desktop
GUI" mode later (see next section).

```
$ sudo apt update
$ sudo apt install libxkbcommon-dev libwlroots-0.19-dev libglib2.0-dev
$ git clone https://github.com/avarvit/wlroots-kbdd
$ cd wlroots-kbdd
$ meson setup build
$ meson compile -C build
$ env LD_PRELOAD=$PWD/build/libwlroots-kbdd-0.19.so labwc
```
Note: if you are running from a "Boot into Text Console" setup, there
is no need to install anything under `/usr`. The library will run from
its build directory if you issue the last command above. However, if
you want to use it from "Boot into Desktop GUI" mode, you need to take
another few steps; if this is the case, please read the next section.

## Using "Boot into Desktop GUI" with this library
Note: the steps in this section are necessary only you want to
run labwc with the library enabled from "Boot in Desktop GUI" mode.

So far, you did not need to fiddle with your system. However, if you
want to make this library work with the raspi-config setup "Boot into
Desktop GUI" mode, you need to change one system file. Happily, this
is a shell script and is very easy to change back. However, you need
to exercise some caution: if you garble this file and have set up your
RPi to boot into "Desktop GUI" mode, you risk getting trapped into a
loop where your system tries to bring up labwc, fails, and tries again
<i>ad infinitum</i>. Here is how to do this cautiously.

#### Install this library under /usr/local
You first need to install the library somewhere safer than your home
directory, in a place where it will not get accidentally removed or
overwritten. This place in a linux system is `/usr/local` To install
the library under `/usr/local`, do this:
```
$ sudo meson install -C build
```
will install the library under `/usr/local/lib/aarch64-linux-gnu`.

#### Change the `labwc-pi` script to use the library
Next, you need to tell your system to use the library from the shell
script that the session manager invokes when you choose "Boot into
Desktop GUI" from raspi-config. This script is `/usr/bin/labwc-pi`.
Before doing anything with it, it is wise to save a copy:
```
$ sudo cp /usr/bin/labwc-pi /usr/bin/labwc-pi.orig
```
You then need to edit the file (remember that you need to use `sudo`
to do this) and change this line
```
exec /usr/bin/labwc -m $@
```
into this:
```
exec env LD_PRELOAD=/usr/local/lib/aarch-64-linux-gnu/libwlroots-kbdd-0.19.so /usr/bin/labwc -m $@
```

#### Test it
You may wish to execute `/usr/bin/labwc-pi` to make sure it works
before switching from within raspi-config into 'Boot into Destkop
GUI" mode (just run `labwc-pi` from the command line and ensure it
works OK; if not, and you cannot figure what is wrong and fix it,
make sure to copy `/usr/bin/labwc-pi.orig` that you saved earlier
back into `/usr/bin/labwc-pi`). That's it. You are done.


## Background and motivation
Wayland uses the xkbcommon library to represent keyboad setup and
input (e.g., keyboard layouts, keymaps, modifiers, keypresses, etc.). 
Using xkbcommon, multiple keyboard layouts can be supported, using a
key combo (e.g., left-Alt-Shift or left Ctrl-Shift) to switch between
configured layout groups (aka languages). Wayland leaves this task to
compositors (it is not a core protocol feature), so each compositor
has its own way of configuring multiple layouts and the key combo
to switch among them, while all compositors use xkbcommon underneath.

Some compositors also support a per-window keyboard layout group
(aka language) setup, as per the X11 `kbdd` program (hence the name
of this library). This "kbdd" functionality can be either built in
(labwc), or implemented by means of a plugin (wayfire). [Note that
a per-window layout group setup is a *huge* time saver when working
in multiple languages; unfortunately, today's desktop environments,
probably influenced by mobile device virtual keyboards, rarely
acknowledge this need...].

What is missing, then? Answer, two good-old X11 features, namely:
(a) an indication of the current layout group (language) so that
the user can see what language they are typing in (instead of having
to type and delete, if the wrong language is switched in) and
(less crucial, but still useful) (b) a point-and-click method for
switching between layout groups, e.g. in a taskbar widget.

## What does `wlroots-kbdd` do?
`wlroots-kbdd` uses DBus to send and receive messages about keyboard
layout changes. By sending appropriate DBus messages when it detects
a layout change (either because the user changed the current layout
group via the key combo, or because they focused in a different window,
thus causing a layout group change), `wlroots-kbdd` sends the current
layout name to a widget that displays it in the taskbar. And, by
receiving appropriate DBus messages from that same widget,
`wlroots-kbdd` can switch the keyboard layout of the currently
focused-in window.

## History
The history behind `wlroots-kbdd` is related to `wf-panel-pi`, a
derivative of `wf-shell`, a Wayland shell that implements a taskbar
with various utility widgets. As its name and origin suggest,
`wf-shell` is part of the `wayfire` compositor add-ons; the
Raspberry PI UI experts have ported it into a RPI-specific
panel, to be used with both `wayfire` and, more recently, `labwc`.

`wf-panel-pi` accepts commands via DBus, addressed to
`org.wayfire.wfpanel`. Commands take the form `command <widget>
<argument>`, directing the "command" to widget `<widget>`
and passing it the argument `<argument>`. This is used among others
to drive bundled `wf-panel-pi` widgets, such as a volume control
widget, into actions, such as mute, volume up/down etc.

Of course, `wf-panel-pi` does not come ready with such a widget,
thus I have developed this myself, and given it the name `kbdlayout`.
The `kbdlayout` widget, after having received a list of layouts as
above, and upon a click, displays a menu of layout short names, and,
when the user clicks on one of these, sends back to the compositor
(at DBus address `org.wayfire.kbdd.layout`) a `switch` command,
having as an argument the layout name the user clicked on. This is
intended to instruct the compositor to swithch the keyboard layout
of the currently configured (toplevel) window to the one sent over
DBus. Additionally, `kbdlayout` can send to the compositor an
`enable` command with a zero or non-zero argument to request that
the overall functionality be turned off or on, respectively. See
the URLs at the end of this file on where to find `kbdlayout` and
check its README for install instructions.

## Caveats
`wlroots-kbdd` works by intercepting some wlroots calls. Interception
works using the LD_PRELOAD mechanism to preload this library before
the wlroots shared library. This means that if the wlroots libary is
statically linked in the compositor library, wlroots-kbdd will not
work. It may also fail if the compositor is setuid-root.

Note that `wlroots-kbdd` removes itself from the LD_PRELOAD path and,
if LD_PRELOAD only contains the `wlroots-kbdd` library, it deletes
the LD_PRELOAD variable from the environment of the compositor. This
is done early enough in the initialization of the compositor, so that
no other programs spawned by the compositor are affected by the
LD_PRELOAD setting.

In this current version, if a shutdown is initiated while a DBus
message is sent and before a few milliseconds (the timeout), the
shutdown may hung for 90 seconds waiting for the DBus session to
terminate. This is not serious, the system eventually shuts down
and no harm is done.

## Debug logging
`wlroots-kbdd` will log debug information if the environment
variable `DEBUG_WLROOTS_KBDD` is set. If the variable is set
to a path under `/tmp`, its value will be taken as the name
of the debug file (for security reasons, it is required to be
under `/tmp`, if not, the path is ignored). If no path is given
or if an erroneous path is given, or the file cannot be opened
for output, `wlroots-kbdd` will log to `stderr` instead.
Example use:
```
$ env LD_PRELOAD=$PWD/build/libwlroots-kbdd-0.18.so DEBUG_WLROOTS_KBDD=/tmp/wlr_kbdd-debug.txt labwc
```
will output debug logging information in `/tmp/wlr_kbdd-debug.txt`.
Note that no strict security checks are performed (e.g., it is not
checked whether the debug file already exists and/or is a symbolic
link), so please take your precautions so as not to overwrite any
precious files.

## Security
`wlroots-kbdd` is not intended to provide a bullet-proof channel for
handling keyboard layouts. Any process that can write to DBus can
fool either `wlroots-kbdd` to switch layouts or turn it off/on, or
misguide the `kbdlayout` widget into displaying wrong info about
the the available and the current layout(s). However, `wrloots-kbdd`
does not introduce any methods that could be used for e.g. keyboard
hijacking and/or stealing keypresses etc. In that sense, no essential
loss of security is introduced by using `wlroots-kbdd`.

## References
https://gitlab.freedesktop.org/wlroots/wlr-protocols/-/merge_requests/31
contains a discussion about implementing "keybind" wlroots protocol (the
PR was rejected).

Some compositors, like sway, have special IPC messages to handle this task
(Googling will return meaningful results).

`wlroots-kbdd` will not work for Wayfire. For Wayfire, I have built a
plugin (based on Alex Jake Green's `wayfire-kbdd-plugin`) that can be
found here: https://github.com/avarvit/wayfire-kbdd-plugin .

https://github.com/raspberrypi-ui/wf-panel-pi is the official repo of
`wf-panel-pi`.

https://github.com/avarvit/wf-panel-pi-kbdlayout is my repo for the
`kbdlayout` widget. Check the README for more information.

https://github.com/WayfireWM/wf-shell is the Wayfire shell app from which
`wf-panel-pi` originates.
