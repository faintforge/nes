#!/usr/bin/env python3
from opcode_table import OPCODES
from enum import IntEnum, IntFlag
from dataclasses import dataclass, replace
import struct

# Mode determines operand and what needs to be where in memory as well as x and
# y registers for offset. Instruction determines what the CPU state starts and
# *should* ends as.

class Flag(IntFlag):
    NONE = 0
    C = 1 << 0
    Z = 1 << 1
    I = 1 << 2
    D = 1 << 3
    B = 1 << 4
    O = 1 << 6
    N = 1 << 7

@dataclass(frozen=True)
class CPU:
    a: int
    x: int
    y: int
    s: int
    p: int
    pc: int
    memory: dict[int, int]

    def pack(self) -> bytes:
        data = struct.pack("<BBBBBH",
                           self.a,
                           self.x,
                           self.y,
                           self.s,
                           self.p,
                           self.pc)
        data += struct.pack("<B", len(self.memory))
        for address, value in self.memory.items():
            data += struct.pack("<HB", address, value)
        return data

class MemoryOpType(IntEnum):
    READ = 0
    WRITE = 1

@dataclass
class MemoryOp:
    type: MemoryOpType
    address: int
    value: int

@dataclass
class ModeState:
    a: int
    x: int
    y: int
    operand: int
    operand_length: int
    memory: dict[int, int]
    memory_ops: list[MemoryOp]

class TestCase:
    # TODO: Scrape cycle count and instruction length from some website and
    # include those in the test case
    def __init__(self, name: str, opcode: int, operand: int, cpu_initial: CPU, cpu_expected: CPU, memory_ops: list[MemoryOp], cycles: int = 0):
        self.name = name
        self.opcode = opcode
        self.operand = operand
        self.cpu_initial = cpu_initial
        self.cpu_expected = cpu_expected
        self.memory_ops = memory_ops.copy()
        self.cycles = cycles

    def generate_test_bin(self):
        data = bytearray()
        data += struct.pack("<B", self.opcode)
        data += struct.pack("<H", self.operand)
        data += self.cpu_initial.pack()
        data += self.cpu_expected.pack()
        data += struct.pack("<B", len(self.memory_ops))
        for op in self.memory_ops:
            data += struct.pack("<BHB", op.type, op.address, op.value)
        return data

def mode_state(mode: str, value: int) -> ModeState:
    # TODO: Handle page boundaries cases
    state = ModeState(
            a=0,
            x=0,
            y=0,
            operand=0,
            operand_length=0,
            memory={},
            memory_ops=[]
        )

    if mode == "A":
        state.a = value

    elif mode == "#imm":
        state.operand = value
        state.operand_length = 1

    elif mode == "zpg":
        state.operand = 0x0A
        state.operand_length = 1
        state.memory = {state.operand: value}
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand, value)
        ]

    elif mode == "zpg,X":
        state.x = 0x0A
        state.operand = 0x01
        state.operand_length = 1
        state.memory = {state.operand+state.x: value}
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand+state.x, value)
        ]

    elif mode == "zpg,Y":
        state.y = 0x0A
        state.operand = 0x02
        state.operand_length = 1
        state.memory = {state.operand+state.y: value}
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand+state.y, value)
        ]

    elif mode == "abs":
        state.operand = 0x0A00
        state.operand_length = 2
        state.memory = {state.operand: value}
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand, value)
        ]

    elif mode == "abs,X":
        state.x = 0x1A
        state.operand = 0x0A01
        state.operand_length = 2
        state.memory = {state.operand+state.x: value}
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand+state.x, value)
        ]

    elif mode == "abs,Y":
        state.y = 0x2B
        state.operand = 0x0A02
        state.operand_length = 2
        state.memory = {state.operand+state.y: value}
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand+state.y, value)
        ]

    elif mode == "(ind)":
        state.operand = 0x0A10
        state.operand_length = 2
        state.memory = {
            state.operand+0: 0x02,
            state.operand+1: 0x0B,
            0x0B02: value,
        }
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand+0, 0x02),
            MemoryOp(MemoryOpType.READ, state.operand+1, 0x0B),
            MemoryOp(MemoryOpType.READ, 0x0B02, value),
        ]

    elif mode == "(ind,X)":
        state.x = 0x0C
        state.operand = 0x13
        state.operand_length = 1
        state.memory = {
            state.operand+state.x+0: 0x0A,
            state.operand+state.x+1: 0x0B,
            0x0B0A: value,
        }
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand+state.x+0, 0x0A),
            MemoryOp(MemoryOpType.READ, state.operand+state.x+1, 0x0B),
            MemoryOp(MemoryOpType.READ, 0x0B0A, value),
        ]

    elif mode == "(ind),Y":
        state.y = 0x0D
        state.operand = 0x0F
        state.operand_length = 1
        state.memory = {
            state.operand: 0x3C,
            state.operand+1: 0x0B,
            0x0B3C+state.y: value,
        }
        state.memory_ops = [
            MemoryOp(MemoryOpType.READ, state.operand+0, 0x3C),
            MemoryOp(MemoryOpType.READ, state.operand+1, 0x0B),
            MemoryOp(MemoryOpType.READ, 0x0B3C+state.y, value),
        ]

    elif mode == "rel":
        raise NotImplementedError("Relative addressing")

    return state

def lda_test(instructions):
    for mode, opcode in instructions["LDA"].items():
        value = 42
        state = mode_state(mode, value)

        initial = CPU(
            a=0,
            x=state.x,
            y=state.y,
            s=0xFF,
            p=0,
            pc=0x8000,
            memory=state.memory.copy(),
        )
        memory_ops = [MemoryOp(MemoryOpType.READ, initial.pc, opcode)]
        for i in range(state.operand_length):
            memory_ops.append(MemoryOp(
                    MemoryOpType.READ,
                    initial.pc+i+1,
                    (state.operand >> (8*i)) & 0xFF))
        memory_ops += state.memory_ops

        expected = replace(initial, a=value, pc=initial.pc+state.operand_length+1)

        case = TestCase(
            name=f"LDA {mode}",
            opcode=opcode,
            operand=state.operand,
            cpu_initial=initial,
            cpu_expected=expected,
            memory_ops=memory_ops,
        )

        with open(f"tests/{case.name}.bin", "wb") as f:
            bin = case.generate_test_bin()
            f.write(bin)

def build_instruction_table() -> dict[str, int]:
    instructions = {}
    for opcode, metadata in OPCODES.items():
        op = metadata["op"]
        mode = metadata["mode"]
        instructions.setdefault(op, {})[mode] = opcode
    return instructions

def main():
    instructions = build_instruction_table()
    lda_test(instructions)

if __name__ == "__main__":
    main()
