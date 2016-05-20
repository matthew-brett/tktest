import sys
import os
from os.path import join as pjoin, exists, dirname

# BEFORE importing distutils, remove MANIFEST. distutils doesn't properly
# update it when the contents of directories change.
if exists('MANIFEST'): os.remove('MANIFEST')

from setuptools import setup, Extension
from Cython.Build import cythonize


libs = ['psapi'] if sys.platform == 'win32' else []
ext = Extension('tktest',
                 ['tktest.pyx', 'find_tk.c'],
                 libraries = libs)
from setupext import BackendTkAgg
BackendTkAgg().add_flags(ext)
ext_modules = cythonize([ext], include_path=['include'])

setup(
    version="0.1",
    description='Test finding Tk modules',
    name = 'tktest',
    ext_modules = ext_modules,
    author='Matthew Brett',
    author_email='matthew.brett@gmail.com',
    maintainer='Matthew Brett',
    maintainer_email='matthew.brett@gmail.com',
    url='http://github.com/matthew-brett/testk',
    license='BSD license',
    classifiers = [
        'Development Status :: 3 - Alpha',
        'Environment :: Console',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX',
        'Operating System :: Unix',
        'Operating System :: MacOS',
    ],
    long_description = open('README.rst', 'rt').read(),
    setup_requires=['cython'],
)
