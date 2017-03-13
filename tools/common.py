import os
import sys
import thread
import threading

# For OS specific tasks
IS_DARWIN = sys.platform == 'darwin'
IS_LINUX = 'linux' in sys.platform
IS_WINDOWS = sys.platform == 'win32'

if IS_WINDOWS:
    import win32file

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
RUNNER = os.path.join(TOOLS_DIR, 'challenge_runner.py')
AJL = os.path.join(TOOLS_DIR, 'AppJailLauncher', 'Debug', 'AppJailLauncher.exe')


def try_delete(path):
    try:
        os.remove(path)
    except OSError:
        pass


def stdout_flush(s):
    sys.stdout.write(s)
    sys.stdout.flush()


class TimeoutError(Exception):
    pass


class Timeout(object):
    raise_timeout = False

    def __init__(self, seconds):
        self.timer = threading.Timer(seconds, thread.interrupt_main)
        self.timed_out = False

    def __enter__(self):
        self.timer.start()
        return self

    def __exit__(self, extype, ex, trace):
        if extype is None:
            return self.timer.cancel()
        elif extype is KeyboardInterrupt:
            # The timer sent an interrupt after the timeout
            self.timed_out = True

            # Option to silently timeout
            if not self.raise_timeout:
                return True

            # raise a TimeoutError in place of the interrupt
            raise TimeoutError('Timed out after {}s'.format(self.timer.interval))


class TimeoutEx(Timeout):
    raise_timeout = True
