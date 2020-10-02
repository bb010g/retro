#!/usr/bin/env python3

# Nga: a Virtual Machine
# Copyright (c) 2010 - 2019, Charles Childers
# Floating Point I/O by Arland Childers, (c) 2020
# Optimizations and process() rewrite by Greg Copeland
# -----------------------------------------------------

import os, sys, math, time, struct
from struct import pack, unpack

ip = 0
stack = [] * 128
address = []
memory = []


class FloatStack(object):
    def __init__(self, *d):
        self.data = list(d)

    def __getitem__(self, id):
        return self.data[id]

    def __call__(self):
        return self.data

    def add(self):
        self.data.append(self.data.pop() + self.data.pop())

    def sub(self):
        self.data.append(0 - (self.data.pop() - self.data.pop()))

    def mul(self):
        self.data.append(self.data.pop() * self.data.pop())

    def div(self):
        a, b = self.data.pop(), self.data.pop()
        self.data.append(b / a)

    def ceiling(self):
        self.data.append(math.ceil(self.data.pop()))

    def floor(self):
        self.data.append(math.floor(self.data.pop()))

    def eq(self):
        return 0 - (self.data.pop() == self.data.pop())

    def neq(self):
        return 0 - (self.data.pop() != self.data.pop())

    def gt(self):
        a, b = self.data.pop(), self.data.pop()
        return 0 - (b > a)

    def lt(self):
        a, b = self.data.pop(), self.data.pop()
        return 0 - (b < a)

    def depth(self):
        return len(self.data)

    def drop(self):
        self.data.pop()

    def pop(self):
        return self.data.pop()

    def swap(self):
        a, b = self.data.pop(), self.data.pop()
        self.data += [a, b]

    def push(self, n):
        self.data.append(n)

    def log(self):
        a, b = self.data.pop(), self.data.pop()
        self.data.append(math.log(b, a))

    def power(self):
        a, b = self.data.pop(), self.data.pop()
        self.data.append(math.pow(a, b))

    def sin(self):
        self.data.append(math.sin(self.data.pop()))

    def cos(self):
        self.data.append(math.cos(self.data.pop()))

    def tan(self):
        self.data.append(math.tan(self.data.pop()))

    def asin(self):
        self.data.append(math.asin(self.data.pop()))

    def acos(self):
        self.data.append(math.acos(self.data.pop()))

    def atan(self):
        self.data.append(math.atan(self.data.pop()))


floats = FloatStack()
afloats = FloatStack()

files = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]


def file_open():
    global files
    slot = 0
    i = 1
    while i < 8:
        if files[i] == 0:
            slot = i
        i += 1
    mode = stack.pop()
    name = extractString(stack.pop())
    if slot > 0:
        if mode == 0:
            if os.path.exists(name):
                files[slot] = open(name, "r")
            else:
                slot = 0
        elif mode == 1:
            files[slot] = open(name, "w")
        elif mode == 2:
            files[slot] = open(name, "a")
        elif mode == 3:
            if os.path.exists(name):
                files[slot] = open(name, "r+")
            else:
                slot = 0
    return slot


def file_read():
    global stack
    slot = stack.pop()
    return ord(files[slot].read(1))


def file_write():
    global stack
    slot = stack.pop()
    files[slot].write(chr(stack.pop()))
    return 1


def file_close():
    global files, stack
    slot = stack.pop()
    files[slot].close()
    files[slot] = 0
    return 0


def file_pos():
    global stack
    slot = stack.pop()
    return files[slot].tell()


def file_seek():
    global stack
    slot = stack.pop()
    pos = stack.pop()
    return files[slot].seek(pos, 0)


def file_size():
    global stack
    slot = stack.pop()
    at = files[slot].tell()
    files[slot].seek(0, 2)  # SEEK_END
    end = files[slot].tell()
    files[slot].seek(at, 0)  # SEEK_SET
    return end


def file_delete():
    global stack
    name = extractString(stack.pop())
    i = 0
    if os.path.exists(name):
        os.remove(name)
        i = 1
    return i


def rxDivMod(a, b):
    x = abs(a)
    y = abs(b)
    q, r = divmod(x, y)
    if a < 0 and b < 0:
        r *= -1
    elif a > 0 and b < 0:
        q *= -1
    elif a < 0 and b > 0:
        r *= -1
        q *= -1
    return q, r


def findEntry(named):
    header = memory[2]
    Done = False
    while header != 0 and not Done:
        if named == extractString(header + 3):
            Done = True
        else:
            header = memory[header]
    return header


def rxGetInput():
    return ord(sys.stdin.read(1))


def rxDisplayCharacter():
    global stack
    if stack[-1] > 0 and stack[-1] < 128:
        if stack[-1] == 8:
            sys.stdout.write(chr(stack.pop()))
            sys.stdout.write(chr(32))
            sys.stdout.write(chr(8))
        else:
            sys.stdout.write(chr(stack.pop()))
    else:
        sys.stdout.write("\033[2J\033[1;1H")
        stack.pop()
    sys.stdout.flush()


def i_no():
    pass


def i_li():
    global ip, memory, stack, address
    ip += 1
    stack.append(memory[ip])


def i_du():
    global ip, memory, stack, address
    stack.append(stack[-1])


def i_dr():
    global ip, memory, stack, address
    stack.pop()


def i_sw():
    global ip, memory, stack, address
    a = stack[-2]
    stack[-2] = stack[-1]
    stack[-1] = a


def i_pu():
    global ip, memory, stack, address
    address.append(stack.pop())


def i_po():
    global ip, memory, stack, address
    stack.append(address.pop())


def i_ju():
    global ip, memory, stack, address
    ip = stack.pop() - 1


def i_ca():
    global ip, memory, stack, address
    address.append(ip)
    ip = stack.pop() - 1


def i_cc():
    global ip, memory, stack, address
    target = stack.pop()
    if stack.pop() != 0:
        address.append(ip)
        ip = target - 1


def i_re():
    global ip, memory, stack, address
    ip = address.pop()


def i_eq():
    global ip, memory, stack, address
    a = stack.pop()
    b = stack.pop()
    if b == a:
        stack.append(-1)
    else:
        stack.append(0)


def i_ne():
    global ip, memory, stack, address
    a = stack.pop()
    b = stack.pop()
    if b != a:
        stack.append(-1)
    else:
        stack.append(0)


def i_lt():
    global ip, memory, stack, address
    a = stack.pop()
    b = stack.pop()
    if b < a:
        stack.append(-1)
    else:
        stack.append(0)


def i_gt():
    global ip, memory, stack, address
    a = stack.pop()
    b = stack.pop()
    if b > a:
        stack.append(-1)
    else:
        stack.append(0)


def i_fe():
    global ip, memory, stack, address
    if stack[-1] == -1:
        stack[-1] = len(stack) - 1
    elif stack[-1] == -2:
        stack[-1] = len(address)
    elif stack[-1] == -3:
        stack[-1] = len(memory)
    elif stack[-1] == -4:
        stack[-1] = -2147483648
    elif stack[-1] == -5:
        stack[-1] = 2147483647
    else:
        stack[-1] = memory[stack[-1]]


def i_st():
    global ip, memory, stack, address
    mi = stack.pop()
    memory[mi] = stack.pop()


def i_ad():
    global ip, memory, stack, address
    t = stack.pop()
    stack[-1] += t
    stack[-1] = unpack("=l", pack("=L", stack[-1] & 0xFFFFFFFF))[0]


def i_su():
    global ip, memory, stack, address
    t = stack.pop()
    stack[-1] -= t
    stack[-1] = unpack("=l", pack("=L", stack[-1] & 0xFFFFFFFF))[0]


def i_mu():
    global ip, memory, stack, address
    t = stack.pop()
    stack[-1] *= t
    stack[-1] = unpack("=l", pack("=L", stack[-1] & 0xFFFFFFFF))[0]


def i_di():
    global ip, memory, stack, address
    a = stack[-1]
    b = stack[-2]
    stack[-1], stack[-2] = rxDivMod(b, a)
    stack[-1] = unpack("=l", pack("=L", stack[-1] & 0xFFFFFFFF))[0]
    stack[-2] = unpack("=l", pack("=L", stack[-2] & 0xFFFFFFFF))[0]


def i_an():
    global ip, memory, stack, address
    t = stack.pop()
    stack[-1] &= t


def i_or():
    global ip, memory, stack, address
    t = stack.pop()
    stack[-1] |= t


def i_xo():
    global ip, memory, stack, address
    t = stack.pop()
    stack[-1] ^= t


def i_sh():
    global ip, memory, stack, address
    t = stack.pop()
    if t < 0:
        stack[-1] <<= t * -1
    else:
        stack[-1] >>= t


def i_zr():
    global ip, memory, stack, address
    if stack[-1] == 0:
        stack.pop()
        ip = address.pop()


def i_ha():
    global ip, memory, stack, address
    ip = 9000000


def i_ie():
    stack.append(3)


def i_iq():
    device = stack.pop()
    if device == 0:  # generic output
        stack.append(0)
        stack.append(0)
    if device == 1:  # floating point
        stack.append(1)
        stack.append(2)
    if device == 2:  # files
        stack.append(0)
        stack.append(4)


def i_ii():
    global stack, memory, floats, files
    device = stack.pop()
    if device == 0:  # generic output
        rxDisplayCharacter()
    if device == 1:  # floating point
        action = stack.pop()
        if action == 0:  # number to float
            floats.push(float(stack.pop()))
        if action == 1:  # string to float
            s = stack.pop()
            floats.push(float(extractString(s)))
        if action == 2:  # float to number
            stack.append(int(floats.pop()))
        if action == 3:  # float to string
            injectString(str(floats.pop()), stack.pop())
        if action == 4:  # add
            floats.add()
        if action == 5:  # sub
            floats.sub()
        if action == 6:  # mul
            floats.mul()
        if action == 7:  # div
            floats.div()
        if action == 8:  # floor
            floats.floor()
        if action == 9:  # ceil
            floats.ceiling()
        if action == 10:  # sqrt
            floats.sqrt()
        if action == 11:  # eq
            stack.append(floats.eq())
        if action == 12:  # -eq
            stack.append(floats.neq())
        if action == 13:  # lt
            stack.append(floats.lt())
        if action == 14:  # gt
            stack.append(floats.gt())
        if action == 15:  # depth
            stack.append(floats.depth())
        if action == 16:  # dup
            floats.dup()
        if action == 17:  # drop
            floats.drop()
        if action == 18:  # swap
            floats.swap()
        if action == 19:  # log
            floats.log()
        if action == 20:  # pow
            floats.pow()
        if action == 21:  # sin
            floats.sin()
        if action == 22:  # cos
            floats.cos()
        if action == 23:  # tan
            floats.tan()
        if action == 24:  # asin
            floats.asin()
        if action == 25:  # atan
            floats.atan()
        if action == 26:  # acos
            floats.acos()
        if action == 27:  # to alt.
            afloats.push(floats.pop())
        if action == 28:  # from alt.
            floats.push(afloats.pop())
        if action == 29:  # alt. depth
            stack.append(afloats.depth())
    if device == 2:  # files
        action = stack.pop()
        if action == 0:
            stack.append(file_open())
        if action == 1:
            file_close()
        if action == 2:
            stack.append(file_read())
        if action == 3:
            file_write()
        if action == 4:
            stack.append(file_pos())
        if action == 5:
            file_seek()
        if action == 6:
            stack.append(file_size())
        if action == 7:
            file_delete()
        if action == 8:  # flush
            pass


instructions = [
    i_no,
    i_li,
    i_du,
    i_dr,
    i_sw,
    i_pu,
    i_po,
    i_ju,
    i_ca,
    i_cc,
    i_re,
    i_eq,
    i_ne,
    i_lt,
    i_gt,
    i_fe,
    i_st,
    i_ad,
    i_su,
    i_mu,
    i_di,
    i_an,
    i_or,
    i_xo,
    i_sh,
    i_zr,
    i_ha,
    i_ie,
    i_iq,
    i_ii,
]


def validateOpcode(opcode):
    I0 = opcode & 0xFF
    I1 = (opcode >> 8) & 0xFF
    I2 = (opcode >> 16) & 0xFF
    I3 = (opcode >> 24) & 0xFF
    if (
        (I0 >= 0 and I0 <= 29)
        and (I1 >= 0 and I1 <= 29)
        and (I2 >= 0 and I2 <= 29)
        and (I3 >= 0 and I3 <= 29)
    ):
        return True
    else:
        return False


def extractString(at):
    i = at
    s = ""
    while memory[i] != 0:
        s = s + chr(memory[i])
        i = i + 1
    return s


def injectString(s, to):
    global memory
    i = to
    for c in s:
        memory[i] = ord(c)
        i = i + 1
    memory[i] = 0


def execute(word, notfound, output="console"):
    global ip, memory, stack, address
    ip = word
    address.append(0)
    while ip < 100000 and len(address) > 0:
        if ip == notfound:
            print("ERROR: word not found!")
        opcode = memory[ip]
        if validateOpcode(opcode):
            I0 = opcode & 0xFF
            I1 = (opcode >> 8) & 0xFF
            I2 = (opcode >> 16) & 0xFF
            I3 = (opcode >> 24) & 0xFF
            if I0 != 0:
                instructions[I0]()
            if I1 != 0:
                instructions[I1]()
            if I2 != 0:
                instructions[I2]()
            if I3 != 0:
                instructions[I3]()
        else:
            print("Invalid Bytecode", opcode, ip)
            ip = 2000000
        ip = ip + 1
    return


def load_image():
    global memory
    cells = int(os.path.getsize("ngaImage") / 4)
    f = open("ngaImage", "rb")
    memory = list(struct.unpack(cells * "i", f.read()))
    f.close()
    remaining = 1000000 - cells
    memory.extend([0] * remaining)


def run():
    Done = False
    Interpreter = memory[findEntry("interpret") + 1]
    notfound = memory[findEntry("err:notfound") + 1]

    while not Done:
        Line = input("\nOk> ")
        if Line == "bye":
            Done = True
        else:
            for Token in Line.split(" "):
                injectString(Token, 1025)
                stack.append(1025)
                execute(Interpreter, notfound)


if __name__ == "__main__":
    load_image()
    run()
