
[ Building ]

We offer a 'make world' script that retrieves nearly everything from
the Internet, creates harmonious configurations, and then builds
everything.  The build process generates many side products, and thus
you should build within a dedicated directory (and not within the
Afterburner directory).

To build, change into your dedicated build directory, and execute:
make -f path/to/afterburner/Makefile
make menuconfig
make world

You can skip the 'make menuconfig' and accept the defaults.  If you
want to specify the build components and their versions, then you must
run 'make menuconfig' and tailor the configuration to your
preferences.  The default configuration will build components that
have been under test for over a year, and thus are fairly old.

Other commands:
  make downloads
    - Lists the files that we retrieve from the Internet.
  make download_now
    - Retrieve the files from the Internet immediately.
  make cvs
    - Lists the CVS repositories that we retrieve from the Internet.
  make cvs_now
    - Retrieve the CVS repositories from the Internet now.
  make installs
    - List all the packages that will be installed via 'make world'

You don't need to use 'make menuconfig' for configuring the system; 
alternatives are 'make xconfig', 'make ttyconfig', and 'make batchconfig'.
The 'make batchconfig' is useful for setting your configuration 
variables via the command line.  For example, to enable Pistachio P3,
to disable QEMU, and to enable gcc, use:
  make cml_params='pistachio_p3=y qemu=n gcc=y' batchconfig


[ Running ]

The build process installs all of the boot files into your configured
boot directory (by default, the "boot" subdirectory within the build
directory).  The boot directory includes an example GRUB menu.

You can test by booting within QEMU:
make run-qemu

When running a complete Linux system, be sure that you disable TLS in
your glibc, otherwise the boot process will probably crash.  To disable
TLS, rename /lib/tls to /lib/tls.disabled.


[ Problems building ]

If you have problems building Antlr, upgrade your compiler.  If you
have problems building other components, then you probably need to
downgrade your compiler.  You can build a tested compiler (gcc 3.4.4)
within the Afterburn build environment by enabling it within 'make
menuconfig'.

The components that definitely need a recent compiler are Antlr and
the Afterburner assembler parser.  You can build them individually
using a different compiler, and use an older compiler with the
remaining components.  Perhaps execute 'make world', and when it fails
building Antlr, execute 'make install-antlr' and then 'make
install-afterburner' while using the newer compiler, and then return
to building 'make world' with the older compiler.


[ Mercurial integration ]

We distribute the Afterburner projects via the CVS system, because CVS
performs well as a central repository.  It is outclassed by Mercurial
(Hg) for normal revision control activities, and we recommend that you
use Mercurial for maintaining your local source tree.  Both Mercurial
and CVS comfortably live together in the same working directory.


[ Developing, building, and configuring ]

The 'make world' script helps developers modify many of the
subcomponents, such as the wedge, Linux, and Xen.  The following is a
summary of the additional support commands provided by the 
'make world' script.

Most make commands have a verb, followed by identification
information.  The identification information identifies the package, and
then a specific configuration.  An example:
  install-wedge-xen-3-dom0-vmi
The verb is 'install', the package is 'wedge', and the configuration
is 'xen-3-dom0-vmi'.

Make commands are undefined unless the packages and their
configuration are enabled within 'make menuconfig'.

For several of the packages that have interactive configuration
systems, we have a 'reconfig' command.  The packages include
linux-2.6, linux-vmi, wedge, and linux-2.4.  For example:
  make reconfig-linux-2.6-driver
  make reconfig-linux-vmi-driver
  make reconfig-wedge-l4ka-passthru
  make reconfig-wedge-xen-2-dom0

Most of the packages have a clean command.  For example:
  make clean-linux-2.6-driver
  make clean-linux-vmi-driver
  make clean-wedge-l4ka-passthru
Some of the packages define global clean commands that clean all
configurations.

Several of the packages have an uninstall command.  The uninstall
command removes the final output of a package, causing make to
regenerate the final output, while rebuilding any files that changed.
You would follow the uninstall command with an install command, and it
is a useful procedure to use when you are editing any of the source
code, such as Linux's, Xen's, or the wedge's.  For example:
  make uninstall-linux-2.6-driver
  make uninstall-wedge-l4ka-passthru
  make uninstall-wedge-xen-2-dom0
An example that builds a new Xen wedge with changes that you made to
the wedge source code:
  make uninstall-wedge-xen-2-dom0 && make install-wedge-xen-2-dom0


