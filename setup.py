from distutils.core import setup, Extension

exactcover = Extension('exactcover', sources=['exactcover.c'])

setup(name='ExactCover',
      version='0.1',
      ext_modules = [exactcover])
