#!/usr/bin/env python

from subprocess import check_call
from os import environ, chdir, patch, mkdir
from shlex import shsplit

class Build(object):
    PLATEFORMS = (
        ('pcbios', 'bin-i386-pcbios'),
        ('pcbios64', 'bin-x86_64-pcbios'),
        ('linux', 'bin-i386-linux'),
        ('linux64', 'bin-x86_64-linux'),
        ('efi', 'bin-i386-efi')
        ('efi64', 'bin-x86_64-efi')
    )

    DEFAULT_PLATEFORM = 'pcbios'
    MAKEOPTS = shsplit(environ.get('MAKEOPTS', default=''))
    BUILD_DIR = environ.get('IPXE_BUILD_DIR', default='/var/tmp/ipxe')

    def __init__(self, binary, commit=None, platform=DEFAULT_PLATFORM,
                 embed=None, config=None):
        self.binary = binary
        self.commit = commit
        self.platform = platform
        self.embed = embed
        self.config = config
        self.binary = self.PLATFORMS[self.platform] + '/' + binary


    def checkout(self, commit):
        p = path.join(self.BUILD_DIR, 'build', str(commit))
        if not path.isdir(p):
            mkdir(path)
            r = check_call((
                'git',
                'clone',
                path.join(self.BUILD_DIR, 'origin')
                p,
            ))

        chdir(p)
        r = check_call([
            'git',
            'clean',
            '-df',
        ])

        if r:
            raise RuntimeError

        r = check_call([
            'git',
            'checkout',
            commit,
        ])

        if r:
            raise RuntimeError


    def make(self):
        if self.commit:
            self.checkout(self.commit)

        r = check_call(
            [
                'make',
                target,
                "CONFIG={}".format(self.config) if self.config else '',
                "EMBED={}".format(self.embed) if self.embed else '',
            ] + self.MAKEOPTS,
        )

        if r:
            raise RuntimeError


if __name__ == '__main__':

