#! /usr/bin/python3

import sys
import os
import subprocess
import tempfile
import shutil

params = dict()

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

def build_initrd(bindir, params, config, path):
        # First, create the base initrd using dracut:
        env = os.environ.copy()
        env['KDUMP_LIBDIR'] = os.path.abspath(params['SCRIPTDIR'] + "/..")
        env['KDUMP_CONF'] = os.path.join(params['SCRIPTDIR'], config)
        env['DRACUT_PATH'] = ' '.join((
            '/sbin',
            '/bin',
            '/usr/sbin',
            '/usr/bin'))

        drivers = []
        if params['NET']:
            drivers.append('af_packet')
            if params['ARCH'].startswith('s390'):
                drivers.append('virtio-net')
            else:
                drivers.append('e1000e')
            extra_args = []
        else:
            drivers.append('sd_mod')
            drivers.append('virtio_blk')
            drivers.append('ext4')
            extra_args = ('--mount', '/dev/disk/by-label/calib-disk /kdump/mnt ext3')
        args = (
            os.path.abspath('dracut'),
            '--local',
            '--hostonly',
            '--no-hostonly-default-device',

            # Standard kdump initrd options:
            '--omit', 'plymouth resume usrmount',
            '--add', 'kdump',

            # Create a simple uncompressed CPIO archive:
            '--no-compress',
            '--no-early-microcode',
            '--add-drivers', ' '+' '.join(drivers),
            # Additional options:
            *extra_args,

            path,
            params['KERNELVER'],
        )
        subprocess.call(args, env=env, stdout=sys.stderr)

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
        return path + os.path.extsep + 'xz'

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
    elif arch == 'riscv64':
        console = 'ttyS1'
        logdev = '4,66'         # ttyS2
    else:
        console = 'ttyS0'
        logdev = '4,65'         # ttyS1

    if arch == 'aarch64':
        console_args = (
            '-serial', 'null',  # ttyAMA0 (used for OVMF debug messages)
            '-chardev', 'file,path={},id=ttyS0'.format(params['MESSAGES_LOG']),
            '-chardev', 'file,path={},id=ttyS1'.format(params['TRACKRSS_LOG']),
            '-device', 'pci-serial-2x,chardev1=ttyS0,chardev2=ttyS1',
            )
    elif arch.startswith('s390'):
        console_args = (
            '-serial', 'file:' + params['MESSAGES_LOG'],
            '-device', 'virtio-serial-ccw',
            '-chardev', 'file,path={},id=hvc0'.format(params['TRACKRSS_LOG']),
            '-device', 'virtconsole,nr=0,chardev=hvc0',
            )
    elif arch == 'riscv64':
        console_args = (
            '-serial', 'mon:stdio',  # one serial port is hardcoded in the virt machine
            '-chardev', 'file,path={},id=ttyS1'.format(params['MESSAGES_LOG']),
            '-chardev', 'file,path={},id=ttyS2'.format(params['TRACKRSS_LOG']),
            '-device', 'pci-serial-2x,chardev1=ttyS1,chardev2=ttyS2',
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
        # memory size for the crash kernel is defined by oldmem_size;
        # put it in the middle of a doubled qemu memory
        oldmem_size = qemu_ram * 1024;
        oldmem_base = int(qemu_ram * 1024 / 2)
        qemu_ram *= 2
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
        mac = '12:34:56:78:9A:BC'
        extra_qemu_args.extend((
            '-nic', 'user,mac={},model={}'.format(mac, model)
        ))
        extra_kernel_args.extend((
            'ifname=kdump0:{}'.format(mac),
            'ip=kdump0:dhcp',
            'rd.neednet=1'
        ))
    else:
        extra_qemu_args.extend((
            '-drive', 'file=disk.raw,index=0,media=disk,if=virtio',
        ))

    # Other arch-specific arguments
    if arch == 'aarch64':
        extra_qemu_args.extend((
            '-machine', 'virt',
            '-cpu', 'cortex-a57',
            '-bios', '/usr/share/qemu/qemu-uefi-aarch64.bin',
        ))
    if arch == 'x86_64':
        extra_qemu_args.extend((
            '-cpu', 'max',
        ))
    if arch == 'riscv64':
        extra_qemu_args.extend((
            '-machine', 'virt',
        ))

    kernel_args = (
        'panic=1',
        'nokaslr',
        'console={}'.format(console),
        'root=kdump',
        'rootflags=bind',
        'rd.shell=0',
        'rd.emergency=poweroff',
        *extra_kernel_args,
        '--',
        'trackrss={}'.format(logdev),
    )
    qemu_args = (
        qemu_name(arch),
        '-smp', str(params['NUMCPUS']),
        '-no-reboot',
        '-m', '{:d}K'.format(qemu_ram),
        '-nographic',
        *console_args,
        '-kernel', params['KERNEL'],
        '-initrd', initrd,
        '-append', ' '.join(kernel_args),
        '-device', 'loader,file={},force-raw=on,addr=0x{:x}'.format(
            elfcorehdr.path, elfcorehdr.address),
        *extra_qemu_args,
    )

    # create the log files and monitor them with tail -f 
    # (redirected to stderr)
    # for debugging possible problems inside the VM
    f = open(params['MESSAGES_LOG'], "w")
    f.close()
    tail_messages = subprocess.Popen(["tail", "-f", params['MESSAGES_LOG']], stdout=2)
    
    f = open(params['TRACKRSS_LOG'], "w")
    f.close()

    subprocess.run(qemu_args, stdout=sys.stderr, stderr=sys.stderr, check=True)

    tail_messages.kill()
    
    print("trackrss output:", file=sys.stderr)
    subprocess.run(['cat', params['TRACKRSS_LOG']], stdout=2)
    print("(end of trackrss output)", file=sys.stderr)

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

def calc_diff(src, dst, key, diffkey):
    src[diffkey] = max(0, dst[key] - src[key])

def dump_ok(crashdir):
    if not os.path.isdir(crashdir):
        print(crashdir + " does not exist", file=sys.stderr)
        return False

    with os.scandir(crashdir) as it:
        for entry in it:
            if not entry.name.startswith('.') and entry.is_dir():
                print("found dump directory: " + entry.path, file=sys.stderr)
                if not os.path.isfile(os.path.join(entry.path, 'vmcore')):
                    print("vmcore not found", file=sys.stderr)
                    return False
                
                if not os.path.isfile(os.path.join(entry.path, 'README.txt')):
                    print("README.txt not found", file=sys.stderr)
                    return False

                try:
                    f = open(os.path.join(entry.path, 'README.txt'),"r")
                    readme = f.read()
                    if not 'vmcore status: saved successfully' in readme:
                        print("README.txt does not contain vmcore success status", file=sys.stderr)
                        return False
                except:
                    print("can't read README.txt", file=sys.stderr)
                    return False 
                print("vmcore and README.txt check OK", file=sys.stderr)
                return True
    return False

def calibrate_kernel(image):
    params['KERNEL'] = image

    # Physical address where elfcorehdr should be loaded.
    # This is tricky. The elfcorehdr memory range is removed from the kernel
    # memory map with a command line option, but the kernel boot code runs
    # before the command line is parsed, and it may overwrite the data.
    if params['ARCH'] == 'aarch64':
        # QEMU defines all RAM at 1G physical for AArch64
        ADDR_ELFCOREHDR = (1024 * 1024 * 1024) + (256 * 1024 * 1024)
    elif params['ARCH'] == 'riscv64':
        # QEMU defines all RAM at 2G physical for RISC-V
        ADDR_ELFCOREHDR = 0x80000000 + (256 * 1024 * 1024)
    else:
        # For other platforms, the region at 768M should be reasonably safe,
        # because it is high enough to avoid conflicts with special-purpose
        # regions and low enough to avoid conflicts with allocations at the
        # end of RAM.
        ADDR_ELFCOREHDR = 768 * 1024 * 1024

    with subprocess.Popen(('get_kernel_version', params['KERNEL']),
                          stdout=subprocess.PIPE) as p:
        params['KERNELVER'] = p.communicate()[0].decode().strip()

    with tempfile.TemporaryDirectory() as tmpdir:
        oldcwd = os.getcwd()
        os.chdir(tmpdir)
        elfcorehdr = build_elfcorehdr(oldcwd, ADDR_ELFCOREHDR)

        # clean up after previous runs
        if os.path.exists('/root/.ssh/id_ed25519'):
            os.remove('/root/.ssh/id_ed25519')
        if os.path.exists('/root/.ssh/id_ed25519.pub'):
            os.remove('/root/.ssh/id_ed25519.pub')
        subprocess.run(('rm', '-rf', '/tmp/netdump'), stdout=sys.stderr, stderr=sys.stderr, check=False)

        # prepare disk image for saving the non-network dump
        subprocess.run(('dd', 'if=/dev/zero', 'of=disk.raw', 'bs=1', 'seek=200M', 'count=1'), stdout=sys.stderr, stderr=sys.stderr, check=True)
        subprocess.run(('/usr/sbin/mkfs.ext3', '-L', 'calib-disk', 'disk.raw'), stdout=sys.stderr, stderr=sys.stderr, check=True)

        # configure and start ssh server for the network dump
        subprocess.run(('ssh-keygen', '-A'), stdout=sys.stderr, stderr=sys.stderr, check=True)
        subprocess.run(('/usr/sbin/sshd', '-p', '40022'), stdout=sys.stderr, stderr=sys.stderr, check=True)
        subprocess.run(('ssh-keygen', '-f', '/root/.ssh/id_ed25519', '-N', ''), stdout=sys.stderr, stderr=sys.stderr, check=True)
        shutil.copy("/root/.ssh/id_ed25519.pub", "/root/.ssh/authorized_keys")

        install_kdump_init(oldcwd)
        init_local_dracut(params)
        
        params['NET'] = False
        initrd = build_initrd(oldcwd, params, 'dummy.conf', "test-initrd")
        results = run_qemu(oldcwd, params, initrd, elfcorehdr)
        # verify that the dump completed successfully
        os.mkdir('mount')
        subprocess.run(('mount', '-o', 'loop', 'disk.raw', 'mount'), stdout=sys.stderr, stderr=sys.stderr, check=True)
        ret = dump_ok('mount/var/crash')
        subprocess.run(('umount', 'mount'), stdout=sys.stderr, stderr=sys.stderr, check=True)
        if not ret:
            print("non-network dump failed; calibration failed", file=sys.stderr)
            exit(1)
                
        params['NET'] = True
        initrd = build_initrd(oldcwd, params, 'dummy-net.conf', "test-initrd-net")
        os.mkdir('/tmp/netdump')
        netresults = run_qemu(oldcwd, params, initrd, elfcorehdr)
        if not dump_ok('/tmp/netdump'):
            print("network dump failed; calibration failed", file=sys.stderr)
            exit(1)

        os.chdir(oldcwd)

    calc_diff(results, netresults, 'KERNEL_INIT', 'INIT_NET')
    calc_diff(results, netresults, 'INIT_CACHED', 'INIT_CACHED_NET')
    calc_diff(results, netresults, 'USER_BASE', 'USER_NET')

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

################################################
# main program

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
arch = os.uname()[4]
params['ARCH'] = os.uname()[4]

if arch == "i386" or arch == "i586" or arch == "i686" or arch == "x86_64":
    image="vmlinuz"
elif arch.startswith("s390"):
    image="image"
elif arch == "aarch64" or arch == "riscv64":
    image="Image"
else:
    image="vmlinux"

calibrate_kernel("/boot/"+image)

# vim: set et ts=4 sw=4 :
