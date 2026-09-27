"""Small, shared file/process utilities. No experiment runs at import time."""
from __future__ import annotations
import contextlib
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import tempfile
import time


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()).hexdigest()


def _pairs(items):
    d = {}
    for k, v in items:
        if k in d:
            raise ValueError('duplicate JSON key: ' + k)
        d[k] = v
    return d


def loads(s):
    return json.loads(s, object_pairs_hook=_pairs,
                      parse_constant=lambda x: (_ for _ in ()).throw(ValueError('nonfinite JSON: ' + x)))


def read(path):
    return loads(Path(path).read_text())


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.tmp')
    with temporary.open('x') as f:
        json.dump(value, f, indent=2, sort_keys=True, allow_nan=False)
        f.write('\n')
        f.flush()
        os.fsync(f.fileno())
    temporary.replace(path)


def append(path, value):
    with Path(path).open('a') as f:
        f.write(json.dumps(value, sort_keys=True, allow_nan=False) + '\n')
        f.flush()


def environment():
    names = ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'VECLIB_MAXIMUM_THREADS',
             'GOTO_NUM_THREADS', 'DYLD_LIBRARY_PATH', 'DYLD_INSERT_LIBRARIES',
             'LIBXSMM_TARGET', 'LIBXSMM_VERBOSE', 'SDKROOT', 'DEVELOPER_DIR')
    hardware = {}
    if platform.system() == 'Darwin':
        keys = ('hw.model', 'hw.ncpu', 'hw.memsize', 'machdep.cpu.brand_string',
                'hw.optional.arm.FEAT_SME', 'hw.optional.arm.FEAT_SME2')
        for key in keys:
            result = subprocess.run(['/usr/sbin/sysctl', '-n', key], text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=5)
            hardware[key] = result.stdout.strip() if result.returncode == 0 else None
    return {'platform': platform.platform(), 'machine': platform.machine(), 'hardware': hardware,
            'node': platform.node(), 'python': platform.python_version(),
            'variables': {n: os.environ.get(n) for n in names}}


@contextlib.contextmanager
def machine_lock(path=None):
    # One host/user lock across relocated copies and different output roots.
    if path is None:
        directory = Path('/tmp') / ('mp5-experiments-' + str(os.getuid()))
        directory.mkdir(mode=0o700, exist_ok=True)
        if directory.is_symlink() or directory.stat().st_uid != os.getuid():
            raise RuntimeError('unsafe host-lock directory: ' + str(directory))
        path = directory / 'host.lock'
    path = Path(path)
    fd = os.open(path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError('another experiment/build holds the host lock: ' + str(path))
        os.ftruncate(fd, 0)
        os.write(fd, (str(os.getpid()) + '\n').encode())
        yield
    finally:
        os.close(fd)


def command(argv, directory, timeout=3600, env=None):
    """Run once, preserve stdout/stderr and status, reap the owned process group."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    record = {'command': list(map(str, argv)), 'started': utc(), 'status': 'RUNNING'}
    write(directory / 'command.json', record)
    child = None
    try:
        with (directory / 'stdout.txt').open('xb') as out, (directory / 'stderr.txt').open('xb') as err:
            child = subprocess.Popen(record['command'], stdout=out, stderr=err, env=env,
                                     start_new_session=True)
            rc = child.wait(timeout=timeout)
        record.update(returncode=rc, status='PASS' if rc == 0 else 'FAILED')
        if rc:
            raise RuntimeError('command failed (%d); see %s' % (rc, directory / 'stderr.txt'))
        return (directory / 'stdout.txt').read_text()
    except BaseException as e:
        if child is not None and child.poll() is None:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
        record.update(status='FAILED', error=str(e), returncode=child.returncode if child else None)
        raise
    finally:
        record['ended'] = utc()
        write(directory / 'command.json', record)


def file_manifest(directory, exclude=()):
    return {str(p.relative_to(directory)): sha(p) for p in sorted(Path(directory).rglob('*'))
            if p.is_file() and not p.is_symlink() and str(p.relative_to(directory)) not in exclude}


def check_manifest(directory, files):
    base = Path(directory).resolve()
    for name, expected in files.items():
        p = base / name
        if not p.resolve().is_relative_to(base) or p.is_symlink() or not p.is_file() or sha(p) != expected:
            raise RuntimeError('missing, stale, or unsafe artifact: ' + str(p))
