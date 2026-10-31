#!/usr/bin/env python3
"""Build an X01A Wi-Fi root image without changing the HakiOS source/staging tree."""
import argparse, os, plistlib, shutil, stat, subprocess, sys, tempfile
from pathlib import Path, PurePosixPath

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('hakios', type=Path)
    parser.add_argument('--base-image', type=Path)
    parser.add_argument('--network-file', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    hakios = args.hakios.resolve()
    base = (args.base_image or hakios/'userland/root.apfs').resolve()
    output = (args.output or repo/'out/wifi-root.apfs').resolve()
    if output == base:
        parser.error('output must differ from the base image')
    network = args.network_file.read_bytes()
    lines = network.splitlines()
    if len(lines) != 2 or not 1 <= len(lines[0]) <= 32:
        parser.error('network file must contain SSID and WPA2 passphrase on two lines')
    password = lines[1]
    if not (8 <= len(password) <= 63 or len(password) == 64 and all(c in b'0123456789abcdefABCDEF' for c in password)):
        parser.error('invalid WPA2 passphrase length/format')
    sys.path.insert(0, str(hakios/'tools/apfs'))
    import apfs
    output.parent.mkdir(parents=True, exist_ok=True)
    sdk = Path(os.environ.get('THEOS', str(Path.home()/'theos')))/'sdks/iPhoneOS16.5.sdk'
    compiler = hakios/'xnu-deps/host/bin/xnu-clang'
    env = os.environ.copy()
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
    with tempfile.TemporaryDirectory(prefix='wifi-root-', dir=output.parent) as temporary:
        temp = Path(temporary); root = temp/'root'; root.mkdir()
        env['TMPDIR'] = str(temp)
        metadata = {}
        container = apfs.Container(str(base))
        try:
            volume = apfs.pick_volume(container, None)
            for name, _, fid, _ in apfs.walk(volume, apfs.ROOT_DIR_INO_NUM):
                relative = PurePosixPath(name.lstrip('/'))
                if '..' in relative.parts:
                    raise ValueError('unsafe path in input image')
                path = root.joinpath(*relative.parts)
                inode = volume.inode(fid)
                record = [f'{inode.mode:o}', str(inode.owner), str(inode.group), str(relative)]
                if stat.S_ISDIR(inode.mode):
                    path.mkdir(parents=True, exist_ok=True)
                elif inode.is_link:
                    record.append(inode.contents().decode())
                    # mkapfs can create a symlink directly from the manifest.
                elif inode.is_reg:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(inode.contents())
                else:
                    raise ValueError('unsupported inode type: ' + name)
                metadata[str(relative)] = record
        finally:
            container.close()
        def record(name, mode):
            metadata[name] = [f'{mode:o}', '0', '0', name]
        for name, mode in [('bin', 0o40755), ('LaunchDaemons', 0o40755), ('fw', 0o40755)]:
            (root/name).mkdir(exist_ok=True)
            if name not in metadata: record(name, mode)
        for source, name, extra in [('net_test.m','net_test',['-fobjc-arc','-framework','Foundation','-framework','SystemConfiguration']),('ip_replay.c','ip_replay',[])]:
            subprocess.run([str(compiler),'-arch','arm64','-isysroot',str(sdk),'-O1',*extra,'-o',str(root/'bin'/name),str(repo/'wifi/userland'/source)],check=True,env=env)
            record('bin/'+name,0o100755)
        for source in (repo/'wifi/LaunchDaemons').glob('*.plist'):
            shutil.copyfile(source,root/'LaunchDaemons'/source.name)
            record('LaunchDaemons/'+source.name,0o100644)
        configd = root/'LaunchDaemons/com.apple.configd.plist'
        settings = plistlib.loads(configd.read_bytes())
        program = settings.get('ProgramArguments', ['/usr/libexec/configd'])
        bundle = 'com.apple.SystemConfiguration.CaptiveNetworkSupport'
        if not any(program[i:i+2] == ['-B',bundle] for i in range(len(program)-1)):
            settings['ProgramArguments'] = [*program,'-B',bundle]
        configd.write_bytes(plistlib.dumps(settings,fmt=plistlib.FMT_BINARY))
        (root/'fw/network').write_bytes(network)
        os.chmod(root/'fw/network',0o600)
        record('fw/network',0o100600)
        manifest = temp/'root.manifest'
        manifest.write_text(''.join('\t'.join(metadata[k])+'\n' for k in sorted(metadata)))
        built = temp/'root.apfs'
        subprocess.run([sys.executable,str(hakios/'tools/apfs/mkapfs.py'),'-z','--plain','fw','-o',str(built),f'redwood={root},manifest={manifest}'],check=True)
        os.chmod(built,0o600)
        os.replace(built,output)
    print('Built private Wi-Fi root image:',output)
    print('Contains network credentials; keep local and out of Git.')

if __name__ == '__main__':
    main()
