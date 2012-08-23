
from distutils.core import setup, Extension

module1 = Extension('sockios',
                    sources = ['sockios.c'])

setup (name = 'sockios',
       version = '0.1',
       description = 'Provides access to SIO* ioctl()s.',
       ext_modules = [module1])

