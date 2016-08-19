from django.conf.urls import patterns, include, url
import objects.urls

from django.contrib import admin
admin.autodiscover()

urlpatterns = patterns('',
    url(r'^admin/', include(admin.site.urls)),
    url(r'^', include(objects.urls)),
)
