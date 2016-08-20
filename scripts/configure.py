#!/usr/bin/python3

import csv
import signal
import readline
import sys
import getopt
import os


class Completer(object):
    def __init__(self, options):
        self.options = sorted(options)
        self.matches = None
        return

    def complete(self, text, state):
        if state == 0:
            if text:
                self.matches = [s
                    for s in self.options if s and s.startswith(text)]
            else:
                self.matches = self.options[:]

        try:
            response = self.matches[state]
        except IndexError:
            response = None
        return response


class Function:
    name = ""
    @staticmethod
    def execute(arguments):
        return "", ""


class Bool(Function):
    name = "bool"
    @staticmethod
    def execute(arguments):
        readline.set_completer(Completer(['y', 'n']).complete)
        query = arguments[0]
        variable_name = arguments[1]
        choice, default = Bool.__resolve_choice_text(arguments[2:len(arguments)])
        while True:
            read_input = str(input("{} [{}] ".format(query, choice))).strip("\r\n")
            if not read_input:
                return variable_name, default
            elif read_input.lower() == 'y' or read_input.lower() == 'n':
                return variable_name, read_input[0]


    @staticmethod
    def __resolve_choice_text(arguments):
        default = "n" if not arguments else ''.join(arguments[0]).strip('[]')
        choice = "Y/n" if default == "y" else "y/N"
        return choice, default


class Option(Function):
    name = "option"
    @staticmethod
    def execute(arguments):
        readline.set_completer(Completer(arguments[2:-1]).complete)
        query = arguments[0]
        variable_name = arguments[1]
        if len(arguments) > 1:
            default_value = arguments[2].strip('[]')
        read_input = ""
        while not read_input:
            read_input = str(input("{} [{}] ".format(query, '/'.join(arguments[2:len(arguments) - 1])))).strip("\r\n")
            if read_input:
                break
            return variable_name, default_value
        return variable_name, read_input


class Include(Function):
    name = "include"
    @staticmethod
    def execute(arguments):
        config_filename = arguments[0]
        parse_file(config_filename)
        return "", ""


class HashComment(Function):
    name ="#"
    @staticmethod
    def execute(arguments):
        print('#')
        print('# ' + ' '.join(arguments))
        print('#')
        return "", ""


class Output(Function):
    name = "output"
    @staticmethod
    def execute(arguments):
        for generator in generator_list:
            if generator.name == arguments[1]:
                interpreter.generators.append(generator(open(arguments[0], "w")))
                return "", ""
        return "", ""


class If(Function):
    name = "if"
    @staticmethod
    def execute(arguments):
        left = interpreter.read_variable(arguments[0])
        condition = arguments[1]
        if not left:
            left = arguments[0]
        right = interpreter.read_variable(arguments[2])
        if not right:
            right = arguments[2]
        if condition == "==":
            if left == right:
                interpreter.condition_stack.append(True)
            else:
                interpreter.condition_stack.append(False)
        return "", ""


class Else(Function):
    name = "else"
    @staticmethod
    def execute(arguments):
        interpreter.condition_stack[-1] = not interpreter.condition_stack[-1]
        return "", ""


class EndIf(Function):
    name = "endif"
    @staticmethod
    def execute(arguments):
        if not len(interpreter.condition_stack):
            raise IndexError
        interpreter.condition_stack.pop()
        return "", ""


class Set(Function):
    name = "set"
    @staticmethod
    def execute(arguments):
        return arguments[0], arguments[1]


class Die(Function):
    name = "die"
    @staticmethod
    def execute(arguments):
        print("Error: {}".format(" ".join(arguments)))
        sys.exit(1)
        return "", ""


class Generator:

    name = ""

    def __init__(self, file):
        self.output_file = file

    def generate_define(self, name, value):
        return ""


class CMakeGenerator(Generator):

    name = "cmake"

    def __init__(self, file):
        super().__init__(file)
        self.output_file = file

    def generate_define(self, name, value):
        self.output_file.write("set({} {})\r\n".format(name, value))


class CHeaderGenerator(Generator):

    name = "c"

    def __init__(self, file):
        super().__init__(file)
        self.output_file = file

    def generate_define(self, name, value):
        if value == 'y':
            self.output_file.write("#define {} 1\r\n".format(name))
        elif value == 'n':
            self.output_file.write("//#define {} 0\r\n".format(name))
        else:
            self.output_file.write("#define {} {}\r\n".format(name, value))


def error_handler(error_string, line, line_number):
    print("Error at line {}: {}".format(line_number, error_string))
    print("\t-> {}".format(line.strip("\r\n")))
    sys.exit(1)


class Variable:

    name = ""
    value = ""

    def __init__(self, _name, _value):
        self.name = _name
        self.value = _value


class VariableList:

    __variables = {}

    def get_variable(self, name):
        if name in self.__variables:
            return self.__variables[name]
        return ""


    def set_variable(self, name, value):
        self.__variables[name] = value


    def unset_variable(self, name):
        pass


    def get(self):
        values = []
        names = []
        for key, value in self.__variables.items():
            names.append(key)
            values.append(value)
        return names, values


class Interpreter:

    line = ""
    variable_list = VariableList()
    condition_stack = []
    builtins = {"bool":Bool(), "option":Option(), "#":HashComment(), "if":If(), "else":Else(), "endif":EndIf(), "set":Set(), "include":Include(), "output":Output(), "die":Die()}
    generators = []


    def __init__(self):
        pass


    def __get_function(self, name):
        if name in self.builtins:
            return self.builtins[name]
        raise NotImplementedError


    def read_variable(self, variable_name):
        return self.variable_list.get_variable(variable_name)


    def parse_line(self, _line, line_number):

        for parsed in csv.reader([_line], delimiter=' ', quotechar='"'):

            line = _line

            if len(parsed) == 0:
                return

            while not parsed[0]:
                parsed = parsed[1:len(parsed)]

            function_name = parsed[0]
            arguments = parsed[1:len(parsed)]

            if len(self.condition_stack):
                if self.condition_stack[-1] == False:
                    if not function_name == "endif" and not function_name == "else":
                       return

            try:
                function = self.__get_function(function_name)
                variable_name, value = function.execute(arguments)
                if variable_name and value:
                    self.variable_list.set_variable(variable_name, value)

            except IndexError:
                error_handler("Not enough arguments for: \'{}\'".format(function_name), _line, line_number)
            except NotImplementedError:
                error_handler("No such keyword: \'{}\'".format(function_name), _line, line_number)


    def generate_output(self):
        print("\nConfiguration:")
        names, values = self.variable_list.get()
        for name, value in zip(names, values):
            print("{} = {}".format(name, value))
            for generator in self.generators:
                generator.generate_define(name, value)
        pass


def parse_file(filename):
    input_file = open(str(filename), 'r')
    line_number = 1
    for line in input_file:
        interpreter.parse_line(line, line_number)
        line_number += 1
    input_file.close()


def signal_handler(s, f):
    print("")
    sys.exit(1)


if __name__ == '__main__':

    interpreter = Interpreter()
    signal.signal(signal.SIGINT, signal_handler)
    readline.parse_and_bind('tab: complete')
    generator_list = [CMakeGenerator, CHeaderGenerator]
    input_filename = ''

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hi:", ["ifile="])
    except getopt.GetoptError:
        print('configure.py -i <inputfile>')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('configure.py -i <inputfile>')
            sys.exit(0)
        elif opt in ("-i", "--ifile"):
            input_filename = arg
        else:
            print("Not recognized option: {}".format(arg))
            sys.exit(1)

    parse_file(input_filename)
    interpreter.generate_output()

