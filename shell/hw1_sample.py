#!/usr/bin/env python
from time import sleep

import pexpect
import re
import sys
import os

SHELL = "./w4118_sh"
PROMPT = "$"
PROMPT = re.escape(PROMPT)

"""
W4118 Operating System I - Homework 1 Test Sample Script
"""

class HomeworkOneTester():
    test_cases = []
    test_results = []

    def add_test(self, cmd, expecting=None, continuation=False):
        """@cmd - command to execute
           @expecting - regex pattern to expect
           @continuation - True if this is an additional command in a
                           multi-command test
        """
        if type(cmd) is dict:
            if len(self.test_cases):
                assert(not cmd['continuation'])
            cmd['num'] = len(self.test_cases)+1
            self.test_cases.append(test_case)
            return
        if not len(self.test_cases):
            assert(not continuation)
        test_case = { 'num'          : len(self.test_cases)+1,
                      'cmd'          : cmd,
                      'expecting'    : expecting,
                      'continuation' : continuation
                    }
        self.test_cases.append(test_case)

    def add_path_test(self, expected, continuation=True):
        self.add_test("path", expected, continuation)

    def _spawn_shell(self):
        p = pexpect.spawn(SHELL, timeout=2)
        p.setecho(False)
        try:
            ret = p.expect([PROMPT, pexpect.EOF])
        except pexpect.TIMEOUT:
            print >> sys.stderr, \
                "fatal: cannot match prompt text. cannot proceed!"
            sys.exit(1)
	if ret == 1:
		print "Program exits without any prompt $"
		sys.exit(1)
        self.p = p

    def _close_shell(self):
        assert(self.p)
        self.p.sendline("exit")
        sleep(.1)
        if self.p.isalive():
            self.p.kill(9)
        self.p.close()

    def _matching_path(self, text, expected):
        """
        Test paths, ignoring the order. Dups don't matter.
        """
        paths = set(text.split(':'))
        expected_paths = set(expected.split(':'))
        return (paths.symmetric_difference(expected_paths) != set([]))

    def _run_test(self, test):
        assert(self.p)

        if not self.p.isalive():
            # The shell has crashed
            test_result = { 'test_case' : test,
                            'failed' : True,
                            'got' : "CRASH!"
                          }
            self.test_results.append(test_result)
            return

        self.p.sendline(test['cmd'])
        self.p.expect([PROMPT, pexpect.EOF])
        got = self.p.before.replace('\r', '').rstrip('\n')

        if test['cmd'] == "path":
            failed = self._matching_path(got, test['expecting'])
        else:
            if type(test['expecting']) is str and test['expecting'] != "":
                failed = (re.search(test['expecting'], got) == None)
            else:
                failed = (got != test['expecting'])
        test_result = { 'test_case' : test,
                        'failed' : failed,
                        'got' : got
                      }
        self.test_results.append(test_result)

    def get_results(self):
        return self.test_results

    def _indented_print(self, text, spaces, final_new_line=True):
        i = 1
        lines = text.split('\n')
        for line in lines:
            if i == 1:
                sys.stdout.write("%s" % line)
            else:
                sys.stdout.write("%s%s" % (' '*spaces, line))
            if i != len(lines):
                sys.stdout.write("\n")
            elif final_new_line:
                sys.stdout.write("\n")
            i += 1

    def print_results(self):
        passed_num = len(self.test_results)
        for result in self.test_results:
            if not result['failed']:
                continue
            passed_num -= 1
            test_case = result['test_case']
            print "Test %d failed ('%s')!" % \
                    (test_case['num'], test_case['cmd'])
            test_case['expecting'] = str(test_case['expecting'])
            sys.stdout.write("    Expected: '")
            self._indented_print(test_case['expecting'], 15, False)
            sys.stdout.write("'\n")

            result['got'] = str(result['got'])
            sys.stdout.write("    Got: '")
            self._indented_print(result['got'], 10, False)
            sys.stdout.write("'\n")
        print "%d of %d tests passed" % (passed_num, len(self.test_results))

    def run(self):
        self.test_results = []
        self._spawn_shell()
        for test in self.test_cases:
            if not test['continuation']:
                self._close_shell()
                self._spawn_shell()
            self._run_test(test)


def run_tests():

    tester = HomeworkOneTester()

    # Test basic functionality
    tester.add_test("exit", "")
    tester.add_test("       exit    ", "")
    tester.add_test("   \t  exit    ", "")
    tester.add_test("cd /", "")
    tester.add_test("/bin/echo test", "test")

    # Test basic errors
    tester.add_test("cd /doesnt_exist", "error: No such file or directory")
    tester.add_test("ls", "error:*")

    # Test path functionality
    tester.add_path_test("", False)

    tester.add_test("path + /bin", ".*")
    tester.add_test("path", "/bin", True)
    tester.add_test("echo test", "test", True)
    tester.add_test("path - /bin", ".*", True)
    tester.add_path_test("")

    # Test path errors
    tester.add_test("path invald_arg", "error:*")
    tester.add_test("path -", "error:*")

    #Test pipe functionality
    tester.add_test("/bin/rm -rf /tmp/hmwk1_test", "") 
    tester.add_test("/bin/mkdir /tmp/hmwk1_test", "") 
    tester.add_test("cd /tmp/hmwk1_test", "");
    tester.add_test("/bin/ls|/usr/bin/wc", "      0       0       0", True)
    tester.add_test("/bin/ls | /usr/bin/wc", "      0       0       0", True)

    # Test multiple pipes without arguments
    tester.add_test("/bin/ls|/usr/bin/wc|/usr/bin/wc", "      1       3      24", True)

    # Test pipe errors
    tester.add_test("|", "error:*");
    tester.add_test("/bin/ls|", "error:*");
    tester.add_test("/bin/ls||/usr/bin/wc", "error:*");
    tester.run()
    tester.print_results()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        SHELL = sys.argv[1]
    run_tests()
