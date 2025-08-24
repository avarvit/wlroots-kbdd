# `wlroots-kbdd` DBus messages

## DBus output
The outputs that `wlroots-kbdd` sends over DBus to `org.wayfire.wfpanel`
are:

- a comma-separated list of the configured keyboard layout short
names (e.g. "US,FR,GB"); this is meant to inform the `kbdlayout`
plugin about all the available layouts, so that a menu can be
shown to the user when the widget is clicked
- a single layout name (e.g., "FR"); this is meant to inform
`kbdlayout` about the *current* keyboard layout, so that it gets
displayed to the user
- a dash ("-"), meaning that the `wlroots-kbdd` functionality is
turned off, and `kbdlayout` should stop displaying layouts and
popping up a menu (a double questionmark is displayed instead
of a current layout, and the menu is disabled)

The introspection XML for `org.wayfire.wfpanel` is:
```
<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name='org.wayfire.wfpanel'>
    <annotation name='org.wayfire.wfpanel.Annotation' value='OnInterface'/>
    <method name='command'>
      <annotation name='org.wayfire.wfpanel.Annotation' value='OnMethod'/>
      <arg type='s' name='plugin' direction='in'/>
      <arg type='s' name='command' direction='in'/>
    </method>
  </interface>
</node>
```
This means that the actual DBus message sent is of the form (&lt;plugin&gt;,
&lt;command&gt;), and wf-panel-pi upon receiving this message, passes
command &lt;command&gt; (in our case, one of the cases listed above) to
the plugin &lt;plugin&gt; (in our case this is `kbdlayout`). Replicating
this functionality with the `wfpanelctl(1)` program is possible (not
to mention fun and educational).

## DBus input
The inputs that `wlroots-kbdd` accepts over DBus at `org.wayfire.kbdd.layout`
are:

- a `switch <layout-short-name>` command to instruct the compositor
to switch the layout of the currently focused toplevel window to
to `<layout-short-name>`
- an `enable <integer>` command to instruct the compositor to
switch the overall functionality off (zero) or on (non-zero) 

The introspection XML for  `org.wayfire.kbdd.layout` is:
```
<?xml version="1.0" encoding="UTF-8"?>
<node>
  <interface name="org.wayfire.kbdd.layout">
    <!--
        The "changed" signal is for notifying other apps about a keyboard
        layout change; currently, it is not implemented
    -->
    <signal name="changed">
      <arg type="s" name="layout"/>
    </signal>
    <!--
        The "enable" method takes one unsigned integer (status) as argument;
        it is supposed to be called by a shell app to initialize or to stop
        kbdd's keyboard layout switching protocol; if "status" is non-zero,
        kbdd will henceforth honor "switch" commands; if "status" is zero,
        switching will be disabled (default state is desabled); kbdd keeps
        track of the last focus switch before this message, assuming that
        this last-focused-to view is the shell; it then keeps track of the
        focus switches to remember which was the last focused-to view
        besides the shell; it is this last-focused-to view that gets its
        keyboard changed by a subsequent "switch" command.
    -->
    <method name="enable">
      <arg type="u" name="status" direction="in"/>
    </method>
    <!--
        The "switch" method takes as an argument a string representing a
        keyboard layout (in capitalized xkbd 2-letter format, e.g., "US",
        "RU", "GR", etc.); if this layout is valid (in the sense that it
        is a configured xkbd layout), it is accepted, otherwise it is
        rejected; if accepted, the last-non-shell view gets its stored
        keyboard layout switched accordingly, so when focus returns to
        that view, the new layout applies.
    -->
    <method name="switch">
      <arg type="s" name="layout" direction="in"/>
    </method>
  </interface>
</node>
```
For example, sending the message `enable(0)` will disable the `kbdd`
functionality (no more DBus messages will be sent out, and "switch"
messages will be ignored); `enable(1)` (or any non-zero value) will
restore the working status. Sending the message `switch("GB")` will
cause a switch of the current layout group to "GB", provided that
"GB" is one of the configured layouts (otherwise the switch command
will be ignored). Testing this functionality with the `dbus-send(1)`
program is possible (not to mention fun and educational).
