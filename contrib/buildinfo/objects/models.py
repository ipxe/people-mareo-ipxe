from django.db import models
from django.db.models import Sum
from django.contrib.auth.models import User

class Commit(models.Model):
    datetime = models.DateTimeField()
    title    = models.TextField()
    hash     = models.CharField(max_length=128, unique=True)

    class Meta:
        ordering = ('-datetime', )

    def __str__(self):
        return '%s: %s' % (self.hash[:6], self.title)

    def net_protocols(self):
        return self.object_set.filter(
            section__name__startswith=('.tbl'),
            section__name__contains=('_protocols'),
        )

    def drivers(self):
        return self.object_set.filter(
            section__name__startswith=('.tbl'),
            section__name__contains=('_drivers'),
        ).exclude(
            pk__in=self.net_protocols(),
        )

    def images(self):
        return self.object_set.filter(
            section__name__startswith=('.tbl.image_types'),
        ).exclude(
            pk__in=self.net_protocols(),
        ).exclude(
            pk__in=self.drivers(),
        )

    def others(self):
        self.object_set.exclude(
            pk__in=self.drivers(),
        ).exclude(
            pk__in=self.net_protocols(),
        ).exclude(
            pk__in=self.images(),
        )


class Object(models.Model):
    TYPE_CHOICES = (
        (0, 'Executable'),
        (1, 'Object File'),
        (2, 'Debug'),
    )

    commit = models.ForeignKey(Commit)
    name   = models.CharField(max_length=128)
    type   = models.PositiveSmallIntegerField(choices=TYPE_CHOICES)
    target = models.CharField(max_length=128)

    class Meta:
        unique_together = (('commit', 'target', 'name'), )
        ordering        = ('commit', 'target', 'name')

    def __str__(self):
        return '%s/%s' % (self.target, self.name)

    def size(self):
        return self.section_set.filter(
            progbits=True,
        ).aggregate(
            Sum('size'),
        )['size__sum'] or 0

    def text_size(self):
        return self.section_set.filter(
            execinstr=True,
        ).aggregate(
            Sum('size'),
        )['size__sum'] or 0

    def data_size(self):
        return self.section_set.filter(
            execinstr=False,
            progbits=True,
        ).aggregate(
            Sum('size'),
        )['size__sum'] or 0

    def bss_size(self):
        return self.section_set.filter(
            execinstr=False,
            progbits=False,
        ).aggregate(
            Sum('size'),
        )['size__sum'] or 0


class Section(models.Model):
    object    = models.ForeignKey(Object)
    name      = models.CharField(max_length=128)
    size      = models.PositiveIntegerField()
    execinstr = models.BooleanField()
    progbits  = models.BooleanField()
    writable  = models.BooleanField()

    class Meta:
        ordering = ('object', 'name')

    def __str__(self):
        return '%s - %s' % (self.object, self.name)

class Symbol(models.Model):
    TYPE_CHOICES = (
        (0, 'Function'),
        (1, 'Object'),
        (2, 'Reference'),
    )

    object = models.ForeignKey(Object)
    name   = models.CharField(max_length=128)
    type   = models.PositiveSmallIntegerField(choices=TYPE_CHOICES)
    size   = models.PositiveIntegerField()

    class Meta:
        ordering = ('object', 'type', 'name')

    def __str__(self):
        return '%s - %s' % (self.object, self.name)
