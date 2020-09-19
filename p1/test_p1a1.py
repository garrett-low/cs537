#!/usr/bin/env python3

import os, sys
from subprocess import Popen, PIPE, CalledProcessError
import signal
from TestFramework import *

# test parameters
inputfile = 'inputfile'
outputfile = 'outputfile'


class TestCompile(Test):
    name = "compilation test"
    description = "compiles source code"
    cmd = 'gcc -Wall -Werror -o reverse reverse.c'.split()

    @classmethod
    def test(cls):
        cls.popen_as_child(cls.cmd)


def reverse(lines):
    """python implementation of the reverse program"""
    output = []
    for line in lines:
        assert line[-1] == '\n', 'Every line must be newline-terminated'
        output.append(line[:-1][::-1] + '\n')
    output.reverse()
    return output


class ReverseTest(Test):
    """abstract class for all test that runs the reverse program to extend"""
    cmd = './reverse -i %s -o %s' % (inputfile, outputfile)
    stdout = ''
    stderr = ''

    @staticmethod
    def generate_lines():
        """generate inputfile lines for testing"""
        return []

    @classmethod
    def test(cls):
        if os.path.isfile(outputfile):
            os.remove(outputfile)

        input_lines = cls.generate_lines()
        with open(inputfile, 'w') as f:
            f.write(''.join(input_lines))

        cmd = (cls.cmd).split()
        cls.stdout, cls.stderr, rc = cls.popen_as_child(cmd, check=False)
        cls.stdout = cls.stdout.decode("ascii")
        cls.stderr = cls.stderr.decode("ascii")
        assert rc == 0, 'Non-zero exit status %d' % rc

        with open(outputfile, 'r') as f:
            test_output_lines = f.read()
        output_lines = ''.join(reverse(input_lines))

        assert output_lines == test_output_lines, 'Output does not match'


class ExpectErrorTest(ReverseTest):
    """abstract class for all reverse tests that expect an error"""
    returncode = 1
    err_msg = ''

    @classmethod
    def test(cls):
        try:
            super(ExpectErrorTest, cls).test()
        except AssertionError as e:
            assert e.args[0] == 'Non-zero exit status %d' % cls.returncode, 'Expect non-zero exit status'
            assert cls.stdout == '', 'Expect nothing to be printed in stdout'
            assert cls.stderr == cls.err_msg, 'Unexpected error message'
            return cls.point_value
        assert False, 'Expect an error from the program'


class TestInvalidFile(ExpectErrorTest):
    name = "invalid file test"
    description = "tests on a non-existing file"

    inputfile_not_exist  = 'youshouldnothavethisfileinyourdirectory'
    cmd = './reverse -i %s -o %s' % (inputfile_not_exist, outputfile)
    err_msg = 'reverse: Cannot open file: %s\n' % inputfile_not_exist


class TestInvalidArg1(ExpectErrorTest):
    name = "invalid argument test 1"
    description = "tests on too few of arguments"
    point_value = 5

    cmd = './reverse -i %s' % inputfile
    returncode = 1
    err_msg = "Usage: reverse -i inputfile -o outputfile\n"


class TestInvalidArg2(ExpectErrorTest):
    name = "invalid argument test 2"
    description = "tests on too many arguments"
    point_value = 5

    cmd = './reverse -i %s -o %s -x somejunk' % (inputfile, outputfile)
    returncode = 1
    err_msg = "Usage: reverse -i inputfile -o outputfile\n"


class TestInvalidArg3(ExpectErrorTest):
    name = "invalid argument test 3"
    description = "tests on arguments with wrong flags"
    point_value = 5

    cmd = './reverse -x %s -y %s' % (inputfile, outputfile)
    returncode = 1
    err_msg = "Usage: reverse -i inputfile -o outputfile\n"


class TestExample(ReverseTest):
    name = "example test"
    description = "comes from the project specification"

    @staticmethod
    def generate_lines():
        return ['first-1\n', 'second-2\n', 'third-3\n', 'fourth-4\n', 'fifth-5\n']


class TestEmptyFile(ReverseTest):
    name = "empty file test"
    description = "tests on an empty file"


class TestNormal1(ReverseTest):
    name = "normal test 1"
    description = "tests on 1000 short lines"

    @staticmethod
    def generate_lines():
        return ['%d\n' % i for i in range(1000)]


class TestNormal2(ReverseTest):
    name = "normal test 2"
    description = "tests on 1000 long lines"

    @staticmethod
    def generate_lines():
        return [('%d\n' % i).rjust(511, '*') for i in range(1000)]


class TestNormal3(ReverseTest):
    name = "normal test 3"
    description = "tests on 5000 varying length lines"
    point_value = 5

    @staticmethod
    def generate_lines():
        return [('%d\n' % i).rjust(i % 511, '*') for i in range(5000)]


class TestNormal4(ReverseTest):
    name = "normal test 4"
    description = "tests on odd number of long lines"
    point_value = 5

    @staticmethod
    def generate_lines():
        return [('%d\n' % i).rjust(511, '*') for i in range(5001)]


class TestNormal5(ReverseTest):
    name = "normal test 5"
    description = "tests on 10 thousand newlines"
    point_value = 5

    @staticmethod
    def generate_lines():
        return ['\n' for i in range(10000)]

class TestStress1(ReverseTest):
    name = "stress test 1"
    description = "tests on 100 thousand long lines (~50MB)"
    point_value = 5
    timeout = 30

    @staticmethod
    def generate_lines():
        return [('%d\n' % i).rjust(511, '*') for i in range(100000)]


class TestStress2(ReverseTest):
    name = "stress test 2"
    description = "tests on 10 million short lines (~100MB)"
    point_value = 5
    timeout = 60

    @staticmethod
    def generate_lines():
        return ['%d\n' % i for i in range(10*1000*1000)]


def run_tests(tests, short_circuit=True, cleanup=False):
    points = 0
    tot_points = sum(test.point_value for test in tests)
    succeeded = failed = 0
    for test in tests:
        point = test.run()
        if point == 0:
            failed += 1
            if short_circuit:
                break
        else:
            succeeded += 1
        points += point
    if cleanup:
        print('-'*21 + 'cleaning up'.center(30) + '-'*21)
        for f in ['inputfile', 'outputfile', 'reverse', 'reverse.o']:
            if os.path.isfile(f):
                print('removing %s' % f)
                os.remove(f)
        print('-'*21 + 'cleanup done'.center(30) + '-'*21)
        print()

    notrun = len(tests)-succeeded-failed
    print('succeeded: %d' % succeeded)
    print('failed: %d' % failed)
    print('not-run: %d\n' % notrun)
    print('score: %d (out of %d)' % (points, tot_points))


def main():
    testall = True
    cleanup = True
    for arg in sys.argv[1:]:
        if arg.startswith('-a'):
            testall = True
        elif arg.startswith('-c'):
            cleanup = True

    run_tests([TestCompile,
        TestInvalidFile,
        TestInvalidArg1,
        TestInvalidArg2,
        TestInvalidArg3,
        TestExample,
        TestEmptyFile,
        TestNormal1,
        TestNormal2,
        TestNormal3,
        TestNormal4,
        TestNormal5,
        TestStress1,
        TestStress2], short_circuit=not testall, cleanup=cleanup)

if __name__ == '__main__':
    main()
