'''
This takes Python __doc__ strings and writes them out in C++ in a way suitable
for pybind11.
 
Created on 23 Feb 2018

@author: paulross
'''
import builtins
import datetime
import importlib
import inspect
import os
import re
import sys

#: Builtin types to ignore. Retain tyoe type to get our class docstrings.
builtins_special = [_b[1] for _b in inspect.getmembers(builtins) if _b[1] != type]
#: Unique delimiter for string literal names.
cpp_str_delim = 'doc_from_python'
#: Detect special method names.
RE_MAGIC_METH = re.compile(r'__.+?__')

def print_doc(doc_str, *args):
    """Print out the documentation string as a string literal named from args."""
    print('const char *DOCSTRING_{} = R"{}({}'.format('_'.join(args),
                                                      cpp_str_delim,
                                                      doc_str))
    print('){}";'.format(cpp_str_delim))
    print()

def get_doc(obj):
    """Returns the documentation or None. Ignore imported modules."""
    if type(obj) not in builtins_special and str(type(obj)) != "<class 'module'>":
        return getattr(obj, '__doc__', None) 

def print_member_doc(name, obj):
    """Recursively print documentation strings.
    Returns the number of strings written out."""
    num = 0
    for member_name, member_obj in sorted(inspect.getmembers(obj)):
        doc = get_doc(member_obj)
        if doc is not None:
            print_doc(doc, name, member_name)
            num += 1
            if RE_MAGIC_METH.match(member_name) is None:
                num += print_member_doc('{}_{}'.format(name, member_name), member_obj)
    return num

def main():
    """Extracts documentation strings from the Python module given as an
    argument and writes them as C++ literal strings to stdout."""
    mod_path = sys.argv[1]
    sys.path.append(os.path.dirname(mod_path))
    mod_name = os.path.splitext(os.path.basename(mod_path))[0]
    mod = importlib.import_module(mod_name)
    print('// Auto generated documentation strings from modue {}'.format(mod_name))
    print('// Generated by {}'.format(__file__))
    print('// Generated on {}'.format(datetime.datetime.now().strftime('%c')))
    print()
    doc_count = 0
    doc = getattr(mod, '__doc__', None)
    if doc is not None:
        print_doc(doc, mod_name)
        doc_count += 1
    doc_count += print_member_doc(mod_name, mod)
    print('// Completed {:d} documentation strings from modue {}'.format(doc_count, mod_name))
    return 0

if __name__ == '__main__':
    sys.exit(main())