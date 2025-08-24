xmltostr = './xmltostr.sh'

kbdd_introspection_c = custom_target('kbdd_introspection.c',
    output: 'kbdd_introspection.c',
    input: 'kbdd_introspection.xml',
    command: [ xmltostr, 'kbdd_srv_introspection', '@INPUT@', '@OUTPUT@' ],
    )


wlr_kbdd_files += kbdd_introspection_c
wlr_kbdd_files += files('dbus_server.c')
