#!/usr/bin/env python3
"""Local-only browser front end for converting extracted TreeFrogUI themes."""

from __future__ import annotations

import hashlib
import json
import mimetypes
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional
from urllib.parse import parse_qs, unquote, urlparse


ROOT = Path(__file__).resolve().parent
STATIC = ROOT / "static"
IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif"}
CONFIG_PATH = Path(os.environ.get("FROGGYTHEMES_CONFIG", ROOT / "config.json")).resolve()


def load_config() -> dict:
    with CONFIG_PATH.open("r", encoding="utf-8") as file:
        config = json.load(file)
    if not isinstance(config.get("devices"), list) or not config["devices"]:
        raise ValueError("FroggyThemes config must define at least one device.")
    for device in config["devices"]:
        if not all(key in device for key in ("id", "label", "width", "height")):
            raise ValueError("Each configured device needs id, label, width, and height.")
    if config.get("default_device") not in {device["id"] for device in config["devices"]}:
        raise ValueError("FroggyThemes config default_device must name a configured device.")
    return config


CONFIG = load_config()
DEVICES = {
    device["id"]: (int(device["width"]), int(device["height"]))
    for device in CONFIG["devices"]
}
MAP_FILE = ROOT / CONFIG["source_maps"]["backgrounds"]
ALIASES_FILE = ROOT / CONFIG["source_maps"]["icons"]
JOBS: dict[str, dict] = {}
JOBS_LOCK = threading.Lock()
SHUTDOWN_REQUESTED = False


def json_response(handler: BaseHTTPRequestHandler, payload: dict, status: int = 200) -> None:
    data = json.dumps(payload).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(data)))
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    handler.wfile.write(data)


def error(message: str, status: int = 400) -> dict:
    return {"error": message, "status": status}


def path_from(value: str, must_exist: bool = True) -> Path:
    if not isinstance(value, str) or not value.strip():
        raise ValueError("A path is required.")
    path = Path(os.path.expanduser(value)).resolve()
    if must_exist and not path.exists():
        raise ValueError(f"Path does not exist: {path}")
    return path


def image_files(root: Path) -> list[Path]:
    found: list[Path] = []
    for directory, dirnames, filenames in os.walk(root, followlinks=False):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        for name in sorted(filenames):
            path = Path(directory) / name
            if path.suffix.lower() in IMAGE_SUFFIXES:
                found.append(path)
    return found


def read_map() -> list[tuple[str, str]]:
    entries = []
    for line in MAP_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "|" not in line:
            continue
        alias, source_name = line.split("|", 1)
        entries.append((alias.strip(), source_name.strip()))
    return entries


def read_aliases() -> list[str]:
    return [
        line.strip()
        for line in ALIASES_FILE.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]


def magick_command() -> Optional[str]:
    configured = os.environ.get("FROGGYTHEMES_MAGICK")
    if configured and shutil.which(configured):
        return configured
    for candidate in ("magick", "convert"):
        executable = shutil.which(candidate)
        if not executable:
            continue
        try:
            result = subprocess.run([executable, "-version"], capture_output=True, text=True, timeout=5)
            if "ImageMagick" in (result.stdout + result.stderr):
                return executable
        except (OSError, subprocess.SubprocessError):
            continue
    return None


def identify_command() -> Optional[list[str]]:
    tool = magick_command()
    if not tool:
        return None
    if Path(tool).stem.lower() == "magick":
        return [tool, "identify"]
    identify = shutil.which("identify")
    if not identify:
        return None
    try:
        result = subprocess.run([identify, "-version"], capture_output=True, text=True, timeout=5)
        if "ImageMagick" in (result.stdout + result.stderr):
            return [identify]
    except (OSError, subprocess.SubprocessError):
        pass
    return None


def dependencies() -> dict:
    command = magick_command()
    version = ""
    if command:
        try:
            version = subprocess.run(
                [command, "-version"], capture_output=True, text=True, timeout=5
            ).stdout.splitlines()[0]
        except (OSError, subprocess.SubprocessError, IndexError):
            version = "detected"
    return {
        "python": f"{os.sys.version_info.major}.{os.sys.version_info.minor}.{os.sys.version_info.micro}",
        "imagemagick": {"command": command, "version": version},
        "ready": bool(command and identify_command()),
    }


def public_config() -> dict:
    return {
        "devices": CONFIG["devices"],
        "default_device": CONFIG["default_device"],
        "treefrogui": CONFIG["treefrogui"],
        "staging": CONFIG["staging"],
        "output": CONFIG["output"],
    }


def relative_to(path: Path, root: Path) -> str:
    try:
        return str(path.relative_to(root))
    except ValueError:
        return str(path)


def safe_pack_name(value: str) -> str:
    cleaned = "_".join(value.strip().replace("/", "_").replace("\\", "_").split())
    cleaned = "".join(char for char in cleaned if char.isalnum() or char in "._-")
    if not cleaned:
        raise ValueError("Pack name must contain letters or numbers.")
    return cleaned[:80]


def candidate_for_name(files: list[Path], names: list[str]) -> Optional[Path]:
    wanted = {name.casefold() for name in names if name}
    for path in files:
        if path.stem.casefold() in wanted:
            return path
    return None


def icon_candidate(files: list[Path], alias: str, source_name: str) -> Optional[Path]:
    exact = candidate_for_name(files, [alias, source_name])
    if exact:
        return exact
    wanted_dirs = {alias.casefold(), source_name.casefold()}
    for path in files:
        if path.parent.name.casefold() not in wanted_dirs:
            continue
        if path.stem.casefold() in {"logo", "system", "console", "icon"}:
            return path
    return None


def update_job(job_id: str, **values) -> None:
    with JOBS_LOCK:
        JOBS[job_id].update(values)


def log_job(job_id: str, message: str) -> None:
    with JOBS_LOCK:
        JOBS[job_id]["log"].append(message)


def cancel_active_jobs() -> int:
    with JOBS_LOCK:
        active = [job for job in JOBS.values() if job.get("state") in {"queued", "running"}]
        for job in active:
            job["cancel"].set()
        return len(active)


def wait_for_jobs(timeout: float = 10.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with JOBS_LOCK:
            active = any(job.get("state") in {"queued", "running"} for job in JOBS.values())
        if not active:
            return
        time.sleep(0.05)


def public_job(job: dict) -> dict:
    return {key: value for key, value in job.items() if key not in {"cancel", "process"}}


def current_job_locked() -> Optional[dict]:
    jobs = list(JOBS.values())
    for job in reversed(jobs):
        if job.get("state") in {"queued", "running"}:
            return public_job(job)
    for job in reversed(jobs):
        if job.get("state") in {"complete", "error", "cancelled"}:
            return public_job(job)
    return None


def active_job_error() -> Optional[str]:
    for job in JOBS.values():
        if job.get("state") in {"queued", "running"}:
            label = "copy" if job.get("kind") == "deploy" else "conversion"
            return f"A {label} job is already in progress. Wait for it to finish before starting another."
    return None


def run_image(job_id: str, command: list[str], label: str) -> None:
    with JOBS_LOCK:
        JOBS[job_id]["process"] = None
        cancelled = JOBS[job_id]["cancel"]
    if cancelled.is_set():
        raise RuntimeError("Conversion cancelled.")
    log_job(job_id, f"Converting {label}")
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    with JOBS_LOCK:
        JOBS[job_id]["process"] = process
    while process.poll() is None:
        if JOBS[job_id]["cancel"].is_set():
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
            raise RuntimeError("Conversion cancelled.")
        time.sleep(0.05)
    stdout, stderr = process.communicate()
    with JOBS_LOCK:
        JOBS[job_id]["process"] = None
    if process.returncode:
        detail = (stderr or stdout).strip().splitlines()
        raise RuntimeError(detail[-1] if detail else f"ImageMagick failed for {label}.")


def background_command(
    tool: str, source: Path, destination: Path, dim: int, quality: int, width: int, height: int
) -> list[str]:
    return [
        tool, str(source), "-background", "black", "-alpha", "remove", "-alpha", "off",
        "-colorspace", "sRGB", "-resize", f"{width}x{height}!", "-strip", "-fill", "black",
        "-colorize", f"{dim}%", "-type", "TrueColor", "-interlace", "none",
        "-sampling-factor", "4:2:0", "-quality", str(quality), str(destination),
    ]


def icon_command(tool: str, source: Path, destination: Path) -> list[str]:
    output = CONFIG["output"]
    return [
        tool, str(source), "-background", "none", "-alpha", "set", "-trim", "+repage",
        "-resize", f"{output['icon_max_width']}x{output['icon_max_height']}>", "-strip",
        "-define", f"png:compression-level={output['png_compression_level']}",
        str(destination),
    ]


def make_notes(destination: Path, payload: dict, counts: dict, width: int, height: int) -> None:
    notes = [
        "Generated by FroggyThemes Converter",
        "",
        f"Pack: {payload['pack_name']}",
        f"Target device: {payload.get('device', 'SF3500')}",
        f"Native panel: {width}x{height}",
        f"Source folder: {payload['source_root']}",
        f"Source URL: {payload.get('source_url') or 'Not supplied'}",
        f"License note: {payload.get('license_note') or 'Review the source license before redistribution.'}",
        f"Background files: {counts.get('backgrounds', 0)}",
        f"Icon files: {counts.get('icons', 0)}",
        "",
        "The source folder was expected to be already extracted before conversion.",
        "This file records provenance; it does not grant a license to third-party artwork.",
    ]
    (destination / "SOURCE-NOTES.txt").write_text("\n".join(notes) + "\n", encoding="utf-8")


def image_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def append_ledger(ledger_path: Path, job_id: str, event: str, **values) -> None:
    fields = [time.strftime("%Y-%m-%dT%H:%M:%S%z"), f"job={job_id}", f"event={event}"]
    for key, value in values.items():
        safe_value = str(value).replace("\n", " ").replace("\r", " ")
        fields.append(f"{key}={safe_value}")
    with ledger_path.open("a", encoding="utf-8") as file:
        file.write(" ".join(fields) + "\n")


def write_manifest(destination: Path, category: str) -> None:
    images = sorted(
        path for path in destination.iterdir()
        if path.is_file() and path.suffix.lower() in {".jpg", ".jpeg", ".png"}
    )
    lines = ["FroggyThemes manifest v1", f"category={category}", ""]
    for path in images:
        lines.append(f"{image_hash(path)}  {path.relative_to(destination).as_posix()}")
    (destination / "MANIFEST.sha256").write_text("\n".join(lines) + "\n", encoding="utf-8")


def ledger_pack_files(ledger_path: Path, job_id: str, category: str,
                      pack: Path, relative_root: Optional[Path] = None) -> None:
    root = relative_root or pack
    for file in sorted(path for path in pack.rglob("*") if path.is_file()):
        append_ledger(
            ledger_path, job_id, "file",
            category=category,
            path=file.relative_to(root).as_posix(),
            bytes=file.stat().st_size,
            sha256=image_hash(file),
        )


def panel_from_notes(pack: Path) -> tuple[int, int]:
    notes = pack / "SOURCE-NOTES.txt"
    if not notes.is_file():
        raise ValueError(f"Missing SOURCE-NOTES.txt: {pack}")
    for line in notes.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("Native panel:"):
            value = line.split(":", 1)[1].strip().lower()
            width, height = value.split("x", 1)
            return int(width), int(height)
    raise ValueError(f"Missing native panel metadata: {pack}")


def verify_manifest(pack: Path, expected_category: Optional[str] = None) -> None:
    manifest = pack / "MANIFEST.sha256"
    if not manifest.is_file():
        raise ValueError(f"Missing MANIFEST.sha256: {pack}")
    listed = set()
    category = None
    for line in manifest.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith("FroggyThemes manifest") or line.startswith("category="):
            if line.startswith("category="):
                category = line.split("=", 1)[1].strip()
            continue
        try:
            expected, relative = line.split(None, 1)
        except ValueError as exc:
            raise ValueError(f"Malformed manifest entry in {manifest}") from exc
        relative = relative.strip()
        path = (pack / relative).resolve()
        if pack.resolve() not in path.parents or not path.is_file():
            raise ValueError(f"Manifest references an invalid file: {relative}")
        if image_hash(path) != expected:
            raise ValueError(f"{path.name} does not match its SHA-256 manifest")
        listed.add(relative)
    if expected_category and category != expected_category:
        raise ValueError(f"Manifest category mismatch in {manifest}")
    actual = {
        path.relative_to(pack).as_posix()
        for path in pack.iterdir()
        if path.is_file() and path.suffix.lower() in {".jpg", ".jpeg", ".png"}
    }
    if actual != listed:
        raise ValueError(f"Manifest/file mismatch in {pack}")


def identify_image(path: Path) -> tuple[str, int, int, str]:
    command = identify_command()
    if not command:
        raise RuntimeError("ImageMagick identify was not found; output integrity cannot be checked.")
    result = subprocess.run(
        command + ["-format", "%m|%w|%h|%[channels]", str(path)],
        capture_output=True, text=True, timeout=15,
    )
    if result.returncode:
        detail = (result.stderr or result.stdout).strip()
        raise ValueError(f"Could not inspect {path.name}: {detail}")
    try:
        image_format, width, height, channels = result.stdout.strip().split("|", 3)
        return image_format.upper(), int(width), int(height), channels.lower()
    except (ValueError, TypeError) as exc:
        raise ValueError(f"Unexpected identify output for {path.name}") from exc


def validate_pack(pack: Path, category: str, expected_width: Optional[int] = None,
                  expected_height: Optional[int] = None) -> None:
    verify_manifest(pack, category)
    images = sorted(path for path in pack.iterdir() if path.is_file() and path.suffix.lower() in {".jpg", ".jpeg", ".png"})
    if not images:
        raise ValueError(f"Pack contains no images: {pack}")
    if category == "theme":
        main = pack / "main.jpg"
        if not main.is_file():
            raise ValueError(f"Theme pack is missing main.jpg: {pack}")
        width, height = panel_from_notes(pack)
        if expected_width is not None and (width, height) != (expected_width, expected_height):
            raise ValueError(f"Pack metadata panel mismatch: {pack}")
        for image in images:
            image_format, image_width, image_height, channels = identify_image(image)
            if image_format != "JPEG" or (image_width, image_height) != (width, height) or "a" in channels:
                raise ValueError(f"Invalid background output: {image}")
    elif category == "icon":
        max_width = int(CONFIG["output"]["icon_max_width"])
        max_height = int(CONFIG["output"]["icon_max_height"])
        for image in images:
            image_format, image_width, image_height, channels = identify_image(image)
            if image_format != "PNG" or image_width < 1 or image_height < 1 or image_width > max_width or image_height > max_height or "gray" in channels:
                raise ValueError(f"Invalid icon output: {image}")
    else:
        raise ValueError(f"Unknown pack category: {category}")


def commit_directories(staging_root: Path, staged_targets: list[tuple[str, Path, Path]]) -> None:
    backups = []
    moved = []
    try:
        for category, staged_target, final_target in staged_targets:
            final_target.parent.mkdir(parents=True, exist_ok=True)
            if final_target.exists():
                if any(final_target.iterdir()):
                    backup = staging_root / ".backups" / category / final_target.name
                    backup.parent.mkdir(parents=True, exist_ok=True)
                    os.replace(final_target, backup)
                    backups.append((backup, final_target))
                else:
                    final_target.rmdir()
            os.replace(staged_target, final_target)
            moved.append(final_target)
    except Exception:
        for final_target in reversed(moved):
            shutil.rmtree(final_target, ignore_errors=True)
        for backup, final_target in reversed(backups):
            if final_target.exists():
                shutil.rmtree(final_target, ignore_errors=True)
            os.replace(backup, final_target)
        raise


def convert_job(job_id: str, payload: dict) -> None:
    output_root = None
    staging_root = None
    ledger_path = None
    try:
        tool = magick_command()
        if not tool:
            raise RuntimeError("ImageMagick was not found. Install it and restart the converter.")
        source_root = path_from(payload.get("source_root"))
        if not source_root.is_dir():
            raise ValueError("Source folder must be a directory.")
        mode = payload.get("mode", "both")
        if mode not in {"backgrounds", "icons", "both"}:
            raise ValueError("Unknown conversion mode.")
        pack_name = safe_pack_name(payload.get("pack_name", ""))
        output_root = path_from(payload.get("output_root"), must_exist=False)
        output_root.mkdir(parents=True, exist_ok=True)
        ledger_path = output_root / "log.txt"
        append_ledger(ledger_path, job_id, "conversion-start",
                      source=payload.get("source_root", ""), pack=pack_name)
        dim = max(0, min(90, int(payload.get("dim", 35))))
        quality_limits = CONFIG["output"]
        quality = max(
            int(quality_limits["background_min_quality"]),
            min(int(quality_limits["background_max_quality"]), int(payload.get("quality", quality_limits["background_quality"]))),
        )
        device = payload.get("device", CONFIG["default_device"])
        if device not in DEVICES:
            raise ValueError("Unknown target device.")
        width, height = DEVICES[device]

        background_source = path_from(payload.get("background_source")) if mode in {"backgrounds", "both"} else None
        if background_source and not background_source.is_file():
            raise ValueError("Background source must be an image file.")
        system_root = path_from(payload.get("system_root"), must_exist=True) if payload.get("system_root") else None
        icon_root = path_from(payload.get("icon_root"), must_exist=True) if mode in {"icons", "both"} else None
        if icon_root and not icon_root.is_dir():
            raise ValueError("Icon source must be a directory.")

        background_files = image_files(system_root) if system_root else []
        icon_files = image_files(icon_root) if icon_root else []
        map_entries = read_map()
        bg_plan = [(alias, candidate_for_name(background_files, [source_name, alias])) for alias, source_name in map_entries]
        icon_plan = [(alias, icon_candidate(icon_files, alias, source_name)) for alias, source_name in map_entries]
        icon_aliases = read_aliases()
        for alias in icon_aliases:
            if not any(existing == alias for existing, _ in icon_plan):
                icon_plan.append((alias, icon_candidate(icon_files, alias, alias)))

        total = (1 + sum(bool(path) for _, path in bg_plan) if background_source else 0)
        total += sum(bool(path) for _, path in icon_plan) if icon_root else 0
        if not total:
            raise ValueError("No convertible images were found in the selected source folders.")

        targets = []
        if mode in {"backgrounds", "both"}:
            targets.append(("theme", output_root / CONFIG["staging"]["theme_packs"] / pack_name))
        if mode in {"icons", "both"}:
            targets.append(("icon", output_root / CONFIG["staging"]["icon_packs"] / pack_name))
        for _, target in targets:
            target.parent.mkdir(parents=True, exist_ok=True)
            marker = target / "SOURCE-NOTES.txt"
            if target.exists() and (not target.is_dir() or any(target.iterdir())) and not payload.get("overwrite"):
                raise ValueError(f"Output already contains files: {target}. Enable replacement to update it.")
            if target.exists() and (not target.is_dir() or any(target.iterdir())) and payload.get("overwrite"):
                if not marker.is_file() or not marker.read_text(encoding="utf-8", errors="replace").startswith(
                    "Generated by FroggyThemes Converter"
                ):
                    raise ValueError(f"Refusing to replace an unrecognized output folder: {target}")

        staging_root = Path(tempfile.mkdtemp(prefix=f".froggythemes-{job_id}-", dir=output_root))
        staged_targets = [(category, staging_root / category / target.name, target) for category, target in targets]
        for _, staged_target, _ in staged_targets:
            staged_target.mkdir(parents=True, exist_ok=True)

        update_job(job_id, state="running", total=total, completed=0, pack_name=pack_name,
                   device=device, width=width, height=height)
        completed = 0
        counts = {"backgrounds": 0, "icons": 0}
        if background_source:
            destination = staged_targets[0][1] / "main.jpg"
            run_image(job_id, background_command(tool, background_source, destination, dim, quality, width, height), "main background")
            completed += 1
            counts["backgrounds"] += 1
            update_job(job_id, completed=completed)
            for alias, source in bg_plan:
                if not source:
                    continue
                destination = staged_targets[0][1] / f"{alias}.jpg"
                run_image(job_id, background_command(tool, source, destination, dim, quality, width, height), f"background {alias}")
                completed += 1
                counts["backgrounds"] += 1
                update_job(job_id, completed=completed)
        if icon_root:
            icon_target = staged_targets[-1][1]
            for alias, source in icon_plan:
                if not source:
                    continue
                destination = icon_target / f"{alias}.png"
                run_image(job_id, icon_command(tool, source, destination), f"icon {alias}")
                completed += 1
                counts["icons"] += 1
                update_job(job_id, completed=completed)
        for _, staged_target, _ in staged_targets:
            make_notes(staged_target, payload, counts, width, height)
        for category, staged_target, _ in staged_targets:
            write_manifest(staged_target, category)
            validate_pack(staged_target, category, width, height)
            append_ledger(ledger_path, job_id, "integrity-pass", category=category, pack=staged_target.name)
            ledger_pack_files(ledger_path, job_id, category, staged_target, staging_root)
        commit_directories(staging_root, staged_targets)
        shutil.rmtree(staging_root, ignore_errors=True)
        log_job(job_id, f"Finished: {counts['backgrounds']} backgrounds, {counts['icons']} icons")
        update_job(job_id, state="complete", completed=total,
                   outputs=[{"category": category, "path": str(target)} for category, target in targets],
                   output_root=str(output_root), mode=mode,
                   integrity={"state": "passed", "backgrounds": counts["backgrounds"], "icons": counts["icons"]})
    except Exception as exc:  # keep failures visible in the browser
        if ledger_path:
            try:
                append_ledger(ledger_path, job_id, "integrity-fail", error=str(exc))
            except OSError:
                pass
        if staging_root:
            shutil.rmtree(staging_root, ignore_errors=True)
        log_job(job_id, str(exc))
        update_job(job_id, state="cancelled" if "cancelled" in str(exc).lower() else "error", error=str(exc),
                   integrity={"state": "failed", "message": str(exc)})


def card_info(value: str) -> dict:
    card_root = path_from(value)
    if not card_root.is_dir():
        raise ValueError("Mounted SD card path must be a directory.")
    treefrogui = CONFIG["treefrogui"]
    frogui = card_root / treefrogui["frogui_directory"]
    return {
        "path": str(card_root),
        "frogui": str(frogui),
        "valid": frogui.is_dir(),
        "settings": (card_root / treefrogui["settings_file"]).is_file(),
        "theme_target": str(card_root / treefrogui["theme_packs_directory"]),
        "icon_target": str(card_root / treefrogui["icon_packs_directory"]),
    }


def deploy_job(job_id: str, payload: dict) -> None:
    staging_root = None
    ledger_path = Path(payload["ledger_path"]) if payload.get("ledger_path") else None
    try:
        card = card_info(payload.get("card_root", ""))
        if not card["valid"]:
            raise ValueError("Select the mounted SD card root containing a frogui directory.")
        source_entries = payload.get("sources", [])
        sources = [(entry["category"], path_from(entry["path"])) for entry in source_entries]
        if not sources:
            raise ValueError("There are no converted packs to copy.")
        overwrite = bool(payload.get("overwrite"))
        copy_plan = []
        staging_root = Path(tempfile.mkdtemp(prefix=f".froggythemes-install-{job_id}-", dir=card["frogui"]))
        for category, source in sources:
            if not source.is_dir() or category not in {"theme", "icon"}:
                raise ValueError(f"Invalid converted pack path: {source}")
            validate_pack(source, category)
            target_root = Path(card["theme_target"] if category == "theme" else card["icon_target"])
            target = target_root / source.name
            if target.exists() and (not target.is_dir() or any(target.iterdir())):
                marker = target / "SOURCE-NOTES.txt"
                if not overwrite:
                    raise ValueError(f"Pack already exists on the SD card: {target}. Enable replacement to update it.")
                if not marker.is_file() or not marker.read_text(encoding="utf-8", errors="replace").startswith(
                    "Generated by FroggyThemes Converter"
                ):
                    raise ValueError(f"Refusing to replace an unrecognized SD-card pack: {target}")
            files = [item for item in source.rglob("*") if item.is_file()]
            staged_target = staging_root / category / source.name
            copy_plan.append((category, source, target, staged_target, files))

        total = sum(len(files) for _, _, _, _, files in copy_plan)
        update_job(job_id, state="running", total=total, completed=0,
                   card_root=card["path"], sources=[str(source) for _, source, _, _, _ in copy_plan])
        if ledger_path:
            append_ledger(ledger_path, job_id, "deployment-start", card=card["path"])
        completed = 0
        for category, source, target, staged_target, files in copy_plan:
            staged_target.mkdir(parents=True, exist_ok=True)
            log_job(job_id, f"Copying {category}/{source.name} to mounted SD card")
            for source_file in files:
                if JOBS[job_id]["cancel"].is_set():
                    raise RuntimeError("Copy cancelled.")
                destination = staged_target / source_file.relative_to(source)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_file, destination)
                if ledger_path:
                    append_ledger(ledger_path, job_id, "copy-file", category=category,
                                  source=source_file, destination=destination, bytes=destination.stat().st_size,
                                  sha256=image_hash(destination))
                completed += 1
                update_job(job_id, completed=completed)
        for category, _, _, staged_target, _ in copy_plan:
            validate_pack(staged_target, category)
        commit_directories(
            staging_root,
            [(category, staged_target, target) for category, _, target, staged_target, _ in copy_plan],
        )
        shutil.rmtree(staging_root, ignore_errors=True)
        if ledger_path:
            append_ledger(ledger_path, job_id, "deployment-complete", card=card["path"], files=completed)
        log_job(job_id, f"Finished: copied {completed} files to {card['path']}")
        update_job(job_id, state="complete", completed=total,
                   integrity={"state": "passed", "files": completed})
    except Exception as exc:
        if ledger_path:
            try:
                append_ledger(ledger_path, job_id, "deployment-fail", error=str(exc), card=payload.get("card_root", ""))
            except OSError:
                pass
        if staging_root:
            shutil.rmtree(staging_root, ignore_errors=True)
        log_job(job_id, str(exc))
        update_job(job_id, state="cancelled" if "cancelled" in str(exc).lower() else "error", error=str(exc),
                   integrity={"state": "failed", "message": str(exc)})


def open_output_folder(job_id: str) -> str:
    with JOBS_LOCK:
        job = JOBS.get(job_id)
        if not job or job.get("state") != "complete" or job.get("kind", "convert") == "deploy":
            raise ValueError("Complete a conversion before opening its output folder.")
        output_root = path_from(job.get("output_root", ""))
    if not output_root.is_dir():
        raise ValueError(f"Output folder no longer exists: {output_root}")
    if os.name == "nt":
        os.startfile(str(output_root))  # type: ignore[attr-defined]
    elif sys.platform == "darwin":
        subprocess.Popen(["open", str(output_root)], start_new_session=True)
    else:
        opener = shutil.which("xdg-open")
        if not opener:
            raise ValueError("The OS file explorer command xdg-open was not found.")
        subprocess.Popen([opener, str(output_root)], start_new_session=True,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return str(output_root)


def browse(path: Path) -> dict:
    entries = []
    for item in sorted(path.iterdir(), key=lambda value: (not value.is_dir(), value.name.casefold())):
        if item.name.startswith("."):
            continue
        entry = {"name": item.name, "path": str(item), "directory": item.is_dir()}
        if item.is_file():
            entry["size"] = item.stat().st_size
            entry["image"] = item.suffix.lower() in IMAGE_SUFFIXES
        entries.append(entry)
    return {"path": str(path), "parent": str(path.parent), "entries": entries[:500]}


class Handler(BaseHTTPRequestHandler):
    server_version = "FroggyThemesConverter/1.0"

    def log_message(self, format: str, *args) -> None:
        return

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        try:
            if parsed.path == "/api/config":
                json_response(self, public_config())
                return
            if parsed.path == "/api/current-job":
                with JOBS_LOCK:
                    current = current_job_locked()
                json_response(self, {"job": current})
                return
            if parsed.path == "/api/dependencies":
                json_response(self, dependencies())
                return
            if parsed.path == "/api/browse":
                path = path_from(parse_qs(parsed.query).get("path", [str(Path.home())])[0])
                if not path.is_dir():
                    raise ValueError("Browse path must be a directory.")
                json_response(self, browse(path))
                return
            if parsed.path == "/api/inspect":
                path = path_from(parse_qs(parsed.query).get("path", [""])[0])
                if not path.is_dir():
                    raise ValueError("Inspect path must be a directory.")
                files = image_files(path)
                folders = {}
                for image in files:
                    folders.setdefault(str(image.parent), 0)
                    folders[str(image.parent)] += 1
                files = files[:300]
                json_response(self, {
                    "path": str(path),
                    "image_count": len(image_files(path)),
                    "images": [{"path": str(item), "name": item.name, "relative": relative_to(item, path)} for item in files],
                    "folders": [{"path": folder, "count": count} for folder, count in sorted(folders.items())[:200]],
                })
                return
            if parsed.path == "/api/preview":
                path = path_from(parse_qs(parsed.query).get("path", [""])[0])
                if not path.is_file() or path.suffix.lower() not in IMAGE_SUFFIXES:
                    raise ValueError("Preview is only available for image files.")
                data = path.read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", mimetypes.guess_type(path.name)[0] or "application/octet-stream")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            if parsed.path == "/api/card-check":
                path = parse_qs(parsed.query).get("path", [""])[0]
                json_response(self, card_info(path))
                return
            if parsed.path == "/docs":
                data = (ROOT / "README.md").read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            if parsed.path.startswith("/api/jobs/"):
                job_id = parsed.path.rsplit("/", 1)[-1]
                with JOBS_LOCK:
                    job = JOBS.get(job_id)
                    if not job:
                        raise ValueError("Unknown job.")
                    response = public_job(job)
                json_response(self, response)
                return
            self.serve_static(parsed.path)
        except Exception as exc:
            json_response(self, error(str(exc)), 400)

    def do_POST(self) -> None:
        global SHUTDOWN_REQUESTED
        parsed = urlparse(self.path)
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length) or b"{}")
            if parsed.path == "/api/convert":
                if SHUTDOWN_REQUESTED:
                    raise ValueError("The converter server is shutting down.")
                job_id = uuid.uuid4().hex[:12]
                with JOBS_LOCK:
                    conflict = active_job_error()
                    if conflict:
                        raise ValueError(conflict)
                    JOBS[job_id] = {
                        "id": job_id, "state": "queued", "completed": 0, "total": 0,
                        "log": [], "cancel": threading.Event(), "process": None,
                        "kind": "convert",
                    }
                threading.Thread(target=convert_job, args=(job_id, payload), daemon=True).start()
                json_response(self, {"job_id": job_id})
                return
            if parsed.path == "/api/deploy":
                if SHUTDOWN_REQUESTED:
                    raise ValueError("The converter server is shutting down.")
                source_job_id = payload.get("job_id")
                with JOBS_LOCK:
                    conflict = active_job_error()
                    if conflict:
                        raise ValueError(conflict)
                    source_job = JOBS.get(source_job_id)
                    if not source_job or source_job.get("state") != "complete":
                        raise ValueError("Complete a conversion before copying packs to the SD card.")
                    sources = list(source_job.get("outputs", []))
                job_id = uuid.uuid4().hex[:12]
                deploy_payload = {
                    "card_root": payload.get("card_root"),
                    "sources": sources,
                    "overwrite": bool(payload.get("overwrite")),
                    "ledger_path": str(Path(source_job["output_root"]) / "log.txt"),
                }
                with JOBS_LOCK:
                    JOBS[job_id] = {
                        "id": job_id, "state": "queued", "completed": 0, "total": 0,
                        "log": [], "cancel": threading.Event(), "process": None,
                        "kind": "deploy", "source_job_id": source_job_id,
                    }
                threading.Thread(target=deploy_job, args=(job_id, deploy_payload), daemon=True).start()
                json_response(self, {"job_id": job_id})
                return
            if parsed.path == "/api/shutdown":
                if payload.get("confirmation") != "Yes":
                    raise ValueError("Shutdown requires the exact confirmation: Yes")
                SHUTDOWN_REQUESTED = True
                active_jobs = cancel_active_jobs()
                threading.Thread(
                    target=self.server_shutdown, args=(self.server,), daemon=True
                ).start()
                json_response(self, {"ok": True, "active_jobs": active_jobs})
                return
            if parsed.path == "/api/open-output":
                path = open_output_folder(payload.get("job_id", ""))
                json_response(self, {"ok": True, "path": path})
                return
            if parsed.path.startswith("/api/jobs/") and parsed.path.endswith("/cancel"):
                job_id = parsed.path.split("/")[-2]
                with JOBS_LOCK:
                    job = JOBS.get(job_id)
                    if not job:
                        raise ValueError("Unknown job.")
                    job["cancel"].set()
                json_response(self, {"ok": True})
                return
            raise ValueError("Unknown API endpoint.")
        except Exception as exc:
            json_response(self, error(str(exc)), 400)

    @staticmethod
    def server_shutdown(server: ThreadingHTTPServer) -> None:
        wait_for_jobs()
        server.shutdown()

    def serve_static(self, requested: str) -> None:
        relative = unquote(requested.lstrip("/")) or "index.html"
        candidate = (STATIC / relative).resolve()
        if STATIC not in candidate.parents and candidate != STATIC:
            self.send_error(404)
            return
        if not candidate.is_file():
            self.send_error(404)
            return
        data = candidate.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", mimetypes.guess_type(candidate.name)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description="Run the local FroggyThemes converter")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"FroggyThemes Converter: http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        cancel_active_jobs()
        wait_for_jobs()
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
