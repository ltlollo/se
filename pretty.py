# TODO: eval 'doc', the distinguish extern/intern lines

class LinePrinter(object):
    "Print a struct line_metadata"
    def __init__(self, val):
        self.val = val
    def to_string(self):
        size  = self.val['size']
        alloc = self.val['alloc']
        mstr  = self.val['intern_line']
        return 'struct line {\n\tsize  : %lu\n\talloc : %lu\n\tstr   : "%s"\n}' % (size, alloc, mstr.string('utf-8', 'backslashreplace', size))
    def display_hint(self):
        return 'line_metadata'


import gdb.printing
pp = gdb.printing.RegexpCollectionPrettyPrinter('line_metadata')
pp.add_printer('line', '^line$', LinePrinter)
gdb.printing.register_pretty_printer(gdb.current_objfile(), pp)

