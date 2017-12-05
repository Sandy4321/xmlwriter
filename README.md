# Table of contents
1. [Introduction](#Introduction)
2. [Discoveries](#Discoveries)
    1. [Build Time](<a name="Build_Time"></a>)
    2. [Passing Python File Objects into C++](<a name="Passing_Python_File_Objects_into_C++"></a>)
3. [Results](#Results)
    1. [The ``pybind11`` Project](The_pybind11_Project)
    2. [Development Time](Development_Time)
    3. [Build System](Build_System)

# Introduction <a name="Introduction"></a>

A Python XML/HTML/SVG writer originally implemented in Python and now migrated to C++ with [pybind11](https://github.com/pybind/pybind11).

The aim of this project was:

* To get familiar with the nuts and bolts of a pybind11 project.
* See how long the migration would take.
* Get a feel for the build system.
* Have a look at what the final code looked like and how maintainable it would be.
* Measure the relative performance of the Python and pybind11 code.
* Have a look at the auto-generated documentation by pybind11.
* To create a faster XML/XHTML/SVG reader, the pure Python implementation was presumed to be slow in other projects (``cpip``, ``TotalDepth`` etc.).

Project is based on the [pybind11 example](https://github.com/pybind/python_example).

# Discoveries <a name="Discoveries"></a>

## Build Time <a name="Build_Time"></a>

The absolute minimal example takes a significant time to build compared to a minimal CPython extension, seven seconds compared to sub-second build time. Xcode is much faster, possibly multiprocessing is at work here.

## Passing Python File Objects into C++

The original `XmlWrite.XmlStream` took as the first argument a string or file like object. If the former it was treated as the file path to write to. I havn't been able to reproduce this pattern by using a `PyObject*` and deciding within the constructor what to do:

```c
    py::class_<XmlStream>(m, "XmlStream")
        .def(py::init<PyObject*, const std::string &, const std::string &, int, bool>());
```

This complains with:

```python
>>> s= "test.xml"
>>> xs = cXmlWrite.XmlStream(s, "utf-8", "", 0, True)
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
TypeError: __init__(): incompatible constructor arguments. The following argument types are supported:
    1. cXmlWrite.XmlStream(arg0: _object, arg1: str, arg2: str, arg3: int, arg4: bool)

Invoked with: 'test.xml', 'utf-8', '', 0, True
```

I can not find a way to map Python's file object to an internal C++ stream: http://pybind11.readthedocs.io/en/stable/advanced/pycpp/object.html

Rewrote both Python and C++ code to write to an internal buffer, the caller can retrieve this and write it to file.

# Results <a name="Results"/a>

## The ``pybind11`` Project <a name="The_pybind11_Project"/a>

This project is based on the [pybind11 example](https://github.com/pybind/python_example), it can also build under Xcode which is useful for error checking.

A degree of effort went into separating pure C++ code from CPython and pybind11 code. The intention was that code in the ``cpp/`` directory was independent of CPython and pybind11, in practice pybind11 headers were needs to provide ``*args`` for the ``__exit__`` methods. Perhaps there is a workaround for this.

## Development Time <a name="Development_Time"/a>

About 2 to 3 days of work. This should be reduced in future projects as there was a fair amount of fiddling around understanding pybind11 corner cases, such as having to use a lambda to convert a ``std::string`` to a bytes object.

This development time would be what I would expect for a 'C' extension (but see [code maintainability](Code_Maintainability) below.

## Build System <a name="Build_System"/a>

This is pretty good. It is worth creating the project to build under Xcode as this is faster to build and find compile/link time bugs.

## Code Maintainability <a name="Code_Maintainability"/a>

This is rather nice. ``XmlWrite.cpp`` is written in C++11 and reads almost like Python. It is 284 lines long compared to the original pure Python implementation of around 300 lines (if you ignore the documentation strings). Of course you have the header file ``XmlWrite.h`` (145 lines) but this is pretty simple and generally does not change much over time. There is some additional code to do base64 encoding/decoding that C++ needs, Python does not as it is in the standard library which I am ignoring here.

Then there is the ``cXmlWrite.cpp`` file that is the pybind11 interface, this is about 120 lines (ignoring documentation strings). Of course it is written in pybind11 style that takes a little getting used to but is perfectly readable.

One notable feature of pybind11 is that if you get it wrong the compilation messages come from template meta-programming and are pretty obscure and you can take a long time to track the problem down. This glue code is definitely better than normal C Extension code or optimised Cython code.

## Performance

This is pretty disappointing. The pybind11 code is consistently faster at writing documents but not by much. Typically the pybind11 module ``cXmlWrite`` takes about 65% to 75% of the time of the pure Python module ``XmlWrite``. This performance is not worth moving away from the convenience of writing in pure Python.

For base64 encoding the pybind11 code is actually slower, taking more than twice as long.

I think that the reasons for this disappointment are these:

* The design of the XML writer required the construction and destruction of many small objects and perhaps pybind11 does not shine here. To be fair any C/C++ solution would, most likely, behave the same way.
* The Python standard library for base64 is written directly in C so one would not expect an improvement when going through pybind11's machinery.

It just goes to show how careful you must be in your choice before you set out to migrate to pybind11.

It might be worth trying pybind11 out on TotalDepth's LIS file indexer. This spends more time in C/C++ land so should show an great improvement. Also we have a C reference implementation that is 100x faster than the pure Python one so we could compare pybind11 with that.

### Actual Benchmarks

```
---------------------------------------------------------------------------------------------------------- benchmark: 14 tests -----------------------------------------------------------------------------------------------------------
Name (time in us)                                Min                     Max                    Mean                 StdDev                  Median                    IQR            Outliers           OPS            Rounds  Iterations
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
test_XmlWrite_create_stream                   2.7469 (1.0)        1,385.3502 (20.96)          2.9812 (1.0)           4.9858 (3.88)           2.8303 (1.0)           0.0517 (1.0)      242;8264  335,439.7985 (1.0)       82726           1
test_XmlWrite_encode_text                     4.0047 (1.46)          66.0908 (1.0)            4.4022 (1.48)          1.2847 (1.0)            4.2454 (1.50)          0.1220 (2.36)     988;2692  227,157.5821 (0.68)      34984           1
test_cXmlWrite_create_stream                  4.0387 (1.47)         719.7023 (10.89)          4.7976 (1.61)          6.5002 (5.06)           4.3088 (1.52)          0.2328 (4.50)     363;3565  208,436.8944 (0.62)      38271           1
test_XmlWrite_decode_text                     4.1472 (1.51)         166.3840 (2.52)           4.5389 (1.52)          1.9878 (1.55)           4.3851 (1.55)          0.1388 (2.68)     847;4309  220,315.4754 (0.66)      65691           1
test_cXmlWrite_encode_text                    9.0417 (3.29)       3,529.4658 (53.40)         10.8616 (3.64)         17.8424 (13.89)          9.2420 (3.27)          0.3050 (5.90)    611;12664   92,067.3021 (0.27)      58513           1
test_cXmlWrite_decode_text                   11.5791 (4.22)       1,964.8140 (29.73)         12.8903 (4.32)         13.4467 (10.47)         11.7938 (4.17)          0.1159 (2.24)    1057;9561   77,577.9732 (0.23)      62090           1
test_cXmlWrite_two_elements                  11.6061 (4.23)       1,086.9242 (16.45)         13.0743 (4.39)         13.4104 (10.44)         12.4397 (4.40)          0.7162 (13.86)    220;1597   76,485.7586 (0.23)      20968           1
test_XmlWrite_two_elements                   17.4632 (6.36)       2,073.3713 (31.37)         19.9164 (6.68)         22.8331 (17.77)         17.9070 (6.33)          0.2342 (4.53)     318;3689   50,209.9564 (0.15)      21619           1
test_cXmlWrite_small_XHTML_doc            1,440.9530 (524.57)     4,770.5336 (72.18)      1,596.3809 (535.49)      325.5815 (253.42)     1,512.8399 (534.52)       74.3284 (>1000.0)     32;86      626.4169 (0.00)        630           1
test_XmlWrite_small_XHTML_doc             1,932.6438 (703.56)     5,513.7030 (83.43)      2,144.2330 (719.26)      461.2596 (359.03)     1,967.9968 (695.33)      151.9362 (>1000.0)     29;42      466.3672 (0.00)        363           1
test_cXmlWrite_large_XHTML_doc           25,484.2401 (>1000.0)   40,995.9340 (620.30)    27,238.3079 (>1000.0)   2,740.6273 (>1000.0)   26,479.8598 (>1000.0)     897.0209 (>1000.0)       3;4       36.7130 (0.00)         35           1
test_XmlWrite_large_XHTML_doc            32,379.7818 (>1000.0)   79,111.7516 (>1000.0)   40,205.0592 (>1000.0)  11,287.6896 (>1000.0)   33,773.3161 (>1000.0)  12,658.6971 (>1000.0)       5;1       24.8725 (0.00)         29           1
test_cXmlWrite_very_large_XHTML_doc     348,854.7760 (>1000.0)  358,303.2009 (>1000.0)  353,562.9081 (>1000.0)   4,434.5642 (>1000.0)  351,852.7462 (>1000.0)   8,141.6880 (>1000.0)       3;0        2.8284 (0.00)          5           1
test_XmlWrite_very_large_XHTML_doc      438,965.4738 (>1000.0)  468,747.6489 (>1000.0)  445,983.4165 (>1000.0)  12,760.6729 (>1000.0)  440,929.3109 (>1000.0)   8,593.7604 (>1000.0)       1;1        2.2422 (0.00)          5           1
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
```

## Documentation

I got this started with commit b41afc0414a7157c1dbc42819181b1cb9b9b0fad. It is a bit fiddly to set up. Migrating the documentation across from Python strings to C strings is a bit tedious but perhaps this could be automated. Possibly the C documentation strings should go in their own header file to reduce the clutter.

The nice thing is that type annotations are generated in the documentation automatically from the C++ code.

One noticeable thing was the slow turnaround when editing the documentation, you have to edit in C++, build then ``make html``.

# Conclusions




# Boilerplate Footnotes

## Installation


**On Unix (Linux, OS X)**

 - clone this repository
 - `pip install ./xmlwriter`

**On Windows (Requires Visual Studio 2015)**

 - For Python 3.5:
     - clone this repository
     - `pip install ./xmlwriter`
 - For earlier versions of Python, including Python 2.7:

   Pybind11 requires a C++11 compliant compiler (i.e. Visual Studio 2015 on
   Windows). Running a regular `pip install` command will detect the version
   of the compiler used to build Python and attempt to build the extension
   with it. We must force the use of Visual Studio 2015.

     - clone this repository
     - `"%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64`
     - `set DISTUTILS_USE_SDK=1`
     - `set MSSdk=1`
     - `pip install ./xmlwriter`

   Note that this requires the user building `xmlwriter` to have registry edition
   rights on the machine, to be able to run the `vcvarsall.bat` script.


## Windows runtime requirements

On Windows, the Visual C++ 2015 redistributable packages are a runtime
requirement for this project. It can be found [here](https://www.microsoft.com/en-us/download/details.aspx?id=48145).

If you use the Anaconda python distribution, you may require the Visual Studio
runtime as a platform-dependent runtime requirement for you package:

```yaml
requirements:
  build:
    - python
    - setuptools
    - pybind11

  run:
   - python
   - vs2015_runtime  # [win]
```


## Building the documentation

Documentation for the example project is generated using Sphinx. Sphinx has the
ability to automatically inspect the signatures and documentation strings in
the extension module to generate beautiful documentation in a variety formats.
The following command generates HTML-based reference documentation; for other
formats please refer to the Sphinx manual:

 - `cd xmlwriter/docs`
 - `make html`

## License

pybind11 is provided under a BSD-style license that can be found in the LICENSE
file. By using, distributing, or contributing to this project, you agree to the
terms and conditions of this license.

## Test call

```python
import cXmlWrite
```
