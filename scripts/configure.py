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


class Asker:
    @staticmethod
    def ask(query, values, default):
        readline.set_completer(Completer(values).complete)
        choice = "/".join(values)
        while True:
            read_input = str(input("{} [{}/def:{}] ".format(query, choice, default))).strip("\r\n")
            if not read_input and default:
                return default
            elif read_input in values:
                return read_input
        return ""


class Function:
    name = ""
    def execute(arguments, variables, conditions):
        pass


class Bool(Function):
    name = "bool"

    def __init__(self, variables, conditions):
        self.__variables = variables


    def execute(self, arguments):
        query = arguments[0]
        variable_name = arguments[1]
        default = arguments[2].strip('[]') if len(arguments) > 2 else 'n'
        self.__variables.set_variable(variable_name, Asker.ask(query, ['y', 'n'], default))


class Option(Function):
    name = "option"

    def __init__(self, variables, conditions):
        self.__variables = variables


    def execute(self, arguments):
        query = arguments[0]
        variable_name = arguments[1]
        default_value = arguments[2].strip('[]') if len(arguments) > 1 else ''
        self.__variables.set_variable(variable_name, Asker.ask(query, arguments[2:len(arguments) - 1], default_value))


class HashComment(Function):
    name ="#"

    def __init__(self, variables, conditions):
        self.__variables = variables


    def execute(self, arguments):
        print('#')
        print('# ' + ' '.join(arguments))
        print('#')


class Output(Function):
    name = "output"

    def __init__(self, variables, conditions):
        self.__variables = variables


    def execute(self, arguments):
        for generator in generator_list:
            if generator.name == arguments[1]:
                interpreter.generators.append(generator(open(arguments[0], "w")))


class If(Function):
    name = "if"

    def __init__(self, variables, conditions):
        self.__variables = variables
        self.__conditions = conditions


    def execute(self, arguments):
        left = self.__variables.get_variable(arguments[0])
        condition = arguments[1]
        if not left:
            left = arguments[0]
        right = self.__variables.get_variable(arguments[2])
        if not right:
            right = arguments[2]
        if condition == "==":
            if left == right:
                self.__conditions.append(True)
            else:
                self.__conditions.append(False)


class Else(Function):
    name = "else"

    def __init__(self, variables, conditions):
        self.__variables = variables
        self.__conditions = conditions

    def execute(self, arguments):
        self.__conditions[-1] = not self.__conditions[-1]


class EndIf(Function):
    name = "endif"

    def __init__(self, variables, conditions):
        self.__variables = variables
        self.__conditions = conditions

    def execute(self, arguments):
        if not len(self.__conditions):
            raise IndexError
        self.__conditions.pop()


class Set(Function):
    name = "set"

    def __init__(self, variables, conditions):
        self.__variables = variables


    def execute(self, arguments):
        self.__variables.set_variable(arguments[0], arguments[1])


class Die(Function):
    name = "die"

    def __init__(self, variables, conditions):
        self.__variables = variables


    def execute(self, arguments):
        print("Error: {}".format(" ".join(arguments)))
        sys.exit(1)


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
        return os.environ.get(name)


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

    variable_list = VariableList()
    condition_stack = []
    condition_instructions = ["endif", "else"]
    builtins = {Bool.name:Bool(variable_list, condition_stack),
                Option.name:Option(variable_list, condition_stack),
                HashComment.name:HashComment(variable_list, condition_stack),
                If.name:If(variable_list, condition_stack),
                Else.name:Else(variable_list, condition_stack),
                EndIf.name:EndIf(variable_list, condition_stack),
                Set.name:Set(variable_list, condition_stack),
                Output.name:Output(variable_list, condition_stack),
                Die.name:Die(variable_list, condition_stack)}
    generators = []
    __file_parser = []


    def __init__(self, file_parser):
        self.__file_parser.append(file_parser)
        pass


    def __get_function(self, name):
        if name in self.builtins:
            return self.builtins[name]
        raise NotImplementedError


    def parse_line(self):

        parsed = self.__file_parser[-1].parse_line()
        if not parsed:
            self.__file_parser.pop()
            if not len(self.__file_parser):
                return 1
            return
        function_name = parsed[0]
        arguments = parsed[1:len(parsed)]

        if len(self.condition_stack):
            if self.condition_stack[-1] == False:
                if not function_name in self.condition_instructions:
                    return

        try:
            if function_name == "include":
                filename = arguments[0]
                self.__file_parser.append(FileParser(filename))
                return
            function = self.__get_function(function_name)
            function.execute(arguments)

        except IndexError:
            error_handler("Not enough arguments for: \'{}\'".format(function_name), self.__file_parser[-1].get_line(), self.__file_parser[-1].get_line_number())
        except NotImplementedError:
            error_handler("No such keyword: \'{}\'".format(function_name), self.__file_parser[-1].get_line(), self.__file_parser[-1].get_line_number())


    def generate_output(self):
        print("\nConfiguration:")
        names, values = self.variable_list.get()
        for name, value in zip(names, values):
            print("{} = {}".format(name, value))
            for generator in self.generators:
                generator.generate_define(name, value)


    def start(self):
        while True:
            if self.parse_line():
                return


class FileParser:

    def __init__(self, filename):
        self.__filename = filename
        self.__file = open(filename, "r+")
        self.__line = ""
        self.__line_number = 1


    def __del__(self):
        self.__file.close()


    def parse_line(self):
        self.__line = self.__file.readline()
        if not self.__line:
            return None
        for parsed in csv.reader([self.__line], delimiter=' ', quotechar='"'):
            if len(parsed) == 0:
                self.__line_number += 1
                return self.parse_line()
            while not parsed[0]:
                parsed = parsed[1:len(parsed)]
            self.__line_number += 1
            return parsed


    def get_filename(self):
        return self.__filename


    def get_line(self):
        return self.__line


    def get_line_number(self):
        return self.__line_number


def signal_handler(s, f):
    print("")
    sys.exit(1)


if __name__ == '__main__':

    signal.signal(signal.SIGINT, signal_handler)
    readline.parse_and_bind('tab: complete')
    generator_list = [CMakeGenerator, CHeaderGenerator]

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

    interpreter = Interpreter(FileParser(input_filename))
    interpreter.start()
    interpreter.generate_output()

