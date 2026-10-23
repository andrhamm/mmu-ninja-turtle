"""Protocol smoke test against vmmu (or any MMU3) on a serial path.
Usage: python3 -I smoke.py /dev/ttysNNN [load]   -> exit 0 on success."""
import os, sys, time, tty

def cx(crc, b):
    d = (crc ^ b) & 0xff
    for _ in range(8):
        d = ((d << 1) ^ 0x07) & 0xff if d & 0x80 else (d << 1) & 0xff
    return d

def req(code, value):
    crc = cx(cx(cx(cx(0, ord(code)), value), 0), 0)  # code, value, 16-bit param 0 (protocol.cpp RequestMsg::CRC)
    return f"{code}{value:x}*{crc:x}\n".encode()

fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY)
tty.setraw(fd)
os.set_blocking(fd, False)

def ask(code, value, timeout=0.5):
    os.write(fd, req(code, value))
    t0, buf = time.monotonic(), b""
    while not buf.endswith(b"\n") and time.monotonic() - t0 < timeout:
        try:
            buf += os.read(fd, 64)
        except BlockingIOError:
            time.sleep(0.0005)
    return buf.decode(errors="replace").strip(), (time.monotonic() - t0) * 1000

time.sleep(0.3)
fails = 0
# printer requires MMU FW 3.0.4 exactly: S0 major=3, S1 minor=0, S2 rev=4
for code, v, want in [("S", 0, "S0 A3*"), ("S", 1, "S1 A0*"), ("S", 2, "S2 A4*"), ("S", 3, "S3 A"), ("Q", 0, "X0 "), ("P", 0, "P0 A0*")]:
    r, ms = ask(code, v)
    ok = r.startswith(want)
    fails += not ok
    print(f"{code}{v} -> {r!r}  ({ms:.1f} ms){'' if ok else f'  FAIL (want {want!r})'}")

# modes: load (L1 must finish), load-error / tc-error (L1 / T1 must end in an error; T feeds a limited distance,
# L pushes until FINDA or a button by design)
mode = sys.argv[2] if len(sys.argv) > 2 else ""
if mode in ("cut-rejected", "cut-accepted"):  # Box Turtle has no cutter -> K must be rejected; MMU3 accepts it
    r, _ = ask("K", 1)
    want = "K1 R" if mode == "cut-rejected" else "K1 A"
    print(f"K1 -> {r!r}")
    fails += not r.startswith(want)
if mode in ("load", "load-error", "tc-error"):
    cmd = "T" if mode == "tc-error" else "L"
    r, _ = ask(cmd, 1)
    print(f"{cmd}1 ->", r)
    t0, last = time.monotonic(), ""
    while time.monotonic() - t0 < 90:
        r, _ = ask("Q", 0)
        if r != last:
            print(f"{time.monotonic() - t0:6.2f}s Q0 -> {r}")
            last = r
        if r.startswith((f"{cmd}1 F", f"{cmd}1 E")):
            break
        time.sleep(0.1)
    want = f"{cmd}1 E" if mode.endswith("error") else f"{cmd}1 F0"  # injected faults must surface as errors
    if not last.startswith(want):
        fails += 1
        print(f"FAIL: expected {want!r}, last {last!r}")

sys.exit(1 if fails else 0)
