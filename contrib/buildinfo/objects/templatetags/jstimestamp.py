from django import template

register = template.Library()

@register.filter
def to_jstimestamp(datetime):
    return str(int(datetime.strftime("%s")) * 1000)
