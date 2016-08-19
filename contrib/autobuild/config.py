#!/usr/bin/env python

import re

from collections import namedtuple


class ConfigFile(object):
    _SETTING_KEYWORD_RE = r'^\s*(?P<keyword>(?://)?#(?:undef|define))'
    _SETTING_NAME_RE = r'\s+(?P<name>\S+)'
    _SETTING_VALUE_RE = r'(?:\s+(?P<value>\S(?:.(?<!/\*))+))?'
    _SETTING_COMMENT_RE = r'(?:\s+/\*\s*(?P<comment>.+?)\s*\*/)?\s*$'

    _SETTING_RE = ''.join((_SETTING_KEYWORD_RE, _SETTING_NAME_RE,
                           _SETTING_VALUE_RE, _SETTING_COMMENT_RE))

    _HEADER_BODY_RE = (r'\s*?(?:/|\s)(?:\*+)(?:/|(?:\s*'
                       '(?P<body>(?:(?!\s\*/).)+)\s)?)')

    setting_re = re.compile(_SETTING_RE)
    header_start_re = re.compile(r'^\s*?/\*+')
    header_body_re = re.compile(_HEADER_BODY_RE)
    header_end_re = re.compile(r'\*/')

    SettingGroup = namedtuple('SettingGroup', ('header', 'settings'))
    Setting = namedtuple('Setting', ('keyword', 'name', 'value', 'comment',
                                    'set'))

    def __init__(self, stream, filename=None, configname=None):
        self.stream = stream
        self.filename = filename
        self.configname = configname
        self._changed = {}

        self._parse()

    def setting_groups(self):
        return list(self._setting_groups)

    def get(self, name):
        for header, settings in self._setting_groups:
            for setting in settings:
                if setting.name == name:
                    return setting

        return None


    def set(self, setting, newvalue=None):
        if setting.value == newvalue and setting.set:
            if setting in self._changed:
                del self._changed[setting]
        else:
            self._changed[setting] = self.Setting(
                set=True,
                keyword='#define',
                name=setting.name,
                value=newvalue,
                comment=setting.comment,
            )

    def unset(self, setting):
        if not setting.set:
            if setting in self._changed:
                del self._changed[setting]
        else:
            self._changed[setting] = self.Setting(
                set=True,
                keyword='#undef',
                name=setting.name,
                value=None,
                comment=setting.comment,
            )

    def write_changed(self, stream):
        for header, settings in self._setting_groups:
            for setting in settings:
                if setting in self._changed:
                    s = self._changed[setting]
                    value = s.value if s.value else ''
                    comment = '/* ' + s.comment + ' */' if s.comment else ''
                    stream.write(
                        "{}\t{}\t{}\t{}\n".format(
                            s.keyword,
                            s.name,
                            value,
                            comment,
                        ),
                    )

    def _parse(self):
        in_header = False

        result = []
        header = []
        header_current_line = []
        settings = []

        for line in self.stream:
            if line == '\n':
                continue

            if not in_header:
                if self.header_start_re.match(line):
                    result.append(self.SettingGroup(
                        header=header,
                        settings=settings)
                    )
                    in_header = True
                    header = []
                    header_current_line = []
                    settings = []
                elif result:
                    m = self.setting_re.match(line)
                    if not m:
                        continue

                    setting = {
                        g: m.group(g) for g in ('keyword', 'name', 'value',
                                                'comment')
                    }

                    if setting['keyword'] in ('//#undef', '#define'):
                        setting['set'] = True
                    elif setting['keyword'] in ('//#define', '#undef'):
                        setting['set'] = False
                    else:
                        setting['set'] = None

                    settings.append(self.Setting(**setting))

            if in_header:
                m = self.header_body_re.search(line)
                if (not m or m.group('body') == None) and header_current_line:
                    header.append(' '.join(header_current_line))
                    header_current_line = []
                elif m and m.group('body') not in (None, ''):
                    header_current_line.append(m.group('body'))

                if self.header_end_re.search(line):
                    if header_current_line:
                        header.append(' '.join(header_current_line))
                    in_header = False

        if header or settings:
            result.append(self.SettingGroup(
                header=header,
                settings=settings)
            )

        self._setting_groups = result[1:]
