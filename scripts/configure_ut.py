#!/usr/bin/python3

import unittest
import configure
from unittest.mock import MagicMock, patch


class ParserTests(unittest.TestCase):

    def setUp(self):
        self.parser = configure.Parser()

    def tearDown(self):
        del self.parser

    def test_can_parse(self):
        parsed = self.parser.parse_line('python is OK')
        self.assertEqual(parsed, ['python', 'is', 'OK'])

    def test_can_parse_null(self):
        parsed = self.parser.parse_line('')
        self.assertIsNone(parsed)

    def test_can_parse_quotes(self):
        parsed = self.parser.parse_line('test "multiple phrases" "on parser"')
        self.assertEqual(parsed, ['test', 'multiple phrases', 'on parser'])

    def test_can_handle_nonclosed_quotes(self):
        parsed = self.parser.parse_line('test "multiple phrases on parser')
        self.assertEqual(parsed, ['test', 'multiple phrases on parser'])


class ReaderStub:

    def __init__(self, filename):
        self.num = 0

    def readline(self):
        if self.num == 0:
            self.num += 1
            return "something"
        else:
            self.num = 0


class InterpreterTests(unittest.TestCase):

    def setUp(self):
        self.interpreter = configure.Interpreter(ReaderStub)

    def tearDown(self):
        del self.interpreter.variable_list
        del self.interpreter

    def test_can_set_variable(self):
        with patch('configure.Parser.parse_line', MagicMock(return_value=['set', 'VAR', 'myvar'])):
            self.interpreter.run("fdsf")
        self.assertEqual(self.interpreter.variable_list.get_variable('VAR'), 'myvar')
        keys, values = self.interpreter.variable_list.get()
        self.assertEqual(len(keys), 1)

    def test_can_set_two_variables(self):
        with patch('configure.Parser.parse_line', MagicMock(return_value=['set', 'VAR1', 'myvar1'])):
            self.interpreter.run("fdsf")
        with patch('configure.Parser.parse_line', MagicMock(return_value=['set', 'VAR2', 'myvar2'])):
            self.interpreter.run("fdsf")
        self.assertEqual(self.interpreter.variable_list.get_variable('VAR1'), 'myvar1')
        self.assertEqual(self.interpreter.variable_list.get_variable('VAR2'), 'myvar2')
        keys, values = self.interpreter.variable_list.get()
        self.assertEqual(len(keys), 2)

    def test_can_set_bool(self):
        with patch('configure.Parser.parse_line', MagicMock(return_value=['bool', '"Test variable 1"', 'VAR1'])), \
                patch('configure.Asker.ask', MagicMock(return_value='n')):
            self.interpreter.run("fdsf")
        with patch('configure.Parser.parse_line', MagicMock(return_value=['bool', '"Test variable 2"', 'VAR2'])), \
                patch('configure.Asker.ask', MagicMock(return_value='y')):
            self.interpreter.run("fdsf")
        self.assertEqual(self.interpreter.variable_list.get_variable('VAR1'), 'n')
        self.assertEqual(self.interpreter.variable_list.get_variable('VAR2'), 'y')
        keys, values = self.interpreter.variable_list.get()
        self.assertEqual(len(keys), 2)

    def test_can_set_option(self):
        with patch('configure.Parser.parse_line', MagicMock(return_value=['option', '"Test variable 1"', 'VAR1', 'val1', 'val2', 'val3'])), \
                patch('configure.Asker.ask', MagicMock(return_value='val1')):
            self.interpreter.run("fdsf")
        with patch('configure.Parser.parse_line', MagicMock(return_value=['option', '"Test variable 2"', 'VAR2', 'val1', 'val2', 'val3'])), \
                patch('configure.Asker.ask', MagicMock(return_value='val2')):
            self.interpreter.run("fdsf")
        self.assertEqual(self.interpreter.variable_list.get_variable('VAR1'), 'val1')
        self.assertEqual(self.interpreter.variable_list.get_variable('VAR2'), 'val2')
        keys, values = self.interpreter.variable_list.get()
        self.assertEqual(len(keys), 2)


class AskerTests(unittest.TestCase):

    def setUp(self):
        self.asker = configure.Asker

    def test_can_ask(self):
        with patch('builtins.input', MagicMock(return_value='a')):
            read = self.asker.ask("Ask for something", ['y', 'n', 'a'], None)
            self.assertEqual(read, 'a')


if __name__ == '__main__':
    unittest.main()

