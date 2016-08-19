from django.conf.urls import patterns, url

from objects.views import IndexView, DataView

urlpatterns = patterns('',
        url('^$', IndexView.as_view(), name='index'),
        url('^data/(?P<target>\S+)/(?P<name>\S+).json$',
            DataView.as_view(), name='data'),
)
