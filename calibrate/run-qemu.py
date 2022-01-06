#! /usr/bin/python3

import os
import subprocess
import tempfile
import shutil

params = dict()

# Total VM memory in KiB:
params['TOTAL_RAM'] = 1024 * 1024

# Number of CPUs for the VM
params['NUMCPUS'] = 2

# Where kernel messages should go
params['MESSAGES_LOG'] = 'messages.log'

# Where trackrss log should go
params['TRACKRSS_LOG'] = 'trackrss.log'

# initramfs name
params['INITRD'] = 'test-initrd'

# Physical address where elfcorehdr should be loaded.
# This is tricky. The elfcorehdr memory range is removed from the kernel
# memory map with a command line option, but the kernel boot code runs
# before the command line is parsed, and it may overwrite the data.
# The region at 768M should be reasonably safe, because it is high enough
# to avoid conflicts with special-purpose regions and low enough to avoid
# conflicts with allocations at the end of RAM.
ADDR_ELFCOREHDR = 768 * 1024 * 1024

def build_initrd(bindir, initrd, kernelver):
    # First, create the base initrd using dracut:
    env = os.environ.copy()
    env['KDUMP_CONFIGFILE'] = os.path.join(bindir, 'dummy.conf')
    args = (
        'dracut',
        '--hostonly',

        # Standard kdump initrd options:
        '--omit', 'plymouth resume usrmount',
        '--add', 'kdump',

        # Create a simple uncompressed CPIO archive:
        '--no-compress',
        '--no-early-microcode',

        initrd,
        kernelver
    )
    subprocess.call(args, env=env)

    # Replace /init with trackrss:
    trackrss = os.path.join(bindir, 'trackrss')
    shutil.copy(trackrss, './init')
    args =(
        'cpio', '-o',
        '-H', 'newc',
        '--owner=0:0',
        '--append', '--file=' + initrd
    )
    with subprocess.Popen(args, stdin=subprocess.PIPE) as p:
        p.communicate(b'init')

    # Compress the result:
    subprocess.call(('xz', '-0', '--check=crc32', initrd))

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

kernel = None
with subprocess.Popen(('../kdumptool/kdumptool', 'find_kernel'),
                      stdout=subprocess.PIPE) as p:
    for line in p.communicate()[0].decode().splitlines():
        (key, val) = line.split(':')
        if key == 'Kernel':
            kernel = val.strip()
if kernel is None:
    print('Cannot determine target kernel', file=sys.stderr)
    exit(1)

with subprocess.Popen(('get_kernel_version', kernel),
                      stdout=subprocess.PIPE) as p:
    kernelver = p.communicate()[0].decode().strip()

with tempfile.TemporaryDirectory() as tmpdir:
    oldcwd = os.getcwd()
    try:
        os.chdir(tmpdir)
        elfcorehdr = build_elfcorehdr(oldcwd, ADDR_ELFCOREHDR)

        build_initrd(oldcwd, params['INITRD'], kernelver)

        kernel_args = (
            'console=ttyS0',
            'elfcorehdr=0x{0:x} memmap={1:d}K$0x{0:x}'.format(
                elfcorehdr.address, elfcorehdr.size),
            'root=kdump',
            'rootflags=bind',
        )
        args = (
            'qemu-kvm',
            '-smp', str(params['NUMCPUS']),
            '-m', '{:d}K'.format(params['TOTAL_RAM']),
            '-display', 'none',
            '-serial', 'file:' + params['MESSAGES_LOG'],
            '-serial', 'file:' + params['TRACKRSS_LOG'],
            '-kernel', kernel,
            '-initrd', params['INITRD'] + '.xz',
            '-append', ' '.join(kernel_args),
            '-device', 'loader,file={},force-raw=on,addr=0x{:x}'.format(
                elfcorehdr.path, elfcorehdr.address)
        )
        subprocess.call(args)

        # Get kernel-space requirements
        script = os.path.join(oldcwd, 'kernel.py')
        with subprocess.Popen(script,
                              stdin=open(params['MESSAGES_LOG']),
                              stdout=subprocess.PIPE) as p:
            for line in p.communicate()[0].decode().splitlines():
                (key, val) = line.strip().split('=')
                params[key] = int(val)

        # Get user-space requirements
        script = os.path.join(oldcwd, 'maxrss.py')
        with subprocess.Popen(script,
                              stdin=open(params['TRACKRSS_LOG']),
                              stdout=subprocess.PIPE) as p:
            for line in p.communicate()[0].decode().splitlines():
                (key, val) = line.strip().split('=')
                params[key] = int(val)

    finally:
        os.chdir(oldcwd)

kernel_base = params['TOTAL_RAM'] - params['INIT_MEMFREE']
# The above also includes the unpacked initramfs, which should be separate
kernel_base -= params['INIT_CACHED']
# It also should not include the MEMMAP array
pagesize = params['PAGESIZE']
pagesize_kb = pagesize // 1024
numpages = (params['TOTAL_RAM'] + pagesize_kb - 1) // pagesize_kb
memmap_pages = (numpages * params['SIZEOFPAGE'] + pagesize - 1) // pagesize
kernel_base -= memmap_pages * pagesize_kb
params['KERNEL_BASE'] = kernel_base - params['PERCPU']

params['PERCPU'] = params['PERCPU'] // params['NUMCPUS']

keys = (
    'KERNEL_BASE',
    'KERNEL_INIT',
    'INIT_CACHED',
    'PAGESIZE',
    'SIZEOFPAGE',
    'PERCPU',
    'USER_BASE')
for key in keys:
    print('{}={:d}'.format(key, params[key]))
