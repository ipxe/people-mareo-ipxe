from django.views.generic import TemplateView, ListView

from objects.models import Commit, Object

class IndexView(TemplateView):
    template_name = 'objects/index.html'


    def get_context_data(self, *args, **kwargs):
        c = super(IndexView, self).get_context_data(*args, **kwargs)

        c['object_list'] = []

        for name in ('ipxe.pxe', 'ipxe.lkrn', 'undionly.kpxe', '80861209.rom' ):
            c['object_list'].append({
                'title': 'bin/%s' % name,
                'name': '%s.tmp' % name,
                'target': 'bin',
            })

        return c

class DataView(ListView):
    template_name = 'objects/data.json'
    model         = Object

    def dispatch(self, *args, **kwargs):
        self.target = kwargs['target']
        self.name   = kwargs['name']

        return super(DataView, self).dispatch(*args, **kwargs)

    def get_queryset(self):
        return Object.objects.filter(
            target=self.target,
            name=self.name
        ).order_by().order_by('commit__datetime')

