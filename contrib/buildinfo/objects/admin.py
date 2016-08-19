from django.contrib import admin

from objects.models import Commit, Object, Section, Symbol

class ObjectInline(admin.TabularInline):
    model = Object
    radio_fields = {'type': admin.HORIZONTAL}

class CommitAdmin(admin.ModelAdmin):
    list_display       = ('hash', 'title', 'datetime')
    list_display_links = list_display
    search_fields      = list_display
    date_hierarchy     = 'datetime'
    inlines            = (ObjectInline, )

class SectionInline(admin.TabularInline):
    model = Section

class SymbolInline(admin.TabularInline):
    model        = Symbol
    radio_fields = {'type': admin.HORIZONTAL}

class ObjectAdmin(admin.ModelAdmin):
    list_display       = ('commit', 'target', 'name', 'type', 'size')
    list_display_links = list_display
    search_fields      = ('target', 'name', 'commit__title', 'commit__hash',
                          'type')
    list_filter        = ('type', 'target')
    radio_fields       = {'type': admin.HORIZONTAL}
    inlines            = (SectionInline, SymbolInline)

class SectionAdmin(admin.ModelAdmin):
    list_display       = ('name', 'object', 'size', 'execinstr', 'progbits',
                         'writable')
    list_display_links = list_display
    search_fields      = ('name', 'object__name', 'size')
    list_filter        = ('object__target', 'object__type', 'execinstr',
                          'progbits', 'writable')

class SymbolAdmin(admin.ModelAdmin):
    list_display       = ('name', 'object', 'type', 'size')
    list_display_links = list_display
    search_fields      = ('name', 'object__target', 'object__name', 'type',
                          'size')
    list_filter        = ('type', 'object__target', 'object__type')
    radio_fields       = {'type': admin.HORIZONTAL}


admin.site.register(Commit, CommitAdmin)
admin.site.register(Object, ObjectAdmin)
admin.site.register(Section, SectionAdmin)
admin.site.register(Symbol, SymbolAdmin)

