#!/usr/bin/python3

import csv
import signal
import readline
import sys
import getopt


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
        readline.set_completer(Completer(arguments[2:len(arguments) - 1]).complete)
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



class Variable:

    name = ""
    value = ""

    def __init__(self, _name, _value):
        self.name = _name
        self.value = _value



def error_handler(error_string, line, line_number):
    print("Error at line {}: {}".format(line_number, error_string))
    print("\t-> {}".format(line.strip("\r\n")))
    sys.exit(1)


class Interpreter:

    list_of_variables = []
    stack = []
    builtins = [Bool(), Option(), HashComment(), Include()]

    def __init__(self):
        pass


    def __get_function(self, function_name):
        for function in self.builtins:
            if function.name == function_name:
                return function
        raise NotImplementedError


    def parse_line(self, _line, line_number):
        for parsed in csv.reader([_line], delimiter=' ', quotechar='"'):

            if len(parsed) == 0:
                return

            while not parsed[0]:
                parsed = parsed[1:len(parsed)]

            function_name = parsed[0]
            arguments = parsed[1:len(parsed)]

            try:
                function = self.__get_function(function_name)
                variable_name, value = function.execute(arguments)
                if variable_name and value:
                    self.list_of_variables.append(Variable(variable_name, value))

            except IndexError:
                error_handler("Not enough arguments for: \'{}\'".format(function_name), _line, line_number)
            except NotImplementedError:
                error_handler("No such keyword: \'{}\'".format(function_name), _line, line_number)


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
    generators = []
    generator_list = [CMakeGenerator, CHeaderGenerator]
    input_filename = ''

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hi:o:", ["ifile=", "ofile="])
    except getopt.GetoptError:
        print('configure.py -i <inputfile> -o <outputfile>')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('configure.py -i <inputfile> -o <outputfile>')
            sys.exit(0)
        elif opt in ("-o", "--ofile"):
            output = str(arg).split(',')
            for generator in generator_list:
                if generator.name == str(output[0]):
                    generators.append(generator(open(str(output[1]), 'w')))
                    break
        elif opt in ("-i", "--ifile"):
            input_filename = arg
        else:
            print("Not recognized option: {}".format(arg))
            sys.exit(1)

    if len(generators) == 0:
        print("No output files!")
        sys.exit(1)

    parse_file(input_filename)
    print("\nConfiguration:")
    for variable in interpreter.list_of_variables:
        print("{} = {}".format(variable.name, variable.value))
        for generator in generators:
            generator.generate_define(variable.name, variable.value)
    print("")



