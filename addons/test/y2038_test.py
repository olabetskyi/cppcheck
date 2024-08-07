# Running the test with Python 2:
# Be sure to install pytest version 4.6.4 (newer should also work)
# Command in cppcheck directory:
# python -m pytest addons/test/test-y2038.py
#
# Running the test with Python 3:
# Command in cppcheck directory:
# PYTHONPATH=./addons python3 -m pytest addons/test/test-y2038.py

import sys
import pytest

from addons.y2038 import check_y2038_safe

from .util import dump_create, dump_remove, convert_json_output


TEST_SOURCE_FILES = ['./addons/test/y2038/y2038-test-1-bad-time-bits.c',
                     './addons/test/y2038/y2038-test-2-no-time-bits.c',
                     './addons/test/y2038/y2038-test-3-no-use-time-bits.c',
                     './addons/test/y2038/y2038-test-4-good.c',
                     './addons/test/y2038/y2038-test-5-good-no-time-used.c']


def setup_module(module):
    sys.argv.append("--cli")
    for f in TEST_SOURCE_FILES:
        dump_create(f)


def teardown_module(module):
    sys.argv.remove("--cli")
    for f in TEST_SOURCE_FILES:
        dump_remove(f)


def test_1_bad_time_bits(capsys):
    is_safe = check_y2038_safe('./addons/test/y2038/y2038-test-1-bad-time-bits.c.dump', quiet=True)
    assert(is_safe is False)
    captured = capsys.readouterr()
    captured = captured.out.splitlines()
    json_output = convert_json_output(captured)

    # Has exactly one warnings of _TIME_BITS and _USE_TIME_BITS64 kind.
    assert(len(json_output['type-bits-undef']) == 1)
    assert(len(json_output['type-bits-not-64']) == 1)

    # There are 2 unsafe calls in test source and 3 in y2038-in.h
    unsafe_calls = json_output['unsafe-call']
    assert(len([c for c in unsafe_calls if c['file'].endswith('h')]) == 3)
    assert(len([c for c in unsafe_calls if c['file'].endswith('c')]) == 0)


def test_2_no_time_bits(capsys):
    is_safe = check_y2038_safe('./addons/test/y2038/y2038-test-2-no-time-bits.c.dump', quiet=True)
    assert(is_safe is False)
    captured = capsys.readouterr()
    captured = captured.out.splitlines()
    json_output = convert_json_output(captured)

    # _USE_TIME_BITS64 defined in y2038-inc.h header, but there is not
    # _TIME_BITS definition. Here must be appropriate warning.
    assert(len(json_output['type-bits-undef']) == 1)
    assert(json_output.get('type-bits-not-64') is None)

    # y2038-in.h still has y2038-unsafe calls.
    unsafe_calls = json_output['unsafe-call']
    assert(len([c for c in unsafe_calls if c['file'].endswith('h')]) == 3)


def test_3_no_use_time_bits(capsys):
    is_safe = check_y2038_safe('./addons/test/y2038/y2038-test-3-no-use-time-bits.c.dump', quiet=True)
    assert(is_safe is False)
    captured = capsys.readouterr()
    captured = captured.out.splitlines()
    json_output = convert_json_output(captured)

    # Included bad _USE_TIME_BITS64 definition must trigger the errors.
    unsafe_calls = json_output['unsafe-call']
    assert(len(unsafe_calls) == 2)


def test_4_good(capsys):
    # pylint: disable-next=unused-variable - FIXME
    is_safe = check_y2038_safe('./addons/test/y2038/y2038-test-4-good.c.dump', quiet=True)
    # assert(is_safe is True) # FIXME: This should be a "good" example returning "True" instead of "False"
    captured = capsys.readouterr()
    captured = captured.out.splitlines()
    json_output = convert_json_output(captured)

    # Defined _TIME_BITS equal to 64 so that glibc knows we want Y2038 support.
    # There are no warnings from C sources.
    unsafe_calls = json_output['unsafe-call']
    assert(len([c for c in unsafe_calls if c['file'].endswith('.c')]) == 0)


def test_5_good(capsys):
    is_safe = check_y2038_safe('./addons/test/y2038/y2038-test-5-good-no-time-used.c.dump', quiet=True)
    assert(is_safe is True)
    captured = capsys.readouterr()
    captured = captured.out.splitlines()
    json_output = convert_json_output(captured)

    # There are no warnings from C sources.
    if 'unsafe-call' in json_output:
        unsafe_calls = json_output['unsafe-call']
        assert(len([c for c in unsafe_calls if c['file'].endswith('.c')]) == 0)


def test_arguments_regression():
    args_ok = ["-t=foo", "--template=foo",
               "-q", "--quiet",
               "--cli"]
    # Arguments with expected SystemExit
    args_exit = ["--non-exists", "--non-exists-param=42", "-h", "--help"]

    from addons.y2038 import get_args_parser

    # sys.argv contains all pytest arguments - so clear all existing arguments first and restore afterwards
    sys_argv_old = sys.argv
    sys.argv = [sys.argv[0]]

    try:
        for arg in args_exit:
            sys.argv.append(arg)
            with pytest.raises(SystemExit):
                parser = get_args_parser()
                parser.parse_args()
            sys.argv.remove(arg)

        for arg in args_ok:
            sys.argv.append(arg)
            try:
                parser = get_args_parser()
                parser.parse_args()
            except SystemExit:
                pytest.fail("Unexpected SystemExit with '%s'" % arg)
            sys.argv.remove(arg)
    finally:
        sys.argv = sys_argv_old