#!/usr/bin/env python3
from opcode_table import OPCODES
from enum import IntEnum, IntFlag
from dataclasses import dataclass, replace, field
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

    @staticmethod
    def build_default() -> CPU:
        return CPU(
                a=0,
                x=0,
                y=0,
                s=0xFF,
                p=Flag.I,
                pc=0x8000,
                memory={}
            )

    def prepare_memory(value: int|None, address: int|None, x: int, y: int, op_type: MemoryOpType) -> CPU:
        pass

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

    @staticmethod
    def build(value: int, opcode: int, mode: str, x: int, y: int, op_type: MemoryOpType) -> (CPU, CPU):
        pc = 0x8000
        operand = get_operand(value if op_type == MemoryOpType.READ else None,
                            mode, x, y)
        address = AddressResolver.resolve_mode(mode, operand.encoded, x, y)

        # Prepare memory
        memory = address.copy()
        memory_ops = [MemoryOp(MemoryOpType.READ, pc, opcode)]

        memory[pc] = opcode
        for i in range(operand.length):
            operand_part = (operand.encoded >> (8*i)) & 0xFF
            memory[pc+1+i] = operand_part
            memory_ops.append(MemoryOp(MemoryOpType.READ, pc+i+1, operand_part))
        if address.address != None:
            memory[address.address] = value
            memory_ops.append(op_type, address.address, value)
        memory_ops += address.memory_ops

class MemoryOpType(IntEnum):
    READ = 0
    WRITE = 1

@dataclass
class MemoryOp:
    type: MemoryOpType
    address: int
    value: int

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

    def generate_test_bin(self) -> bytearray:
        data = bytearray()
        data += struct.pack("<B", len(self.name))
        data += struct.pack(f"<{len(self.name)}s", self.name.encode())
        data += struct.pack("<B", self.opcode)
        data += struct.pack("<H", self.operand)
        data += self.cpu_initial.pack()
        data += self.cpu_expected.pack()
        data += struct.pack("<B", len(self.memory_ops))
        for op in self.memory_ops:
            data += struct.pack("<BHB", op.type, op.address, op.value)
        return data

@dataclass
class ResolvedAddress:
    address: int | None
    memory: dict[int, int] = field(default_factory=dict)
    memory_ops: list[MemoryOp] = field(default_factory=list)

class AddressResolver:
    @staticmethod
    def resolve_mode(mode: str, operand: int, x: int, y: int, indirect_base_address: int = 0) -> ResolvedAddress:
        match mode:
            case "A": return ResolvedAddress(None)
            case "#imm": return ResolvedAddress(None)
            case "zpg": return AddressResolver.zero_page(operand)
            case "zpg,X": return AddressResolver.zero_page_x(operand, x)
            case "zpg,Y": return AddressResolver.zero_page_y(operand, y)
            case "abs": return AddressResolver.absolute(operand)
            case "abs,X": return AddressResolver.absolute_x(operand, x)
            case "abs,Y": return AddressResolver.absolute_y(operand, y)
            case "(ind)": return AddressResolver.indirect(operand, indirect_base_address)
            case "(ind,X)": return AddressResolver.indexed_indirect(operand, x, indirect_base_address)
            case "(ind),Y": return AddressResolver.indirect_indexed(operand, y, indirect_base_address)
            case "rel": return ResolvedAddress(None)
            case "": return ResolvedAddress(None)

    @staticmethod
    def zero_page(operand: int) -> ResolvedAddress:
        return ResolvedAddress(operand & 0xFF)

    @staticmethod
    def zero_page_x(operand: int, x: int) -> ResolvedAddress:
        return ResolvedAddress((operand+x) & 0xFF)

    @staticmethod
    def zero_page_y(operand: int, y: int) -> ResolvedAddress:
        return ResolvedAddress((operand+y) & 0xFF)

    @staticmethod
    def absolute(operand: int) -> ResolvedAddress:
        return ResolvedAddress(operand)

    @staticmethod
    def absolute_x(operand: int, x: int) -> ResolvedAddress:
        return ResolvedAddress(operand+x)

    @staticmethod
    def absolute_y(operand: int, y: int) -> ResolvedAddress:
        return ResolvedAddress(operand+y)

    @staticmethod
    def indirect(operand: int, indirect_base_address: int) -> ResolvedAddress:
        low_addr = operand & 0xFF
        high_addr = (operand+1) & 0xFF
        low = indirect_base_address & 0xFF
        high = (indirect_base_address >> 8) & 0xFF
        return ResolvedAddress(low | (high<<8), memory_ops=[
            MemoryOp(MemoryOpType.READ, operand, low),
            MemoryOp(MemoryOpType.READ, operand+1, high)
        ], memory={
            low_addr: low,
            high_addr: high
        })

    @staticmethod
    def indexed_indirect(operand: int, x: int, indirect_base_address: int) -> ResolvedAddress:
        low_addr = (operand+x) & 0xFF
        high_addr = (operand+x+1) & 0xFF
        low = indirect_base_address & 0xFF
        high = (indirect_base_address >> 8) & 0xFF
        return ResolvedAddress(low | (high<<8), memory_ops=[
            MemoryOp(MemoryOpType.READ, low_addr, low),
            MemoryOp(MemoryOpType.READ, high_addr, high)
        ], memory={
            low_addr: low,
            high_addr: high
        })

    @staticmethod
    def indirect_indexed(operand: int, y: int, indirect_base_address: int) -> ResolvedAddress:
        low_addr = operand
        high_addr = (operand+1) & 0xFF
        low = indirect_base_address & 0xFF
        high = (indirect_base_address >> 8) & 0xFF
        return ResolvedAddress((low | (high<<8)) + y, memory_ops=[
            MemoryOp(MemoryOpType.READ, low_addr, low),
            MemoryOp(MemoryOpType.READ, high_addr, high)
        ], memory={
            low_addr: low,
            high_addr: high
        })

@dataclass
class Operand:
    encoded: int
    length: int
    indirect_address: int = 0

def get_operand(value: int|None, mode: str, x: int, y: int) -> Operand:
    match mode:
        case "A": return None
        case "#imm":
            # Write instructions should pass None as their value
            if value == None:
                raise Exception("Write instructions shouldn't be able to use immediate addressing")
            else:
                return Operand(value, 1)
        case "zpg": return Operand(0xB2, 1)
        case "zpg,X": return Operand(0xB1, 1)
        case "zpg,Y": return Operand(0xB4, 1)
        case "abs": return Operand(0x0B3F, 2)
        case "abs,X": return Operand(0x0B3F, 2)
        case "abs,Y": return Operand(0x0B3F, 2)
        case "(ind)": return Operand(0xC1, 1, indirect_address=0x0B3F)
        case "(ind,X)": return Operand(0x03, 1, indirect_address=0x0B3F)
        case "(ind),Y": return Operand(0xC4, 1, indirect_address=0x0B3F)
        case "rel": raise NotImplementedError()
        case "": return None

# Indirect indexed: 
# opcode
# zpg address
# zpg   -> low addr
# zpg+1 -> high addr
# addr  -> value

def lda_test(instructions) -> list[TestCase]:
    tests = []

    for mode, opcode in instructions["LDA"].items():
        value = 0
        x = 0xFF
        y = 0xB1
        operand = get_operand(value, mode, x, y)
        address = AddressResolver.resolve_mode(mode, operand.encoded, x, y, indirect_base_address=operand.indirect_address)
        initial = CPU.build_default().prepare_memory(address, x, y, op_type)
        # CPU(
        #     a=value,
        #     x=x,
        #     y=y,
        #     s=0xFF,
        #     p=0,
        #     pc=0x8000,
        #     memory=address.memory.copy() | operand.memory
        # )

        initial.memory[initial.pc] = opcode
        memory_ops = [MemoryOp(MemoryOpType.READ, initial.pc, opcode)]
        for i in range(operand.length):
            initial.memory[initial.pc+1+i] = (operand.encoded >> (8*i)) & 0xFF
            memory_ops.append(MemoryOp(
                    MemoryOpType.READ,
                    initial.pc+i+1,
                    (operand.encoded >> (8*i)) & 0xFF))
        memory_ops += address.memory_ops
        if address.address != None:
            initial.memory[address.address] = value
            memory_ops.append(MemoryOp(MemoryOpType.READ, address.address, value))

        expected = replace(initial,
                           p=initial.p | Flag.Z,
                           pc=initial.pc+operand.length+1)

        case = TestCase(
            name=f"LDA {mode}",
            opcode=opcode,
            operand=value,
            cpu_initial=initial,
            cpu_expected=expected,
            memory_ops=memory_ops,
        )
        tests.append(case)

    return tests

def sta_test(instructions) -> list[TestCase]:
    tests = []

    for mode, opcode in instructions["STA"].items():
        value = 42
        x = 0xFF
        y = 0xB1
        # operand = get_operand(None, mode, x, y)
        # address = AddressResolver.resolve_mode(mode, operand.encoded, x, y, indirect_base_address=operand.indirect_address)
        # initial = replace(CPU.build_default(),
        #         a=value,
        #         x=x,
        #         y=y,
        #         # TODO: Make a merge function that checks for collision in
        #         # memory needs. It should also handle inserting opcode and
        #         # operands into the ROM.
        #         memory=address.memory.copy() | operand.memory
        #     )
        #
        # initial.memory[initial.pc] = opcode
        # memory_ops = [MemoryOp(MemoryOpType.READ, initial.pc, opcode)]
        # for i in range(operand.length):
        #     initial.memory[initial.pc+1+i] = (operand.encoded >> (8*i)) & 0xFF
        #     memory_ops.append(MemoryOp(
        #             MemoryOpType.READ,
        #             initial.pc+i+1,
        #             (operand.encoded >> (8*i)) & 0xFF))
        # memory_ops += address.memory_ops
        # assert(address.address != None)
        # memory_ops.append(MemoryOp(MemoryOpType.WRITE, address.address, value))
        #
        # expected = replace(initial, pc=initial.pc+operand.length+1)
        # expected.memory[address.address] = value

        # case = TestCase(
        #     name=f"STA {mode}",
        #     opcode=opcode,
        #     operand=value,
        #     cpu_initial=initial,
        #     cpu_expected=expected,
        #     memory_ops=memory_ops,
        # )
        # case.generate_test_bin()
        # tests.append(case)

    return tests

def build_instruction_table() -> dict[str, int]:
    instructions = {}
    for opcode, metadata in OPCODES.items():
        op = metadata["op"]
        mode = metadata["mode"]
        instructions.setdefault(op, {})[mode] = opcode
    return instructions

def main():
    instructions = build_instruction_table()

    tests = []
    # tests += lda_test(instructions)
    tests += sta_test(instructions)

    data = bytearray()
    data += struct.pack("<H", len(tests))
    for test in tests:
        data += test.generate_test_bin()
    with open("tests/bigboy.bin", "wb") as f:
        f.write(data)

if __name__ == "__main__":
    main()
