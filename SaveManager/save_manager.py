import argparse
import binascii
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

try:
    import serial  # type: ignore
    from serial.tools import list_ports  # type: ignore
except Exception as e:  # pragma: no cover
    serial = None
    _SERIAL_IMPORT_ERROR = e


DEFAULT_BAUD = 115200
READY_RE = re.compile(r"target_bytes=(\d+)(?:\s+\(legacy=(\d+)\))?")
RECV_RE = re.compile(r"\[SaveImport\]\s+RECV\s+(\d+)/(\d+)")


@dataclass
class SlotStatus:
    slot: int
    raw_line: str


def _require_pyserial() -> None:
    if serial is None:
        raise RuntimeError(
            "缺少依赖 pyserial。请先安装：pip install pyserial\n"
            f"导入错误: {_SERIAL_IMPORT_ERROR}"
        )


def _open_serial(port: str, baud: int, timeout: float):
    _require_pyserial()
    # rts/dtr 默认不动，避免某些板子自动复位
    return serial.Serial(
        port=port,
        baudrate=baud,
        timeout=timeout,
        write_timeout=timeout,
    )


def _write_line(ser, line: str) -> None:
    if not line.endswith("\n"):
        line += "\n"
    ser.write(line.encode("utf-8", errors="replace"))
    ser.flush()


def _read_line(ser) -> str:
    b = ser.readline()
    if not b:
        return ""
    # 设备侧输出基本是 ASCII
    return b.decode("utf-8", errors="replace").strip()


def _wait_for_lines(
    ser,
    predicate,
    wanted_count: int,
    total_timeout_s: float,
):
    start = time.time()
    out = []
    while time.time() - start < total_timeout_s:
        line = _read_line(ser)
        if not line:
            continue
        if predicate(line):
            out.append(line)
            if len(out) >= wanted_count:
                return out
    raise TimeoutError("等待设备输出超时。")


def cmd_status(port: str, baud: int, timeout: float) -> list[SlotStatus]:
    with _open_serial(port, baud, timeout) as ser:
        _write_line(ser, "SAVE_SLOT_STATUS")

        status_map: dict[int, SlotStatus] = {}
        start = time.time()
        while time.time() - start < timeout:
            line = _read_line(ser)
            if not line:
                continue
            if line.startswith("slot"):
                # 形如 slot0:..., slot1:..., slot2:...
                try:
                    prefix, _rest = line.split(":", 1)
                    slot_idx = int(prefix.replace("slot", ""))
                except Exception:
                    continue
                status_map[slot_idx] = SlotStatus(slot=slot_idx, raw_line=line)
        statuses = list(status_map.values())
        statuses.sort(key=lambda s: s.slot)
        return statuses


def _read_block_between_markers(
    ser,
    start_marker: str,
    end_marker: str,
    timeout_s: float,
) -> list[str]:
    """
    读取串口输出中从 start_marker 到 end_marker（包含 marker 行）的完整块。
    若超时则抛出 TimeoutError。
    """
    start = time.time()
    lines: list[str] = []
    in_block = False
    while time.time() - start < timeout_s:
        line = _read_line(ser)
        if not line:
            continue
        if not in_block:
            if line == start_marker:
                in_block = True
                lines.append(line)
            continue
        # in_block
        lines.append(line)
        if line == end_marker and len(lines) > 1:
            return lines
    raise TimeoutError("读取状态块超时（未找到结束标记）。")


def cmd_status_with_game_status(port: str, baud: int, timeout: float):
    """
    返回 (slot_status_lines, per_slot_block_lines)
    """
    with _open_serial(port, baud, timeout) as ser:
        # 1) 槽状态
        _write_line(ser, "SAVE_SLOT_STATUS")
        status_map: dict[int, SlotStatus] = {}
        start = time.time()
        while time.time() - start < timeout:
            line = _read_line(ser)
            if not line:
                continue
            if line.startswith("slot"):
                try:
                    prefix, _rest = line.split(":", 1)
                    slot_idx = int(prefix.replace("slot", ""))
                except Exception:
                    continue
                status_map[slot_idx] = SlotStatus(slot=slot_idx, raw_line=line)
        statuses = list(status_map.values())
        statuses.sort(key=lambda s: s.slot)

        # 2) 每个槽各自的状态（串口命令 s0/s1/s2）
        blocks: dict[int, list[str]] = {}
        for s in statuses:
            _write_line(ser, f"s{s.slot}")
            # 先尝试读取标准状态块
            try:
                block = _read_block_between_markers(
                    ser,
                    start_marker="========================================",
                    end_marker="========================================",
                    timeout_s=timeout,
                )
                blocks[s.slot] = block
                continue
            except TimeoutError:
                pass

            # 若没有块，读取一条失败提示
            start2 = time.time()
            fallback = [f"[slot{s.slot}] EMPTY/UNAVAILABLE"]
            while time.time() - start2 < timeout:
                line = _read_line(ser)
                if not line:
                    continue
                if line.startswith("[SlotStatus]"):
                    fallback = [line]
                    break
            blocks[s.slot] = fallback

        return statuses, blocks


def _print_status_grouped(statuses: list[SlotStatus], blocks: dict[int, list[str]]) -> None:
    for idx, s in enumerate(statuses):
        print(f"slot{s.slot}：{s.raw_line}")
        print("--- 当前游玩状态（s）---")
        lines = blocks.get(s.slot, [f"[slot{s.slot}] EMPTY/UNAVAILABLE"])
        print("\n".join(lines))
        if idx != len(statuses) - 1:
            print()


def cmd_export(port: str, baud: int, timeout: float, slot: int, out_path: Path, pet_only: bool = False) -> None:
    with _open_serial(port, baud, timeout) as ser:
        cmd = f"SAVE_EXPORT_PET {slot}" if pet_only else f"SAVE_EXPORT {slot}"
        _write_line(ser, cmd)

        begin = _wait_for_lines(
            ser,
            lambda l: l.startswith("SAVE_EXPORT_BEGIN"),
            wanted_count=1,
            total_timeout_s=timeout,
        )[0]

        # 收集数据行直到 END
        hex_chunks: list[str] = []
        start = time.time()
        end_line = ""
        while time.time() - start < timeout:
            line = _read_line(ser)
            if not line:
                continue
            if line.startswith("SAVE_EXPORT_DATA "):
                hex_chunks.append(line[len("SAVE_EXPORT_DATA ") :].strip())
                continue
            if line.startswith("SAVE_EXPORT_END"):
                end_line = line
                break
        if not end_line:
            raise TimeoutError("导出过程中等待 SAVE_EXPORT_END 超时。")

        # 拼回原始 bytes
        joined = "".join(hex_chunks)
        try:
            raw = binascii.unhexlify(joined.encode("ascii"))
        except Exception as e:
            raise ValueError(f"解析导出 HEX 失败: {e}\nBEGIN={begin}\nEND={end_line}")

        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_bytes(raw)
        print(f"已导出 slot{slot} -> {out_path} ({len(raw)} bytes)")


def _chunk_hex(data: bytes, chunk_bytes: int) -> list[str]:
    out: list[str] = []
    for i in range(0, len(data), chunk_bytes):
        out.append(binascii.hexlify(data[i : i + chunk_bytes]).decode("ascii").upper())
    return out


def cmd_import(port: str, baud: int, timeout: float, slot: int, in_path: Path) -> None:
    raw = in_path.read_bytes()
    # 设备端 SLOT_RAW_BYTES = sizeof(SaveHeader)+sizeof(PetState)；不同版本可能变化
    # 这里不硬编码大小，只要能完整发完，设备端会做校验并提示 Incomplete/FAILED。

    with _open_serial(port, baud, timeout) as ser:
        _write_line(ser, f"SAVE_IMPORT_BEGIN {slot}")

        # 等 READY
        ready_line = _wait_for_lines(
            ser,
            lambda l: l.startswith("[SaveImport] READY"),
            wanted_count=1,
            total_timeout_s=timeout,
        )[0]
        print(f"[Import] Device READY: {ready_line}")
        print(f"[Import] Local file bytes: {len(raw)}")

        # 若固件给出了期望长度，先做本地文件长度校验，避免无意义超时
        expected_sizes: list[int] = []
        m = READY_RE.search(ready_line)
        if m:
            expected_sizes.append(int(m.group(1)))
            if m.group(2):
                expected_sizes.append(int(m.group(2)))
        if expected_sizes and len(raw) not in expected_sizes:
            raise RuntimeError(
                f"导入文件长度不匹配: {len(raw)} bytes, 设备期望 {expected_sizes} bytes。"
            )
        if not expected_sizes:
            raise RuntimeError(
                "无法从设备 READY 行解析期望长度。请确认固件与脚本版本一致。\n"
                f"READY 行: {ready_line}"
            )

        # 16 bytes/行。每行发送后等待设备 RECV 回执，避免高速连发导致最后一包丢失。
        sent = 0
        chunks = _chunk_hex(raw, 16)
        for idx, hx in enumerate(chunks):
            retries = 0
            chunk_bytes = len(hx) // 2
            expected_after = sent + chunk_bytes
            acked = False
            while retries < 3 and not acked:
                _write_line(ser, f"SAVE_IMPORT_DATA {hx}")
                t0 = time.time()
                while time.time() - t0 < timeout:
                    line = _read_line(ser)
                    if not line:
                        continue
                    m_recv = RECV_RE.search(line)
                    if m_recv:
                        cur = int(m_recv.group(1))
                        # total = int(m_recv.group(2))  # 可用于调试
                        if cur >= expected_after:
                            sent = cur
                            acked = True
                            break
                        # 收到旧回执则继续等
                        continue
                    if ("Invalid hex payload" in line or
                        "No active session" in line):
                        raise RuntimeError(f"设备端导入失败: {line}")
                if not acked:
                    retries += 1
                    if retries < 3:
                        print(f"[Import] 分包 {idx+1}/{len(chunks)} 未确认，重试 {retries}/2...")
            if not acked:
                raise TimeoutError(
                    f"分包 {idx+1}/{len(chunks)} 发送后未收到 RECV 确认。"
                )

        _write_line(ser, "SAVE_IMPORT_COMMIT")

        # 等 OK 或 FAILED
        start = time.time()
        seen_lines: list[str] = []
        while time.time() - start < timeout:
            line = _read_line(ser)
            if not line:
                continue
            if len(seen_lines) < 20:
                seen_lines.append(line)
            if "[SaveImport] Full backup restored" in line:
                print(f"完整备份恢复完成：{in_path} -> slot{slot}")
                return
            if "[SaveImport] Visit started" in line:
                print(f"串门开始：{in_path} -> slot{slot}")
                print("访客宠物已加载。销毁(reset)访客宠物将结束串门并恢复主人宠物。")
                return
            if "[SaveImport] REJECTED" in line:
                raise RuntimeError(f"设备端拒绝导入: {line}")
            if "[SaveImport] FAILED" in line or line.startswith("[SaveImport] FAILED"):
                raise RuntimeError(f"设备端导入失败: {line}")
            if ("Incomplete payload" in line or
                "Invalid hex payload" in line or
                "No active session" in line):
                raise RuntimeError(f"设备端导入失败: {line}")
        tail = "\n".join(seen_lines[-8:])
        raise TimeoutError(f"等待设备导入结果超时。最近设备输出:\n{tail}")


def _print_commands() -> None:
    print("可用命令（输入括号内的简写）：")
    print("  (st) 查询存档状态")
    print("  (ex) 导出完整备份（宠物+图鉴）到本地 .bin")
    print("  (ep) 导出仅宠物（用于串门/交换）到本地 .bin")
    print("  (im) 导入本地 .bin 到指定槽")
    print("  (q)  退出")


def _list_serial_ports() -> list[str]:
    _require_pyserial()
    return [p.device for p in list_ports.comports()]


def _prompt(text: str, default: Optional[str] = None) -> str:
    if default is None:
        return input(f"{text}: ").strip()
    v = input(f"{text}（默认 {default}）: ").strip()
    return v if v else default


def _prompt_int(text: str, allowed: Optional[list[int]] = None, default: Optional[int] = None) -> int:
    while True:
        s = _prompt(text, str(default) if default is not None else None)
        try:
            v = int(s)
        except ValueError:
            print("请输入整数。")
            continue
        if allowed is not None and v not in allowed:
            print(f"可选值：{allowed}")
            continue
        return v


def _interactive_main() -> int:
    _require_pyserial()
    print("ESP32 Fate Tamagotchi 存档管理（串口）")

    ports = _list_serial_ports()
    if ports:
        print("检测到串口：", ", ".join(ports))
        port = _prompt("请输入串口号，例如 COM5", ports[0])
    else:
        port = _prompt("未检测到串口列表，请手动输入串口号，例如 COM5")

    baud = _prompt_int("请输入波特率", default=DEFAULT_BAUD)
    timeout = float(_prompt("请输入超时秒数", "10"))

    while True:
        _print_commands()
        cmd = _prompt("请选择命令", "st").lower()
        if cmd in ("q", "quit", "exit"):
            return 0
        if cmd == "st":
            sts, blocks = cmd_status_with_game_status(port, baud, timeout)
            _print_status_grouped(sts, blocks)
            continue
        if cmd == "ex":
            slot = _prompt_int("请输入槽位（0/1/2）", allowed=[0, 1, 2], default=0)
            out_path = Path(_prompt("请输入输出文件路径", f"slot{slot}_full.bin"))
            cmd_export(port, baud, timeout, slot, out_path, pet_only=False)
            continue
        if cmd == "ep":
            slot = _prompt_int("请输入槽位（0/1/2）", allowed=[0, 1, 2], default=0)
            out_path = Path(_prompt("请输入输出文件路径", f"slot{slot}_pet.bin"))
            cmd_export(port, baud, timeout, slot, out_path, pet_only=True)
            continue
        if cmd == "im":
            print("导入规则：")
            print("  - 带图鉴数据的文件 = 完整备份恢复（覆盖图鉴）")
            print("  - 仅宠物数据的文件 = 串门（冻结主人宠物，加载访客）")
            slot = _prompt_int("请输入槽位（0/1/2）", allowed=[0, 1, 2], default=0)
            in_path = Path(_prompt("请输入要导入的 .bin 文件路径"))
            cmd_import(port, baud, timeout, slot, in_path)
            continue
        print("未知命令，请重新选择。")


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(
        prog="save_manager.py",
        description="ESP32 Fate Tamagotchi 存档管理（串口）：状态/导出/导入（按槽位）",
    )
    p.add_argument("--port", help="串口号，例如 COM6；不填则进入交互模式")
    p.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    p.add_argument("--timeout", type=float, default=10.0, help="每次命令整体超时（秒）")

    sub = p.add_subparsers(dest="cmd")

    sub.add_parser("status", help="查询 slot0/slot1/slot2 状态")

    exp = sub.add_parser("export", help="导出指定槽位到本地 .bin")
    exp.add_argument("--slot", type=int, required=True, choices=[0, 1, 2])
    exp.add_argument("--out", type=Path, required=True)
    exp.add_argument("--pet-only", action="store_true", help="仅导出宠物（不含图鉴，用于串门/交换）")

    imp = sub.add_parser("import", help="导入本地 .bin 覆盖到指定槽位")
    imp.add_argument("--slot", type=int, required=True, choices=[0, 1, 2])
    imp.add_argument("--in", dest="in_path", type=Path, required=True)

    args = p.parse_args(argv)

    if args.cmd is None:
        return _interactive_main()

    if not args.port:
        raise SystemExit("缺少 --port（或直接不带参数运行进入交互模式）")

    if args.cmd == "status":
        sts, blocks = cmd_status_with_game_status(args.port, args.baud, args.timeout)
        _print_status_grouped(sts, blocks)
        return 0

    if args.cmd == "export":
        cmd_export(args.port, args.baud, args.timeout, args.slot, args.out, pet_only=getattr(args, 'pet_only', False))
        return 0

    if args.cmd == "import":
        cmd_import(args.port, args.baud, args.timeout, args.slot, args.in_path)
        return 0

    raise AssertionError("unreachable")


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        print("已取消。", file=sys.stderr)
        raise SystemExit(130)
