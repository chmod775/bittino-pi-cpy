# Copy testmod.mpy to your CIRCUITPY drive alongside this file,
# then at the CircuitPython REPL run: import test_testmod
#
# If you see "testmod: ALL TESTS PASSED" without a HardFault,
# the ABI patches are working end-to-end.

import testmod

# 1. Direct fun_table call + rodata relocation
result = testmod.hello()
assert result == "world", "hello() returned {!r}".format(result)
print("  hello(): OK")

# 2. mp_raise_msg via raise_msg_str path
try:
    testmod.raise_test()
    raise AssertionError("raise_test() did not raise")
except ValueError as e:
    assert str(e) == "testmod raise ok", "wrong message: {!r}".format(str(e))
    print("  raise_test(): OK")

# 3. A fun_table call with argument parsing
result = testmod.add_one(41)
assert result == 42, "add_one(41) returned {!r}".format(result)
print("  add_one(): OK")

print("testmod: ALL TESTS PASSED")
