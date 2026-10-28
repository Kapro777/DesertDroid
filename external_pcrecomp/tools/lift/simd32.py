"""
simd32.py - MMX/SSE/SSE2 -> C for the global-register lifter (lift32.py).

lift32 had MMX for the ops one MMX rasterizer needed and no SSE at all: an
`movss` became a comment, and every other xmm instruction fell to the
"UNIMPLEMENTED" branch, which was a null statement. Need for Speed
Underground 2 (2004) has ~3,600 SSE, ~2,500 SSE2 and ~16,700 MMX
instructions in reached code, much of it vector math that feeds the renderer,
so "compiles" meant "draws nothing" with no error anywhere.

One emitter for both register files. Every packed integer op exists as an MMX
op on 64 bits and an SSE2 op on 128 bits with the same lane semantics, so an
operand is read into a V128 (runtime/recomp32/recomp_simd.h) whatever its
width, the lanes are computed, and the result is written back at the width
of the destination: n_lanes = width / lane_bits.

The semantics are ported from lift32_cpu.py's SSE emitter (which difftest has
not covered either) and extended with what NFSU2 needs. 3DNow! is NOT
modelled: it lifts to RECOMP_UNIMPL, and the runtime's cpuid reports no
3DNow!, so CPU-dispatched code never chooses those paths.

Interface: SimdEmitter(lifter).handles(insn) / .emit(insn) -> [C lines]
The lifter provides _fmt_mem_addr(mem), _fmt_read(op), _fmt_write(op, v).
"""
import re
from capstone.x86 import X86_OP_REG, X86_OP_IMM, X86_OP_MEM

XMM_BASE = None  # filled lazily from capstone reg ids
_ARITH = {"add": "+", "sub": "-", "mul": "*", "div": "/"}
_CMP_PRED = {
    "eq": "(_a == _b)", "lt": "(_a < _b)", "le": "(_a <= _b)",
    "unord": "(_a != _a || _b != _b)", "neq": "!(_a == _b)",
    "nlt": "!(_a < _b)", "nle": "!(_a <= _b)", "ord": "!(_a != _a || _b != _b)",
}
_CMP_RE = re.compile(r"^cmp(%s)(ss|sd|ps|pd)$" % "|".join(_CMP_PRED))

# packed integer: name -> (kind, lane bits, signed)
PINT = {}
for _w, _b in (("b", 8), ("w", 16), ("d", 32), ("q", 64)):
    PINT["padd" + _w] = ("wrap+", _b, "u")
    PINT["psub" + _w] = ("wrap-", _b, "u")
for _w, _b in (("b", 8), ("w", 16)):
    PINT["padds" + _w] = ("sat+", _b, "i")
    PINT["psubs" + _w] = ("sat-", _b, "i")
    PINT["paddus" + _w] = ("sat+", _b, "u")
    PINT["psubus" + _w] = ("sat-", _b, "u")
for _w, _b in (("b", 8), ("w", 16), ("d", 32)):
    PINT["pcmpeq" + _w] = ("cmpeq", _b, "u")
    PINT["pcmpgt" + _w] = ("cmpgt", _b, "i")
PINT.update({
    "pmullw": ("mullo", 16, "i"), "pmulhw": ("mulhi", 16, "i"),
    "pmulhuw": ("mulhi", 16, "u"),
    "pmaxsw": ("max", 16, "i"), "pminsw": ("min", 16, "i"),
    "pmaxub": ("max", 8, "u"), "pminub": ("min", 8, "u"),
    "pavgb": ("avg", 8, "u"), "pavgw": ("avg", 16, "u"),
})
LOGIC = {"pand": "&", "por": "|", "pxor": "^", "pandn": "andn",
         "andps": "&", "andpd": "&", "orps": "|", "orpd": "|",
         "xorps": "^", "xorpd": "^", "andnps": "andn", "andnpd": "andn"}
SHIFT = {"psllw": ("<<", 16, False), "pslld": ("<<", 32, False), "psllq": ("<<", 64, False),
         "psrlw": (">>", 16, False), "psrld": (">>", 32, False), "psrlq": (">>", 64, False),
         "psraw": (">>", 16, True), "psrad": (">>", 32, True)}
PACK = {"packsswb": (16, 8, "i"), "packuswb": (16, 8, "u"), "packssdw": (32, 16, "i")}
PUNPCK = {}
for _h, _hi in (("l", False), ("h", True)):
    for _w, _b in (("bw", 8), ("wd", 16), ("dq", 32), ("qdq", 64)):
        PUNPCK["punpck" + _h + _w] = (_b, _hi)
MOV128 = {"movaps", "movups", "movapd", "movupd", "movdqa", "movdqu",
          "movntps", "movntpd", "movntdq"}
NOOPS = {"emms", "femms", "sfence", "lfence", "mfence", "prefetchnta",
         "prefetcht0", "prefetcht1", "prefetcht2", "prefetch", "prefetchw"}
OTHER = {"movss", "movsd", "movq", "movd", "movlps", "movlpd", "movhps", "movhpd",
         "movhlps", "movlhps", "movq2dq", "movdq2q", "movntq", "movmskps",
         "movmskpd", "pmovmskb", "pextrw", "pinsrw", "shufps", "shufpd", "pshufd",
         "pshuflw", "pshufhw", "pshufw", "unpcklps", "unpckhps", "unpcklpd",
         "unpckhpd", "pslldq", "psrldq", "pmaddwd", "psadbw",
         "sqrtps", "sqrtpd", "sqrtss", "sqrtsd", "rcpps", "rcpss", "rsqrtps",
         "rsqrtss", "minps", "maxps", "minpd", "maxpd", "minss", "maxss", "minsd",
         "maxsd", "ucomiss", "comiss", "ucomisd", "comisd",
         "cvtsi2ss", "cvtsi2sd", "cvttss2si", "cvttsd2si", "cvtss2si", "cvtsd2si",
         "cvtss2sd", "cvtsd2ss", "cvtdq2ps", "cvtps2dq", "cvttps2dq", "cvtps2pd",
         "cvtpd2ps", "cvtdq2pd", "cvtpi2ps", "cvtps2pi", "cvttps2pi", "cvtpi2pd",
         "cvtpd2pi", "cvttpd2pi", "cvtpd2dq", "cvttpd2dq",
         "stmxcsr", "ldmxcsr", "maskmovq", "pswapd"}
FARITH = {o + w for o in _ARITH for w in ("ps", "pd", "ss", "sd")}
ALL = set(PINT) | set(LOGIC) | set(SHIFT) | set(PACK) | set(PUNPCK) | MOV128 | NOOPS | OTHER | FARITH
# Mnemonics that are also non-SIMD instructions (string movsd, cmpsd).
AMBIG = {"movsd", "cmpsd"}


def _is3dnow(m):
    return m.startswith("pf") or m in ("pi2fd", "pi2fw", "pf2id", "pf2iw", "pavgusb",
                                       "pmulhrw", "pswapd")


class SimdEmitter:
    def __init__(self, lifter):
        self.L = lifter

    # ---- operand classification ----
    def _rn(self, op):
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
        if not hasattr(SimdEmitter, "_md"):
            SimdEmitter._md = Cs(CS_ARCH_X86, CS_MODE_32)
        return SimdEmitter._md.reg_name(op.reg)

    def is_x(self, op):
        return op.type == X86_OP_REG and self._rn(op).startswith("xmm")

    def is_m(self, op):
        return op.type == X86_OP_REG and self._rn(op).startswith("mm")

    def idx(self, op):
        n = self._rn(op)
        return int(n[3:] if n.startswith("xmm") else n[2:])

    def handles(self, insn):
        m = insn.mnemonic
        if m.startswith("rep ") or m.startswith("repne "):
            return False
        ops = insn.operands or []
        anyx = any(self.is_x(o) for o in ops)
        anym = any(self.is_m(o) for o in ops)
        if m in AMBIG:
            return anyx
        if _CMP_RE.match(m):
            return anyx
        if _is3dnow(m) or m in ("femms",):
            return True
        # Everything else in the set is SIMD whatever its operands look like:
        # `cvttss2si eax, [mem]` has no xmm operand at all.
        return m in ALL

    # ---- operand access ----
    def addr(self, op):
        return self.L._fmt_mem_addr(op.mem)

    def width(self, op):
        """bits of a vector operand: 128 for xmm/m128, 64 for mm/m64"""
        if self.is_x(op):
            return 128
        if self.is_m(op):
            return 64
        return op.size * 8

    def vread(self, op, w=None):
        """operand as a V128 expression"""
        if self.is_x(op):
            return f"g_xmm[{self.idx(op)}]"
        if self.is_m(op):
            return f"v128_q(g_mm[{self.idx(op)}])"
        if op.type == X86_OP_MEM:
            w = w or op.size * 8
            a = self.addr(op)
            if w == 128:
                return f"rdv128({a})"
            if w == 64:
                return f"v128_q(rdm64({a}))"
            if w == 32:
                return f"v128_d(rdm32({a}))"
        raise NotImplementedError(f"vread {op.type}")

    def vwrite(self, op, expr):
        if self.is_x(op):
            return f"g_xmm[{self.idx(op)}] = {expr};"
        if self.is_m(op):
            return f"g_mm[{self.idx(op)}] = ({expr}).u64[0];"
        if op.type == X86_OP_MEM:
            a = self.addr(op)
            if op.size == 16:
                return f"wrv128({a}, {expr});"
            if op.size == 8:
                return f"wrm64({a}, ({expr}).u64[0]);"
            if op.size == 4:
                return f"wrm32({a}, ({expr}).u32[0]);"
        raise NotImplementedError("vwrite")

    def ss(self, op):
        if self.is_x(op):
            return f"g_xmm[{self.idx(op)}].f32[0]"
        return f"rdmf32({self.addr(op)})"

    def sd(self, op):
        if self.is_x(op):
            return f"g_xmm[{self.idx(op)}].f64[0]"
        return f"rdmf64({self.addr(op)})"

    def gpr_read(self, op):
        return self.L._fmt_read(op)

    def gpr_write(self, op, v):
        return self.L._fmt_write(op, v) + ";"

    # ---- emission ----
    def emit(self, insn):
        try:
            return self._emit(insn)
        except NotImplementedError as e:
            return [f'RECOMP_UNIMPL(0x{insn.address:08X}u, "{insn.mnemonic} {insn.op_str} ({e})");']

    def _emit(self, insn):
        m = insn.mnemonic
        ops = insn.operands or []
        d = ops[0] if ops else None
        s = ops[1] if len(ops) > 1 else None
        L = self.L

        if _is3dnow(m):
            return [f'RECOMP_UNIMPL(0x{insn.address:08X}u, "3dnow {m} {insn.op_str}");']
        if m in NOOPS:
            return [f"/* {m} */"]
        if m == "stmxcsr":
            return [f"MEM32({self.addr(d)}) = g_mxcsr;"]
        if m == "ldmxcsr":
            return [f"g_mxcsr = MEM32({self.addr(d)});"]

        # ---- moves ----
        if m in MOV128:
            return [self.vwrite(d, self.vread(s, 128))]
        if m in ("movss", "movsd"):
            wide = m == "movsd"
            rd = self.sd if wide else self.ss
            if self.is_x(d) and self.is_x(s):
                return [f"{rd(d)} = {rd(s)};" if not wide else
                        f"g_xmm[{self.idx(d)}].u64[0] = g_xmm[{self.idx(s)}].u64[0];"]
            if self.is_x(d):
                if wide:
                    return [f"g_xmm[{self.idx(d)}] = v128_q(rdm64({self.addr(s)}));"]
                return [f"g_xmm[{self.idx(d)}] = v128_d(rdm32({self.addr(s)}));"]
            if wide:
                return [f"wrm64({self.addr(d)}, g_xmm[{self.idx(s)}].u64[0]);"]
            return [f"wrm32({self.addr(d)}, g_xmm[{self.idx(s)}].u32[0]);"]
        if m == "movd":
            if self.is_x(d) or self.is_m(d):
                v = self.gpr_read(s) if s.type == X86_OP_REG else f"rdm32({self.addr(s)})"
                if self.is_x(d):
                    return [f"g_xmm[{self.idx(d)}] = v128_d((uint32_t)({v}));"]
                return [f"g_mm[{self.idx(d)}] = (uint64_t)(uint32_t)({v});"]
            src = f"g_xmm[{self.idx(s)}].u32[0]" if self.is_x(s) else f"(uint32_t)g_mm[{self.idx(s)}]"
            if d.type == X86_OP_MEM:
                return [f"wrm32({self.addr(d)}, {src});"]
            return [self.gpr_write(d, src)]
        if m in ("movq", "movntq"):
            # 64 bits; an xmm destination zeroes its high quadword
            if self.is_x(d):
                src = (f"g_xmm[{self.idx(s)}].u64[0]" if self.is_x(s) else
                       f"g_mm[{self.idx(s)}]" if self.is_m(s) else f"rdm64({self.addr(s)})")
                return [f"g_xmm[{self.idx(d)}] = v128_q({src});"]
            src = (f"g_xmm[{self.idx(s)}].u64[0]" if self.is_x(s) else
                   f"g_mm[{self.idx(s)}]" if self.is_m(s) else f"rdm64({self.addr(s)})")
            if self.is_m(d):
                return [f"g_mm[{self.idx(d)}] = {src};"]
            return [f"wrm64({self.addr(d)}, {src});"]
        if m == "movq2dq":
            return [f"g_xmm[{self.idx(d)}] = v128_q(g_mm[{self.idx(s)}]);"]
        if m == "movdq2q":
            return [f"g_mm[{self.idx(d)}] = g_xmm[{self.idx(s)}].u64[0];"]
        if m in ("movlps", "movlpd", "movhps", "movhpd"):
            lane = 1 if m[3] == "h" else 0
            if self.is_x(d):
                return [f"g_xmm[{self.idx(d)}].u64[{lane}] = rdm64({self.addr(s)});"]
            return [f"wrm64({self.addr(d)}, g_xmm[{self.idx(s)}].u64[{lane}]);"]
        if m == "movhlps":
            return [f"g_xmm[{self.idx(d)}].u64[0] = g_xmm[{self.idx(s)}].u64[1];"]
        if m == "movlhps":
            return [f"g_xmm[{self.idx(d)}].u64[1] = g_xmm[{self.idx(s)}].u64[0];"]
        if m in ("pmovmskb", "movmskps", "movmskpd"):
            w = self.width(s)
            lane, bits = {"pmovmskb": ("u8", 8), "movmskps": ("u32", 32), "movmskpd": ("u64", 64)}[m]
            n = w // bits
            expr = " | ".join(f"((uint32_t)((_s.{lane}[{i}] >> {bits - 1}) & 1u) << {i})" for i in range(n))
            return [f"{{ V128 _s = {self.vread(s)}; {self.gpr_write(d, expr)} }}"]
        if m == "pextrw":
            i = ops[2].imm & (7 if self.is_x(s) else 3)
            return [f"{{ V128 _s = {self.vread(s)}; {self.gpr_write(d, f'(uint32_t)_s.u16[{i}]')} }}"]
        if m == "pinsrw":
            i = ops[2].imm & (7 if self.is_x(d) else 3)
            v = self.gpr_read(s) if s.type == X86_OP_REG else f"MEM16({self.addr(s)})"
            return [f"{{ V128 _d = {self.vread(d)}; _d.u16[{i}] = (uint16_t)({v}); {self.vwrite(d, '_d')} }}"]

        # ---- logic ----
        if m in LOGIC:
            op = LOGIC[m]
            body = ("_d.u64[0] = ~_d.u64[0] & _s.u64[0]; _d.u64[1] = ~_d.u64[1] & _s.u64[1];"
                    if op == "andn" else f"_d.u64[0] {op}= _s.u64[0]; _d.u64[1] {op}= _s.u64[1];")
            return [f"{{ V128 _s = {self.vread(s, self.width(d))}; V128 _d = {self.vread(d)}; {body} {self.vwrite(d, '_d')} }}"]

        # ---- packed integer ----
        if m in PINT:
            kind, bits, sg = PINT[m]
            w = self.width(d)
            n = w // bits
            T = f"{sg}{bits}"
            UT = f"uint{bits}_t"
            sat = f"simd_sat_{sg}{bits}" if bits <= 16 else None
            parts = []
            for i in range(n):
                a, b = f"_d.{T}[{i}]", f"_s.{T}[{i}]"
                r = f"_r.u{bits}[{i}]"
                if kind == "wrap+": parts.append(f"{r} = ({UT})(_d.u{bits}[{i}] + _s.u{bits}[{i}]);")
                elif kind == "wrap-": parts.append(f"{r} = ({UT})(_d.u{bits}[{i}] - _s.u{bits}[{i}]);")
                elif kind == "sat+": parts.append(f"{r} = ({UT}){sat}((int32_t){a} + (int32_t){b});")
                elif kind == "sat-": parts.append(f"{r} = ({UT}){sat}((int32_t){a} - (int32_t){b});")
                elif kind == "cmpeq": parts.append(f"{r} = ({a} == {b}) ? ({UT})~({UT})0 : 0;")
                elif kind == "cmpgt": parts.append(f"{r} = ({a} > {b}) ? ({UT})~({UT})0 : 0;")
                elif kind == "mullo": parts.append(f"{r} = ({UT})((int32_t){a} * (int32_t){b});")
                elif kind == "mulhi":
                    ct = "int32_t" if sg == "i" else "uint32_t"
                    parts.append(f"{r} = ({UT})((({ct}){a} * ({ct}){b}) >> 16);")
                elif kind == "max": parts.append(f"{r} = ({UT})({a} > {b} ? {a} : {b});")
                elif kind == "min": parts.append(f"{r} = ({UT})({a} < {b} ? {a} : {b});")
                elif kind == "avg": parts.append(f"{r} = ({UT})(((uint32_t){a} + (uint32_t){b} + 1u) >> 1);")
            return [f"{{ V128 _s = {self.vread(s, w)}; V128 _d = {self.vread(d)}; V128 _r = _d; "
                    + " ".join(parts) + f" {self.vwrite(d, '_r')} }}"]
        if m == "pmaddwd":
            w = self.width(d)
            n = w // 32
            parts = " ".join(f"_r.i32[{i}] = (int32_t)((int32_t)_d.i16[{2*i}] * _s.i16[{2*i}] + (int32_t)_d.i16[{2*i+1}] * _s.i16[{2*i+1}]);"
                             for i in range(n))
            return [f"{{ V128 _s = {self.vread(s, w)}; V128 _d = {self.vread(d)}; V128 _r = _d; {parts} {self.vwrite(d, '_r')} }}"]
        if m == "psadbw":
            w = self.width(d)
            groups = w // 64
            parts = []
            for g in range(groups):
                sm = " + ".join(f"(uint32_t)abs((int)_d.u8[{g*8+i}] - (int)_s.u8[{g*8+i}])" for i in range(8))
                parts.append(f"_r.u64[{g}] = (uint64_t)({sm});")
            return [f"{{ V128 _s = {self.vread(s, w)}; V128 _d = {self.vread(d)}; V128 _r = _d; " + " ".join(parts) + f" {self.vwrite(d, '_r')} }}"]
        if m in PACK:
            sbits, dbits, sg = PACK[m]
            w = self.width(d)
            half = w // sbits
            sat = f"simd_sat_{sg}{dbits}"
            parts = []
            for j in range(half):
                parts.append(f"_r.u{dbits}[{j}] = (uint{dbits}_t){sat}((int32_t)_d.i{sbits}[{j}]);")
            for j in range(half):
                parts.append(f"_r.u{dbits}[{half + j}] = (uint{dbits}_t){sat}((int32_t)_s.i{sbits}[{j}]);")
            return [f"{{ V128 _s = {self.vread(s, w)}; V128 _d = {self.vread(d)}; V128 _r = _d; " + " ".join(parts) + f" {self.vwrite(d, '_r')} }}"]
        if m in PUNPCK:
            bits, hi = PUNPCK[m]
            w = self.width(d)
            n = w // bits
            base = n // 2 if hi else 0
            parts = " ".join(f"_r.u{bits}[{2*i}] = _d.u{bits}[{base+i}]; _r.u{bits}[{2*i+1}] = _s.u{bits}[{base+i}];"
                             for i in range(n // 2))
            return [f"{{ V128 _s = {self.vread(s, w)}; V128 _d = {self.vread(d)}; V128 _r = _d; {parts} {self.vwrite(d, '_r')} }}"]
        if m in SHIFT:
            op, bits, arith = SHIFT[m]
            w = self.width(d)
            n = w // bits
            if s.type == X86_OP_IMM:
                cnt = f"uint64_t _n = {s.imm & 0xFF}u;"
            else:
                cnt = f"uint64_t _n = ({self.vread(s, w)}).u64[0];"
            if arith:
                body = f"for (_i = 0; _i < {n}; _i++) _d.i{bits}[_i] = (int{bits}_t)(_d.i{bits}[_i] >> (_n > {bits-1}u ? {bits-1}u : (unsigned)_n));"
            else:
                body = (f"for (_i = 0; _i < {n}; _i++) _d.u{bits}[_i] = _n > {bits-1}u ? 0 : "
                        f"(uint{bits}_t)(_d.u{bits}[_i] {op} (unsigned)_n);")
            return [f"{{ {cnt} int _i; V128 _d = {self.vread(d)}; {body} {self.vwrite(d, '_d')} }}"]
        if m in ("pslldq", "psrldq"):
            k = min(s.imm & 0xFF, 16)
            pick = (f"(_i >= {k}) ? _d.u8[_i - {k}] : 0" if m == "pslldq"
                    else f"(_i + {k} < 16) ? _d.u8[_i + {k}] : 0")
            return [f"{{ V128 _d = {self.vread(d)}, _t; int _i; for (_i = 0; _i < 16; _i++) _t.u8[_i] = {pick}; {self.vwrite(d, '_t')} }}"]

        # ---- shuffles ----
        if m in ("shufps", "shufpd", "pshufd", "pshuflw", "pshufhw", "pshufw"):
            imm = ops[2].imm & 0xFF
            if m == "shufpd":
                body = f"_r.u64[0] = _d.u64[{imm & 1}]; _r.u64[1] = _s.u64[{(imm >> 1) & 1}];"
            elif m == "shufps":
                body = " ".join(f"_r.u32[{i}] = {'_d' if i < 2 else '_s'}.u32[{(imm >> (2*i)) & 3}];" for i in range(4))
            elif m == "pshufd":
                body = " ".join(f"_r.u32[{i}] = _s.u32[{(imm >> (2*i)) & 3}];" for i in range(4))
            elif m == "pshufw":
                body = " ".join(f"_r.u16[{i}] = _s.u16[{(imm >> (2*i)) & 3}];" for i in range(4))
            else:
                b = 4 if m == "pshufhw" else 0
                keep = 0 if b else 1
                body = f"_r.u64[{keep}] = _s.u64[{keep}]; " + " ".join(
                    f"_r.u16[{b+i}] = _s.u16[{b + ((imm >> (2*i)) & 3)}];" for i in range(4))
            return [f"{{ V128 _s = {self.vread(s, self.width(d))}; V128 _d = {self.vread(d)}; V128 _r = _d; {body} {self.vwrite(d, '_r')} }}"]
        if m in ("unpcklps", "unpckhps", "unpcklpd", "unpckhpd"):
            lo = m[5] == "l"     # unpcklps / unpckhps: the letter after "unpck"
            if m.endswith("pd"):
                k = 0 if lo else 1
                body = f"_r.u64[0] = _d.u64[{k}]; _r.u64[1] = _s.u64[{k}];"
            else:
                a, b = (0, 1) if lo else (2, 3)
                body = (f"_r.u32[0] = _d.u32[{a}]; _r.u32[1] = _s.u32[{a}]; "
                        f"_r.u32[2] = _d.u32[{b}]; _r.u32[3] = _s.u32[{b}];")
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d = {self.vread(d)}; V128 _r; {body} {self.vwrite(d, '_r')} }}"]

        # ---- float arithmetic ----
        if m in FARITH:
            op, w = _ARITH[m[:-2]], m[-2:]
            if w in ("ss", "sd"):
                rd = self.sd if w == "sd" else self.ss
                ct = "double" if w == "sd" else "float"
                return [f"{{ {ct} _b = {rd(s)}; {rd(d)} = ({ct})({rd(d)} {op} _b); }}"]
            lane, n = ("f64", 2) if w == "pd" else ("f32", 4)
            body = " ".join(f"_d.{lane}[{i}] = _d.{lane}[{i}] {op} _s.{lane}[{i}];" for i in range(n))
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d = {self.vread(d)}; {body} {self.vwrite(d, '_d')} }}"]
        if m in ("sqrtss", "sqrtsd", "rcpss", "rsqrtss"):
            rd = self.sd if m == "sqrtsd" else self.ss
            f = {"sqrtss": "sqrtf(_b)", "sqrtsd": "sqrt(_b)", "rcpss": "1.0f / _b",
                 "rsqrtss": "1.0f / sqrtf(_b)"}[m]
            ct = "double" if m == "sqrtsd" else "float"
            return [f"{{ {ct} _b = {rd(s)}; {rd(d)} = {f}; }}"]
        if m in ("sqrtps", "sqrtpd", "rcpps", "rsqrtps"):
            lane, n = ("f64", 2) if m == "sqrtpd" else ("f32", 4)
            f = {"sqrtps": "sqrtf(_s.f32[%d])", "sqrtpd": "sqrt(_s.f64[%d])",
                 "rcpps": "1.0f / _s.f32[%d]", "rsqrtps": "1.0f / sqrtf(_s.f32[%d])"}[m]
            body = " ".join(f"_d.{lane}[{i}] = " + (f % i) + ";" for i in range(n))
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d = {self.vread(d)}; {body} {self.vwrite(d, '_d')} }}"]
        if m in ("minps", "maxps", "minpd", "maxpd", "minss", "maxss", "minsd", "maxsd"):
            cmp = "<" if m.startswith("min") else ">"
            w = m[-2:]
            if w in ("ss", "sd"):
                rd = self.sd if w == "sd" else self.ss
                ct = "double" if w == "sd" else "float"
                return [f"{{ {ct} _a = {rd(d)}, _b = {rd(s)}; {rd(d)} = (_a {cmp} _b) ? _a : _b; }}"]
            lane, n = ("f64", 2) if w == "pd" else ("f32", 4)
            body = " ".join(f"_d.{lane}[{i}] = (_d.{lane}[{i}] {cmp} _s.{lane}[{i}]) ? _d.{lane}[{i}] : _s.{lane}[{i}];" for i in range(n))
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d = {self.vread(d)}; {body} {self.vwrite(d, '_d')} }}"]

        # ---- compares ----
        if m in ("ucomiss", "comiss", "ucomisd", "comisd"):
            rd = self.sd if m.endswith("sd") else self.ss
            L._flag_state = ('eflags', '_flag_a, _flag_b')
            L._flag_seq += 1
            return [f"_flag_a = simd_comi_eflags((double)({rd(d)}), (double)({rd(s)})); _flag_b = 0; "
                    f"_flag_k = FK_EFLAGS; _cf = _flag_a & 1u;"]
        mm = _CMP_RE.match(m)
        if mm:
            pred, w = mm.group(1), mm.group(2)
            P = _CMP_PRED[pred]
            if w in ("ss", "sd"):
                rd = self.sd if w == "sd" else self.ss
                ct, lane, ones = ("double", "u64", "~(uint64_t)0") if w == "sd" else ("float", "u32", "0xFFFFFFFFu")
                return [f"{{ {ct} _a = {rd(d)}, _b = {rd(s)}; g_xmm[{self.idx(d)}].{lane}[0] = {P} ? {ones} : 0; }}"]
            ct, lane, il, n, ones = (("double", "f64", "u64", 2, "~(uint64_t)0") if w == "pd"
                                     else ("float", "f32", "u32", 4, "0xFFFFFFFFu"))
            body = " ".join(f"{{ {ct} _a = _d.{lane}[{i}], _b = _s.{lane}[{i}]; _r.{il}[{i}] = {P} ? {ones} : 0; }}" for i in range(n))
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d = {self.vread(d)}; V128 _r; {body} {self.vwrite(d, '_r')} }}"]

        # ---- conversions ----
        if m in ("cvtsi2ss", "cvtsi2sd"):
            v = self.gpr_read(s) if s.type == X86_OP_REG else f"rdm32({self.addr(s)})"
            tgt = self.sd(d) if m.endswith("sd") else self.ss(d)
            ct = "double" if m.endswith("sd") else "float"
            return [f"{tgt} = ({ct})(int32_t)({v});"]
        if m in ("cvttss2si", "cvttsd2si", "cvtss2si", "cvtsd2si"):
            rd = self.sd if "sd2" in m else self.ss
            fn = "simd_cvtt_i32" if m.startswith("cvtt") else "simd_cvt_i32"
            return [self.gpr_write(d, f"(uint32_t){fn}((double)({rd(s)}))")]
        if m == "cvtss2sd":
            return [f"{{ float _b = {self.ss(s)}; g_xmm[{self.idx(d)}].f64[0] = (double)_b; }}"]
        if m == "cvtsd2ss":
            return [f"{{ double _b = {self.sd(s)}; g_xmm[{self.idx(d)}].f32[0] = (float)_b; }}"]
        if m in ("cvtdq2ps", "cvtps2dq", "cvttps2dq"):
            if m == "cvtdq2ps":
                body = " ".join(f"_d.f32[{i}] = (float)_s.i32[{i}];" for i in range(4))
            else:
                fn = "simd_cvtt_i32" if m == "cvttps2dq" else "simd_cvt_i32"
                body = " ".join(f"_d.i32[{i}] = {fn}((double)_s.f32[{i}]);" for i in range(4))
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d = g_xmm[{self.idx(d)}]; {body} g_xmm[{self.idx(d)}] = _d; }}"]
        if m == "cvtps2pd":
            src = self.vread(s, 64) if s.type == X86_OP_MEM else self.vread(s)
            return [f"{{ V128 _s = {src}; V128 _d; _d.f64[0] = _s.f32[0]; _d.f64[1] = _s.f32[1]; g_xmm[{self.idx(d)}] = _d; }}"]
        if m == "cvtdq2pd":
            src = self.vread(s, 64) if s.type == X86_OP_MEM else self.vread(s)
            return [f"{{ V128 _s = {src}; V128 _d; _d.f64[0] = _s.i32[0]; _d.f64[1] = _s.i32[1]; g_xmm[{self.idx(d)}] = _d; }}"]
        if m in ("cvtpd2ps", "cvtpd2dq", "cvttpd2dq"):
            if m == "cvtpd2ps":
                body = "_d.f32[0] = (float)_s.f64[0]; _d.f32[1] = (float)_s.f64[1];"
            else:
                fn = "simd_cvtt_i32" if m.startswith("cvtt") else "simd_cvt_i32"
                body = f"_d.i32[0] = {fn}(_s.f64[0]); _d.i32[1] = {fn}(_s.f64[1]);"
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d; {body} _d.u64[1] = 0; g_xmm[{self.idx(d)}] = _d; }}"]
        if m == "cvtpi2ps":
            return [f"{{ V128 _s = {self.vread(s, 64)}; g_xmm[{self.idx(d)}].f32[0] = (float)_s.i32[0]; g_xmm[{self.idx(d)}].f32[1] = (float)_s.i32[1]; }}"]
        if m == "cvtpi2pd":
            return [f"{{ V128 _s = {self.vread(s, 64)}; g_xmm[{self.idx(d)}].f64[0] = _s.i32[0]; g_xmm[{self.idx(d)}].f64[1] = _s.i32[1]; }}"]
        if m in ("cvtps2pi", "cvttps2pi"):
            fn = "simd_cvtt_i32" if m.startswith("cvtt") else "simd_cvt_i32"
            src = self.vread(s, 64) if s.type == X86_OP_MEM else self.vread(s)
            return [f"{{ V128 _s = {src}; V128 _d; _d.i32[0] = {fn}((double)_s.f32[0]); _d.i32[1] = {fn}((double)_s.f32[1]); g_mm[{self.idx(d)}] = _d.u64[0]; }}"]
        if m in ("cvtpd2pi", "cvttpd2pi"):
            fn = "simd_cvtt_i32" if m.startswith("cvtt") else "simd_cvt_i32"
            return [f"{{ V128 _s = {self.vread(s, 128)}; V128 _d; _d.i32[0] = {fn}(_s.f64[0]); _d.i32[1] = {fn}(_s.f64[1]); g_mm[{self.idx(d)}] = _d.u64[0]; }}"]
        if m == "maskmovq":
            return [f"{{ V128 _s = {self.vread(d)}, _k = {self.vread(s)}; int _i; for (_i = 0; _i < 8; _i++) if (_k.u8[_i] & 0x80) MEM8(edi + _i) = _s.u8[_i]; }}"]
        raise NotImplementedError(m)
