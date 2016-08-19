from django.core.management.base import BaseCommand, CommandError

from objects.models import Commit, Object, Section, Symbol

from os import listdir
from os.path import isfile, isdir, join, basename, normpath
from subprocess import check_output, check_call
from dateutil import parser as dtparser

from elftools.elf.elffile import ELFFile
from elftools.elf.constants import SH_FLAGS
from elftools.elf.sections import SymbolTableSection

class Command(BaseCommand):
    TARGETS = (
        'bin',
        #'bin-i386-efi',
        #'bin-x86_64-efi',
        #'bin-i386-linux',
        #'bin-x86_64-linux',
    )

    args = '<path>'
    help = 'Analyse objects file in <path>'

    def handle(self, *args, **options):
        if not args:
            raise CommandError('missing parameter %s' % self.args)

        path = normpath(args[0])
        if not isdir(path):
            raise CommandError('not a directory: %s' % path)

        check_call(
            'git -C "%s" fetch origin' % path,
            shell=True,
        )


        commits = check_output(
            'git -C "%s" rev-list HEAD..origin/master' % path,
            shell=True,
        ).decode('ascii').split()

        for commit in reversed(commits):
            check_call(
                'git -C "%s" checkout %s' % (path, commit),
                shell=True,
            )

            self.analyse(path, commit)

            check_call(
                'git -C "%s" checkout master' % path,
                shell=True,
            )

            check_call(
                'git -C "%s" merge --ff-only %s' % (path, commit),
                shell=True,
            )

    def analyse(self, path, commithash):
        title = check_output(
            'git -C "%s" show -s --format=%%s %s' % (path, commithash),
            shell=True,
        ).decode('utf-8').strip()

        self.stdout.write(
            'Analysing commmit %s: %s...' % (commithash[:8], title)
        )

        commitdate = dtparser.parse(check_output(
            'git -C "%s" show -s --format=%%ci %s' % (path, commithash),
            shell=True,
        ))

        commit, created = Commit.objects.get_or_create(
            hash=commithash,
            defaults={
                'title': title,
                'datetime': commitdate,
            },
        )

        check_call(
            'make -C "%s" clean' % path,
            shell=True
        )

        check_call(
            'make -C "%s" -j 10 all' % path,
            shell=True
        )

        if not created:
            Object.objects.filter(commit=commit).delete()

        for path in [normpath(join(path, d)) for d in self.TARGETS]:
            if not isdir(path):
                self.stderr.write('not a directory: %s' % path)
                continue

            target = basename(path)

            for f in listdir(path):
                fpath = join(path, f)
                if not isfile(fpath):
                    continue

                if fpath[-2:] == '.o':
                    ftype = 2 if fpath[-6:-3] == 'dbg' else 1
                elif fpath[-4:] == '.tmp':
                    ftype = 0
                else:
                    continue

                with open(fpath, 'rb') as f:
                    try:
                        elf = ELFFile(f)
                    except:
                        print('Error opening %s' % fpath)
                        continue

                    obj = Object(
                        commit=commit,
                        name=basename(f.name),
                        type=ftype,
                        target=target,
                    )
                    obj.save()


                    sections = []
                    symbols  = []

                    for section in elf.iter_sections():
                        name  = section.name.decode('ascii')
                        size  = section['sh_size']
                        flags = section['sh_flags']
                        type  = section['sh_type']

                        if flags & SH_FLAGS.SHF_ALLOC and size > 0:
                            sections.append(Section(
                                object=obj,
                                name=name,
                                size=size,
                                execinstr=bool(flags & SH_FLAGS.SHF_EXECINSTR),
                                progbits=bool(type == 'SHT_PROGBITS'),
                                writable=bool(flags & SH_FLAGS.SHF_WRITE),
                            ))

                        if isinstance(section, SymbolTableSection) and \
                           fpath[-2:] == '.o':
                            for symbol in section.iter_symbols():
                                sname = symbol.name.decode('ascii')
                                ssize = symbol['st_size']
                                stype = symbol['st_info']['type']
                                sbind = symbol['st_info']['bind']

                                if stype == 'STT_FUNC' and ssize > 0:
                                    stype = 0
                                elif stype == 'STT_OBJECT' and ssize > 0:
                                    stype = 1
                                elif (stype, sbind) == \
                                     ('STT_NOTYPE', 'STB_GLOBAL'):
                                    stype = 2
                                else:
                                    continue

                                symbols.append(Symbol(
                                    object=obj,
                                    type=stype,
                                    name=sname,
                                    size=ssize,
                                ))

                    Section.objects.bulk_create(sections)
                    Symbol.objects.bulk_create(symbols)

