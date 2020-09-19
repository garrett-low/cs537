#!/usr/bin/env python3

import os, sys
from subprocess import Popen, PIPE, CalledProcessError
import signal


class TimeoutException(Exception):
    def __init__(self):
        super(TimeoutException, self).__init__("Timeout")


class Timeout:
    """Raise TimeoutError when a function call times out"""

    def __init__(self, seconds=0):
        self.seconds = seconds

    def handle_timeout(self, signum, frame):
        raise TimeoutException()

    def __enter__(self):
        signal.signal(signal.SIGALRM, self.handle_timeout)
        signal.alarm(self.seconds)

    def __exit__(self, type, value, traceback):
        signal.alarm(0)


class Test(object):
    """Barebone of a unit test."""

    name = "empty test"
    description = "this tests nothing and should be overriden"
    point_value = 10
    timeout = 20
    child = None

    @classmethod
    def popen_as_child(cls, cmd, check=True):
        p = Popen(cmd, stdout=PIPE, stderr=PIPE)
        cls.child = p
        stdout, stderr = p.communicate()
        cls.child = None
        if check:
            if p.returncode != 0:
                raise CalledProcessError(p.returncode, cmd, stdout, stderr)
            return stdout
        return stdout, stderr, p.returncode

    @classmethod
    def test(cls):
        """any real implementation of the test goes here
        it should raise exceptions when things fails (mostly through assert)
        and the run() below should handle that"""
        pass

    @classmethod
    def run(cls, args=None):
        """run the test
        This method prints useful message/line separators/etc.
        Any exception comes out of test() will be catched and regarded as
        failure.
        If nothing fails, the point value of the test is returned."""

        print('='*21 + ('%s starts' % cls.name).center(30) + '='*21)
        print(cls.description)
        print('point value: %d' % cls.point_value)

        try:
            with Timeout(cls.timeout):
                cls.test()
        except Exception as e:
            print('Test failed: %s' % cls.name)
            print('Reason:      %s' % e)
            print()
            if cls.child is not None:
                try:
                    with Timeout(5):
                        cls.child.kill()
                        cls.child.wait()
                except Exception as e:
                    print('Failed to kill child %d' % cls.child.pid)
            return 0
        print('Test succeeded: %s' % cls.name)
        print()
        return cls.point_value
