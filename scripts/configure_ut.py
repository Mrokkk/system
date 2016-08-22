#!/usr/bin/python3

import unittest
import configure
from unittest.mock import MagicMock

class File:

    def readline(self):
        print("called readline")
        return "set mmm x"

    def close(self):
        print("called close")

class ParserTests(unittest.TestCase):

    def setUp(self):
        pass

    def test_reading(self):
        configure.Parser.open_file = MagicMock(return_value=File())
        self.reader = configure.Parser("")
        read = self.reader.read_line()
        self.assertTrue(read == ['set', 'mmm', 'x'])


class InterpreterTests(unittest.TestCase):

    def interpreter_init(self):
        return configure.Interpreter(configure.Parser)

    def setUp(self):
        self.interpreter = self.interpreter_init()

    def tearDown(self):
        self.interpreter.variable_list = None
        self.interpreter = None

    def test_can_set_variable(self):
        self.interpreter.interpret_line(['set', 'VAR', 'myvar'])
        self.assertTrue(self.interpreter.variable_list.get_variable('VAR') == 'myvar')
        keys, values = self.interpreter.variable_list.get()
        self.assertTrue(len(keys) == 1)

    def test_can_set_2_variables(self):
        self.interpreter.interpret_line(['set', 'VAR1', 'y'])
        self.interpreter.interpret_line(['set', 'VAR2', 'n'])
        self.assertTrue(self.interpreter.variable_list.get_variable('VAR1') == 'y')
        self.assertTrue(self.interpreter.variable_list.get_variable('VAR2') == 'n')
        self.assertFalse(self.interpreter.variable_list.get_variable('VAR'))
        keys, values = self.interpreter.variable_list.get()
        self.assertTrue(len(keys) == 2)



def main():
    unittest.main()

if __name__ == '__main__':
    main()

