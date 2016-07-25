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
                                for s in self.options
                                if s and s.startswith(text)]
            else:
                self.matches = self.options[:]

        try:
            response = self.matches[state]
        except IndexError:
            response = None
        return response


def set_variable(variable, value):
    for generator in generators:
        generator.generate_define(variable, value)


class Function:
    @staticmethod
    def execute(arguments):
        pass


class Bool(Function):
    @staticmethod
    def execute(arguments):
        readline.set_completer(Completer(['y', 'n']).complete)
        query = arguments[0]
        variable_name = arguments[1]
        choice, default = Bool.__resolve_choice_text(arguments[2:len(arguments)])
        while True:
            read_input = str(input("{} [{}] ".format(query, choice))).strip("\r\n")
            if not read_input:
                set_variable(variable_name, default)
                return
            elif read_input.lower() == 'y' or read_input.lower() == 'n':
                set_variable(variable_name, read_input[0])
                return

    @staticmethod
    def __resolve_choice_text(arguments):
        default = "n" if not arguments else ''.join(arguments[0]).strip('[]')
        choice = "Y/n" if default == "y" else "y/N"
        return choice, default


class Option(Function):
    @staticmethod
    def execute(arguments):
        readline.set_completer(Completer(arguments[2:len(arguments) - 1]).complete)
        query = arguments[0]
        variable_name = arguments[1]
        read_input = ""
        while not read_input:
            read_input = str(input("{} [{}] ".format(query, '/'.join(arguments[2:len(arguments) - 1])))).strip("\r\n")
            if read_input:
                break
            print("  -> Enter correct option!")
        set_variable(variable_name, read_input)


class Include(Function):
    @staticmethod
    def execute(arguments):
        config_filename = arguments[0]
        parse_file(config_filename)


class HashComment(Function):
    @staticmethod
    def execute(arguments):
        print('#')
        print('# ' + ' '.join(arguments))
        print('#')


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


def signal_handler(s, f):
    print("")
    sys.exit(1)


def get_function(function_name):
    if function_name == "bool":
        function = Bool()
    elif function_name == "option":
        function = Option()
    elif function_name == "#":
        function = HashComment()
    elif function_name == "include":
        function = Include()
    else:
        raise NotImplementedError
    return function


def error_handler(error_string, line, line_number):
    print("Error at line {}: {}".format(line_number, error_string))
    print("\t-> {}".format(line.strip("\r\n")))
    sys.exit(1)


def parse_line(_line, line_number: str):
    for parsed in csv.reader([_line], delimiter=' ', quotechar='"'):

        if len(parsed) == 0:
            return

        function_name = parsed[0]
        arguments = parsed[1:len(parsed)]

        try:
            function = get_function(function_name)
            function.execute(arguments)
        except IndexError:
            error_handler("Not enough arguments for: \'{}\'".format(function_name), _line, line_number)
        except NotImplementedError:
            error_handler("No such keyword: \'{}\'".format(function_name), _line, line_number)


def parse_file(filename: str):
    input_file = open(str(filename), 'r')
    line_number = 1
    for line in input_file:
        parse_line(line, line_number)
        line_number += 1
    input_file.close()


if __name__ == '__main__':
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
            for __gen in generator_list:
                if __gen.name == str(output[0]):
                    generators.append(__gen(open(str(output[1]), 'w')))
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

sys.exit(0)
