#!/usr/bin/env python

# This is only the automatic generated test file for ../perspective.py
# This must be filled with real tests and this commentary
# must be cleared.
# If you want to help, read the python unittest documentation:
# http://docs.python.org/library/unittest.html

import sys
sys.path.append('..') # this line allows to import the extension code

import unittest
from perspective import *

class ProjectBasicTest(unittest.TestCase):

  #def setUp(self):

  def test_run_without_parameters(self):
    args = [ 'minimal-blank.svg' ]
    e = Project()
    e.affect( args, False )
    #self.assertEqual( e.something, 'some value', 'A commentary about that.' )

if __name__ == '__main__':
  unittest.main()
