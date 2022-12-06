project( 'rndu4000', 'C', version : '1.0.0' )

uname = run_command( 'uname', '-r', check: true ).stdout().strip()
uname_without_arch =  run_command(  'sh',
                                '-c',
                                'uname -r | sed \'s/-/ /g\' | awk \'{ print $1 "-" $2 }\'',
                                check: true ).stdout().strip()

kernel_dir = '/lib/modules/' + uname + '/build'

rndu4000_src = []
rndu4000_src += 'src/module.c'
rndu4000_src += 'src/lcd.c'
rndu4000_src += 'src/led.c'
rndu4000_src += 'src/buttons.c'



##########################################################################
# Fake target to allow qtcreator editing without a scaring syntax analyser
rndu4000_includes = []
rndu4000_includes += '/usr/lib/gcc/x86_64-linux-gnu/10/include'
rndu4000_includes += '/usr/src/linux-headers-' + uname + '/include'
rndu4000_includes += '/usr/src/linux-headers-' + uname + '/arch/x86/include/generated'
rndu4000_includes += '/usr/src/linux-headers-' + uname + '/arch/x86/include/generated/uapi'
rndu4000_includes += '/usr/src/linux-headers-' + uname + '/include/generated/uapi'

rndu4000_includes += '/usr/src/linux-headers-' + uname_without_arch + '-common/include'
rndu4000_includes += '/usr/src/linux-headers-' + uname_without_arch + '-common/arch/x86/include'
rndu4000_includes += '/usr/src/linux-headers-' + uname_without_arch + '-common/arch/x86/include/uapi'
rndu4000_includes += '/usr/src/linux-headers-' + uname_without_arch + '-common/include/uapi'


add_project_arguments( '-D__KERNEL__', language : 'C' )
add_project_arguments( '-DCONFIG_X86_X32_ABI', language : 'C' )
add_project_arguments( '-DCC_USING_FENTRY', language : 'C' )
add_project_arguments( '-DMODULE', language : 'C' )
add_project_arguments( '-DKBUILD_BASENAME="rndu4000"', language : 'C' )
add_project_arguments( '-DKBUILD_MODNAME="rndu4000"', language : 'C' )
add_project_arguments( '-Wno-pointer-sign', language : 'C' )

if get_option( 'qtcreator' )
  rndu4000 = shared_library(    'rndu4000',
                                rndu4000_src,
                                include_directories : rndu4000_includes,
                                build_by_default: false )
endif


##########################################################################
# Real target
custom_target( 'rndu4000_module_build',
            input : rndu4000_src,
            output : 'rndu4000.ko',
            command: [ 'make',
                        '-j', '4',
                        '-C', kernel_dir,
                        'M='+ meson.current_build_dir(),
                        'src=' + meson.current_source_dir() + '/src',
                        'clean',  'modules' ],
            console: true,
            install: false,
            build_by_default: true )

