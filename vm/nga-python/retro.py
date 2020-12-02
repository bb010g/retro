#!/usr/bin/env python3

# Nga: a Virtual Machine
# Copyright (c) 2010 - 2020, Charles Childers
# Floating Point I/O by Arland Childers, (c) 2020
# Optimizations and process() rewrite by Greg Copeland
# -----------------------------------------------------

import os, sys, math, time, struct, random, datetime
from struct import pack, unpack

from ClockDevice import Clock
from RNGDevice import RNG
from FileSystemDevice import FileSystem

from FloatStack import FloatStack
from IntegerStack import IntegerStack
from Memory import Memory


ip = 0
stack = IntegerStack()
address = []
memory = Memory("ngaImage", 1000000)

clock = Clock()
rng = RNG()

floats = FloatStack()
afloats = FloatStack()
files = FileSystem()


class Retro:
    def __init__(self):
        self.ip = 0
        self.stack = IntegerStack()
        self.address = IntegerStack()
        self.memory = Memory("ngaImage", 1000000)
        self.clock = Clock()
        self.rng = RNG()
        self.files = FileSystem()
        self.floats = FloatStack()
        self.afloats = FloatStack()
        self.interpreter = self.memory.fetch(self.find_entry("interpret") + 1)
        self.not_found = self.memory.fetch(self.find_entry("err:notfound") + 1)
        self.instructions = [
            self.i_no,
            self.i_li,
            self.i_du,
            self.i_dr,
            self.i_sw,
            self.i_pu,
            self.i_po,
            self.i_ju,
            self.i_ca,
            self.i_cc,
            self.i_re,
            self.i_eq,
            self.i_ne,
            self.i_lt,
            self.i_gt,
            self.i_fe,
            self.i_st,
            self.i_ad,
            self.i_su,
            self.i_mu,
            self.i_di,
            self.i_an,
            self.i_or,
            self.i_xo,
            self.i_sh,
            self.i_zr,
            self.i_ha,
            self.i_ie,
            self.i_iq,
            self.i_ii,
        ]

    def div_mod(self, a, b):
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

    def find_entry(self, named):
        header = self.memory.fetch(2)
        Done = False
        while header != 0 and not Done:
            if named == self.extract_string(header + 3):
                Done = True
            else:
                header = self.memory.fetch(header)
        return header

    def get_input(self):
        return ord(sys.stdin.read(1))

    def display_character(self):
        if self.stack.tos() > 0 and self.stack.tos() < 128:
            if self.stack.tos() == 8:
                sys.stdout.write(chr(self.stack.pop()))
                sys.stdout.write(chr(32))
                sys.stdout.write(chr(8))
            else:
                sys.stdout.write(chr(self.stack.pop()))
        else:
            sys.stdout.write("\033[2J\033[1;1H")
            self.stack.pop()
        sys.stdout.flush()

    def i_no(self):
        pass

    def i_li(self):
        self.ip += 1
        self.stack.push(self.memory.fetch(self.ip))

    def i_du(self):
        self.stack.dup()

    def i_dr(self):
        self.stack.drop()

    def i_sw(self):
        self.stack.swap()

    def i_pu(self):
        self.address.push(self.stack.pop())

    def i_po(self):
        self.stack.push(self.address.pop())

    def i_ju(self):
        self.ip = self.stack.pop() - 1

    def i_ca(self):
        self.address.push(self.ip)
        self.ip = self.stack.pop() - 1

    def i_cc(self):
        target = self.stack.pop()
        if self.stack.pop() != 0:
            self.address.push(self.ip)
            self.ip = target - 1

    def i_re(self):
        self.ip = self.address.pop()

    def i_eq(self):
        a = self.stack.pop()
        b = self.stack.pop()
        if b == a:
            self.stack.push(-1)
        else:
            self.stack.push(0)

    def i_ne(self):
        a = self.stack.pop()
        b = self.stack.pop()
        if b != a:
            self.stack.push(-1)
        else:
            self.stack.push(0)

    def i_lt(self):
        a = self.stack.pop()
        b = self.stack.pop()
        if b < a:
            self.stack.push(-1)
        else:
            self.stack.push(0)

    def i_gt(self):
        a = self.stack.pop()
        b = self.stack.pop()
        if b > a:
            self.stack.push(-1)
        else:
            self.stack.push(0)

    def i_fe(self):
        target = self.stack.pop()
        if target == -1:
            self.stack.push(self.stack.depth())
        elif target == -2:
            self.stack.push(self.address.depth())
        elif target == -3:
            self.stack.push(self.memory.size())
        elif target == -4:
            self.stack.push(2147483648)
        elif target == -5:
            self.stack.push(2147483647)
        else:
            self.stack.push(self.memory.fetch(target))

    def i_st(self):
        mi = self.stack.pop()
        self.memory.store(self.stack.pop(), mi)

    def i_ad(self):
        t = self.stack.pop()
        v = self.stack.pop()
        self.stack.push(unpack("=l", pack("=L", (t + v) & 0xFFFFFFFF))[0])

    def i_su(self):
        t = self.stack.pop()
        v = self.stack.pop()
        self.stack.push(unpack("=l", pack("=L", (v - t) & 0xFFFFFFFF))[0])

    def i_mu(self):
        t = self.stack.pop()
        v = self.stack.pop()
        self.stack.push(unpack("=l", pack("=L", (v * t) & 0xFFFFFFFF))[0])

    def i_di(self):
        t = self.stack.pop()
        v = self.stack.pop()
        b, a = self.div_mod(v, t)
        self.stack.push(unpack("=l", pack("=L", a & 0xFFFFFFFF))[0])
        self.stack.push(unpack("=l", pack("=L", b & 0xFFFFFFFF))[0])

    def i_an(self):
        t = self.stack.pop()
        m = self.stack.pop()
        self.stack.push(m & t)

    def i_or(self):
        t = self.stack.pop()
        m = self.stack.pop()
        self.stack.push(m | t)

    def i_xo(self):
        t = self.stack.pop()
        m = self.stack.pop()
        self.stack.push(m ^ t)

    def i_sh(self):
        t = self.stack.pop()
        v = self.stack.pop()

        if t < 0:
            v <<= t * -1
        else:
            v >>= t

        self.stack.push(v)

    def i_zr(self):
        if self.stack.tos() == 0:
            self.stack.pop()
            self.ip = self.address.pop()

    def i_ha(self):
        self.ip = 9000000

    def i_ie(self):
        self.stack.push(6)

    def i_iq(self):
        device = self.stack.pop()
        if device == 0:  # generic output
            self.stack.push(0)
            self.stack.push(0)
        if device == 1:  # floating point
            self.stack.push(1)
            self.stack.push(2)
        if device == 2:  # files
            self.stack.push(0)
            self.stack.push(4)
        if device == 3:  # rng
            self.stack.push(0)
            self.stack.push(10)
        if device == 4:  # time
            self.stack.push(0)
            self.stack.push(5)
        if device == 5:  # scripting
            self.stack.push(0)
            self.stack.push(9)

    float_instr = {
        0: lambda: floats.push(float(stack.pop())),  # number to float
        1: lambda: floats.push(float(extract_string(stack.pop()))),  # string to float
        2: lambda: stack.push(int(floats.pop())),  # float to number
        3: lambda: inject_string(str(floats.pop()), stack.pop()),  # float to string
        4: lambda: floats.add(),  # add
        5: lambda: floats.sub(),  # sub
        6: lambda: floats.mul(),  # mul
        7: lambda: floats.div(),  # div
        8: lambda: floats.floor(),  # floor
        9: lambda: floats.ceil(),  # ceil
        10: lambda: floats.sqrt(),  # sqrt
        11: lambda: stack.push(floats.eq()),  # eq
        12: lambda: stack.push(floats.neq()),  # -eq
        13: lambda: stack.push(floats.lt()),  # lt
        14: lambda: stack.push(floats.gt()),  # gt
        15: lambda: stack.push(floats.depth()),  # depth
        16: lambda: floats.dup(),  # dup
        17: lambda: floats.drop(),  # drop
        18: lambda: floats.swap(),  # swap
        19: lambda: floats.log(),  # log
        20: lambda: floats.pow(),  # pow
        21: lambda: floats.sin(),  # sin
        22: lambda: floats.cos(),  # cos
        23: lambda: floats.tan(),  # tan
        24: lambda: floats.asin(),  # asin
        25: lambda: floats.atan(),  # atan
        26: lambda: floats.acos(),  # acos
        27: lambda: afloats.push(floats.pop()),  # to alt
        28: lambda: floats.push(afloats.pop()),  # from alt
        29: lambda: stack.push(afloats.depth()),  # alt. depth
    }
    files_instr = {
        0: lambda: stack.push(file_open()),
        1: lambda: file_close(),
        2: lambda: stack.push(file_read()),
        3: lambda: file_write(),
        4: lambda: stack.push(file_pos()),
        5: lambda: file_seek(),
        6: lambda: stack.push(file_size()),
        7: lambda: file_delete(),
        8: lambda: 1 + 1,
    }

    rng_instr = {0: lambda: stack.push(rng())}

    clock_instr = {
        0: lambda: stack.push(int(time.time())),
        1: lambda: stack.push(clock["day"]),
        2: lambda: stack.push(clock["month"]),
        3: lambda: stack.push(clock["year"]),
        4: lambda: stack.push(clock["hour"]),
        5: lambda: stack.push(clock["minute"]),
        6: lambda: stack.push(clock["second"]),
        7: lambda: stack.push(clock["day_utc"]),
        8: lambda: stack.push(clock["month_utc"]),
        9: lambda: stack.push(clock["year_utc"]),
        10: lambda: stack.push(clock["hour_utc"]),
        11: lambda: stack.push(clock["minute_utc"]),
        12: lambda: stack.push(clock["second_utc"]),
    }

    def i_ii(self):
        device = self.stack.pop()
        if device == 0:  # generic output
            self.display_character()
        if device == 1:  # floating point
            action = self.stack.pop()
            float_instr[int(action)]()
        if device == 2:  # files
            action = self.stack.pop()
            files_instr[int(action)]()
        if device == 3:  # rng
            rng_instr[0]()
        if device == 4:  # clock
            action = self.stack.pop()
            clock_instr[int(action)]()
        if device == 5:  # scripting
            action = self.stack.pop()
            if action == 0:
                self.stack.push(len(sys.argv) - 2)
            if action == 1:
                a = self.stack.pop()
                b = self.stack.pop()
                self.stack.push(self.inject_string(sys.argv[a + 2], b))
            if action == 2:
                run_file(self.extract_string(self.stack.pop()))
            if action == 3:
                b = self.stack.pop()
                self.stack.push(self.inject_string(sys.argv[0], b))

    def validate_opcode(self, I0, I1, I2, I3):
        if (
            (I0 >= 0 and I0 <= 29)
            and (I1 >= 0 and I1 <= 29)
            and (I2 >= 0 and I2 <= 29)
            and (I3 >= 0 and I3 <= 29)
        ):
            return True
        else:
            return False

    def extract_string(self, at):
        s = ""
        while self.memory.fetch(at) != 0:
            s = s + chr(self.memory.fetch(at))
            at = at + 1
        return s

    def inject_string(self, s, to):
        for c in s:
            self.memory.store(ord(c), to)
            to = to + 1
        self.memory.store(0, to)

    def execute(self, word, notfound):
        self.ip = word
        if self.address.depth() == 0:
            self.address.push(0)
        while self.ip < 100000:
            if self.ip == notfound:
                print("ERROR: word not found!")
            opcode = self.memory.fetch(self.ip)
            I0 = opcode & 0xFF
            I1 = (opcode >> 8) & 0xFF
            I2 = (opcode >> 16) & 0xFF
            I3 = (opcode >> 24) & 0xFF
            if self.validate_opcode(I0, I1, I2, I3):
                # print("Bytecode: ", I0, I1, I2, I3, "at", self.ip)
                if I0 != 0:
                    self.instructions[I0]()
                if I1 != 0:
                    self.instructions[I1]()
                if I2 != 0:
                    self.instructions[I2]()
                if I3 != 0:
                    self.instructions[I3]()
            else:
                print("Invalid Bytecode: ", opcode, "at", self.ip)
                self.ip = 2000000
            if self.address.depth() == 0:
                self.ip = 2000000
            self.ip = self.ip + 1
        return

    def run(self):
        done = False
        while not done:
            line = input("\nOk> ")
            if line == "bye":
                done = True
            else:
                for token in line.split():
                    self.inject_string(token, 1024)
                    self.stack.push(1024)
                    self.execute(self.interpreter, self.not_found)

    def run_file(self, file):
        if not os.path.exists(file):
            print("File '{0}' not found".format(file))
            return

        in_block = False
        with open(file, "r") as source:
            for line in source.readlines():
                if line.rstrip() == "~~~":
                    in_block = not in_block
                elif in_block:
                    for token in line.strip().split():
                        self.inject_string(token, 1024)
                        self.stack.push(1024)
                        self.execute(self.interpreter, self.not_found)

    def update_image(self):
        import requests
        import shutil

        data = requests.get("http://forth.works/ngaImage", stream=True)
        with open("ngaImage", "wb") as f:
            data.raw.decode_content = True
            shutil.copyfileobj(data.raw, f)


if __name__ == "__main__":
    retro = Retro()
    if len(sys.argv) == 1:
        retro.run()

    if len(sys.argv) == 2:
        retro.run_file(sys.argv[1])

    sources = []

    if len(sys.argv) > 2:
        i = 1
        e = len(sys.argv)
        while i < e:
            param = sys.argv[i]
            if param == "-f":
                i += 1
                sources.append(sys.argv[i])
            i += 1

    if len(sys.argv) > 2 and sys.argv[1][0] != "-":
        retro.run_file(sys.argv[1])
    else:
        for source in sources:
            retro.run_file(source)
