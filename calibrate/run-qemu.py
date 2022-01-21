#! /usr/bin/python3

import sys
import os
import subprocess
import tempfile
import shutil

params = dict()

# Directory with scripts and other data
params['SCRIPTDIR'] = os.path.abspath(os.path.dirname(sys.argv[0]))

# System dracut base directory
params['DRACUTDIR'] = '/usr/lib/dracut'

# Total VM memory in KiB:
params['TOTAL_RAM'] = 1024 * 1024

# Number of CPUs for the VM
params['NUMCPUS'] = 2

# Where kernel messages should go
params['MESSAGES_LOG'] = 'messages.log'

# Where trackrss log should go
params['TRACKRSS_LOG'] = 'trackrss.log'

# Store the system architecture for convenience
params['ARCH'] = os.uname()[4]

# Physical address where elfcorehdr should be loaded.
# This is tricky. The elfcorehdr memory range is removed from the kernel
# memory map with a command line option, but the kernel boot code runs
# before the command line is parsed, and it may overwrite the data.
if params['ARCH'] == 'aarch64':
    # QEMU defines all RAM at 1G physical for AArch64
    ADDR_ELFCOREHDR = (1024 * 1024 * 1024) + (256 * 1024 * 1024)
else:
    # For other platforms, the region at 768M should be reasonably safe,
    # because it is high enough to avoid conflicts with special-purpose
    # regions and low enough to avoid conflicts with allocations at the
    # end of RAM.
    ADDR_ELFCOREHDR = 768 * 1024 * 1024

def install_kdump_init(bindir):
    env = os.environ.copy()
    env['DESTDIR'] = os.path.abspath('.')
    args = (
        'cmake',
        '--install', os.path.join(bindir, '..', 'dracut'),
    )
    subprocess.call(args, env=env, stdout=sys.stderr)

def init_local_dracut(params):
    basedir = params['DRACUTDIR']
    os.symlink(shutil.which('dracut'), 'dracut')
    for name in os.listdir(basedir):
        if name == 'modules.d':
            os.mkdir(name)
            for module in os.listdir(os.path.join(basedir, name)):
                dst = os.path.join(name, module)
                if module[2:] != 'kdump':
                    os.symlink(os.path.join(basedir, dst), dst)

            dst = os.path.join(name, '99kdump')
            os.symlink(os.path.join('..', basedir[1:], dst), dst)
        else:
            os.symlink(os.path.join(basedir, name), name)

class build_initrd(object):
    def __init__(self, bindir, params, config, path='test-initrd'):
        # First, create the base initrd using dracut:
        env = os.environ.copy()
        env['KDUMP_LIBDIR'] = os.path.abspath('usr/lib/kdump')
        env['KDUMP_CONFIGFILE'] = os.path.join(params['SCRIPTDIR'], config)
        env['DRACUT_PATH'] = ' '.join((
            os.path.abspath(os.path.join(bindir, '..', 'kdumptool')),
            '/sbin',
            '/bin',
            '/usr/sbin',
            '/usr/bin'))

        if params['NET']:
            if params['ARCH'].startswith('s390'):
                net_driver = 'virtio-net'
            else:
                net_driver = 'e1000e'
            extra_args = ('--add-drivers', net_driver)
        else:
            extra_args = ()
        args = (
            os.path.abspath('dracut'),
            '--local',
            '--hostonly',

            # Standard kdump initrd options:
            '--omit', 'plymouth resume usrmount',
            '--add', 'kdump',

            # Create a simple uncompressed CPIO archive:
            '--no-compress',
            '--no-early-microcode',

            # Additional options:
            *extra_args,

            path,
            params['KERNELVER'],
        )
        subprocess.call(args, env=env)

        # Replace /init with trackrss:
        trackrss = os.path.join(bindir, 'trackrss')
        shutil.copy(trackrss, './init')
        args =(
            'cpio', '-o',
            '-H', 'newc',
            '--owner=0:0',
            '--append', '--file=' + path,
        )
        with subprocess.Popen(args, stdin=subprocess.PIPE) as p:
            p.communicate(b'init')

        # Compress the result:
        subprocess.call(('xz', '-f', '-0', '--check=crc32', path))
        self.path = path + os.path.extsep + 'xz'

class build_elfcorehdr(object):
    def __init__(self, bindir, addr, path='elfcorehdr.bin'):
        self.address = addr
        self.path = path

        mkelfcorehdr = os.path.join(bindir, 'mkelfcorehdr')
        args = (
            mkelfcorehdr,
            path,
            str(addr),
        )
        subprocess.call(args)

        self.size = (os.stat(self.path).st_size + 1023) // 1024

def qemu_name(machine):
    if machine == 'aarch64_be':
        machine = 'aarch64'
    elif machine == 'armv8b' or machine == 'armv8l':
        machine = 'arm'
    if machine == 'i586' or machine == 'i686':
        machine = 'i386'
    if machine == 'ppcle':
        machine = 'ppc'
    elif machine == 'ppc64le':
        machine = 'ppc64'
    return 'qemu-system-' + machine

def run_qemu(bindir, params, initrd, elfcorehdr):
    arch = params['ARCH']
    extra_qemu_args = []
    extra_kernel_args = []

    # Set up console tty and a serial port for trackrss
    if arch.startswith('ppc'):
        console = 'hvc0'
        logdev = '229,1'        # hvc1
    elif arch.startswith('s390'):
        console = 'sclp0'
        logdev = '229,0'        # hvc0
    else:
        console = 'ttyS0'
        logdev = '4,65'         # ttyS1

    if arch.startswith('s390'):
        console_args = (
            '-serial', 'file:' + params['MESSAGES_LOG'],
            '-device', 'virtio-serial-ccw',
            '-chardev', 'file,path={},id=hvc0'.format(params['TRACKRSS_LOG']),
            '-device', 'virtconsole,nr=0,chardev=hvc0',
            )
    else:
        console_args = (
            '-serial', 'file:' + params['MESSAGES_LOG'],
            '-serial', 'file:' + params['TRACKRSS_LOG'],
        )

    qemu_ram = params['TOTAL_RAM']
    # Set up ELF core headers
    if arch.startswith('s390'):
        S390_OLDMEM_BASE = 0x10418 # cf. struct parmarea
        oldmem = 'oldmem.bin'
        oldmem_size = qemu_ram * 1024
        oldmem_base = oldmem_size
        with open(oldmem, 'wb') as f:
            f.write(oldmem_base.to_bytes(8, 'big'))
            f.write(oldmem_size.to_bytes(8, 'big'))
        extra_qemu_args.extend((
            '-device', 'loader,addr=0x{:x},file={}'.format(
                S390_OLDMEM_BASE, oldmem),
        ))
    else:
        extra_kernel_args.append(
            'elfcorehdr=0x{0:x} crashkernel={1:d}K@0x{0:x}'.format(
                elfcorehdr.address, elfcorehdr.size))

    # Kernel and QEMU arguments to congifure network
    if params['NET']:
        if arch.startswith('s390'):
            model = 'virtio'
        else:
            model = 'e1000e'
        extra_qemu_args.extend(('-nic', 'user,model={}'.format(model)))
        extra_kernel_args.extend(('bootdev=eth0', 'ip=eth0:dhcp'))

    # Other arch-specific arguments
    if arch == 'aarch64':
        extra_qemu_args.extend((
            '-machine', 'virt',
            '-cpu', 'max',
            '-bios', '/usr/share/qemu/qemu-uefi-aarch64.bin',
        ))
    kernel_args = (
        'panic=1',
        'nokaslr',
        'console={}'.format(console),
        'root=kdump',
        'rootflags=bind',
        *extra_kernel_args,
        '--',
        'trackrss={}'.format(logdev),
    )
    qemu_args = (
        qemu_name(arch),
        '-smp', str(params['NUMCPUS']),
        '-no-reboot',
        '-m', '{:d}K'.format(qemu_ram),
        '-display', 'none',
        *console_args,
        '-kernel', params['KERNEL'],
        '-initrd', initrd.path,
        '-append', ' '.join(kernel_args),
        '-device', 'loader,file={},force-raw=on,addr=0x{:x}'.format(
            elfcorehdr.path, elfcorehdr.address),
        *extra_qemu_args,
    )
    subprocess.call(qemu_args)

    results = dict()

    # Get kernel-space requirements
    script = os.path.join(params['SCRIPTDIR'], 'kernel.py')
    with subprocess.Popen(script,
                          stdin=open(params['MESSAGES_LOG']),
                          stdout=subprocess.PIPE) as p:
        for line in p.communicate()[0].decode().splitlines():
            (key, val) = line.strip().split('=')
            results[key] = int(val)

    # Get user-space requirements
    script = os.path.join(params['SCRIPTDIR'], 'maxrss.py')
    with subprocess.Popen(script,
                          stdin=open(params['TRACKRSS_LOG']),
                          stdout=subprocess.PIPE) as p:
        for line in p.communicate()[0].decode().splitlines():
            (key, val) = line.strip().split('=')
            results[key] = int(val)

    kernel_base = params['TOTAL_RAM'] - results['INIT_MEMFREE']
    # The above also includes the unpacked initramfs, which should be separate
    kernel_base -= results['INIT_CACHED']
    # It also should not include the MEMMAP array
    pagesize = results['PAGESIZE']
    pagesize_kb = pagesize // 1024
    numpages = (params['TOTAL_RAM'] + pagesize_kb - 1) // pagesize_kb
    memmap_pages = (numpages * results['SIZEOFPAGE'] + pagesize - 1) // pagesize
    kernel_base -= memmap_pages * pagesize_kb
    results['KERNEL_BASE'] = kernel_base - results['PERCPU']

    results['PERCPU'] = results['PERCPU'] // params['NUMCPUS']

    return results

with subprocess.Popen(('../kdumptool/kdumptool',
                       '-F', os.path.join(params['SCRIPTDIR'], 'dummy.conf'),
                       'find_kernel'),
                      stdout=subprocess.PIPE) as p:
    for line in p.communicate()[0].decode().splitlines():
        (key, val) = line.split(':')
        if key == 'Kernel':
            params['KERNEL'] = val.strip()
if 'KERNEL' not in params:
    print('Cannot determine target kernel', file=sys.stderr)
    exit(1)

with subprocess.Popen(('get_kernel_version', params['KERNEL']),
                      stdout=subprocess.PIPE) as p:
    params['KERNELVER'] = p.communicate()[0].decode().strip()

with tempfile.TemporaryDirectory() as tmpdir:
    oldcwd = os.getcwd()
    try:
        os.chdir(tmpdir)
        elfcorehdr = build_elfcorehdr(oldcwd, ADDR_ELFCOREHDR)

        install_kdump_init(oldcwd)
        init_local_dracut(params)

        params['NET'] = False
        initrd = build_initrd(oldcwd, params, 'dummy.conf')
        results = run_qemu(oldcwd, params, initrd, elfcorehdr)

        params['NET'] = True
        initrd = build_initrd(oldcwd, params, 'dummy-net.conf')
        netresults = run_qemu(oldcwd, params, initrd, elfcorehdr)

    finally:
        os.chdir(oldcwd)

results['INIT_NET'] = netresults['KERNEL_INIT'] - results['KERNEL_INIT']
results['INIT_CACHED_NET'] = netresults['INIT_CACHED'] - results['INIT_CACHED']
results['USER_NET'] = netresults['USER_BASE'] - results['USER_BASE']

keys = (
    'KERNEL_BASE',
    'KERNEL_INIT',
    'INIT_CACHED',
    'PAGESIZE',
    'SIZEOFPAGE',
    'PERCPU',
    'USER_BASE',
    'INIT_NET',
    'INIT_CACHED_NET',
    'USER_NET',
)
for key in keys:
    print('{}={:d}'.format(key, results[key]))
