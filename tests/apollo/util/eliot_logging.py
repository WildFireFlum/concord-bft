from functools import lru_cache
from eliot import start_action, start_task, to_file, add_destinations, log_call, log_message
from datetime import datetime
import os
import sys
import traceback
from pathlib import Path

from eliot import register_exception_extractor
register_exception_extractor(Exception, lambda e: {"traceback": traceback.format_exc()})

@lru_cache(maxsize=None)
def logdir_timestamp():
    if 'APOLLO_RUN_TEST_DIR' not in os.environ:
        print("APOLLO_RUN_TEST_DIR environment variable not set")
        return datetime.now().strftime("%y-%m-%d_%H-%M-%S")
    return os.environ['APOLLO_RUN_TEST_DIR']


def set_file_destination():
    test_name = os.environ.get('TEST_NAME')

    if not test_name:
        now = logdir_timestamp()
        test_name = f"apollo_run_{now}"

    relative_apollo_logs = 'tests/apollo/logs'
    relative_current_run_logs = f'{relative_apollo_logs}/{logdir_timestamp()}'
    logs_dir = f'../../build/{relative_current_run_logs}'
    test_dir = f'{logs_dir}/{test_name}'
    test_log = f'{test_dir}/{test_name}.log'

    logs_shortcut = Path('../../build/apollogs')
    logs_shortcut.mkdir(exist_ok=True)
    all_logs = logs_shortcut / 'all'
    if not all_logs.exists():
        all_logs.symlink_to(target=Path(f'../{relative_apollo_logs}'), target_is_directory=True)

    Path(test_dir).mkdir(parents=True, exist_ok=True)
    latest_shortcut = Path(logs_shortcut) / 'latest'

    if latest_shortcut.exists():
        latest_shortcut.unlink()
    latest_shortcut.symlink_to(target=Path(f'../{relative_current_run_logs}'), target_is_directory=True)

    # Set the log file path
    to_file(open(test_log, "a+"))


# Set logs to the console
def stdout(message):
    if message.get("action_status", "") == "succeeded":
        return

    additional_fields = [(key, val) for key, val in message.items() if key not in
                       ("action_type", "action_status", "result", "task_uuid",
                        "timestamp", "task_level", "message_type")]

    fields = [datetime.fromtimestamp(message["timestamp"]).strftime("%d/%m/%Y %H:%M:%S.%f"),
     message.get("message_type", None),
     message.get("action_type", None),
     message.get("action_status", None),
     message.get("result", None),
     str(additional_fields).replace('\\n', '\n'),
     message["task_uuid"],
     ]

    print(f' - '.join([field for field in fields if field]))


if os.getenv('APOLLO_LOG_STDOUT', False):
    add_destinations(stdout)

if os.environ.get('KEEP_APOLLO_LOGS', "").lower() in ["true", "on"]:
    # Uncomment to see logs in console
    set_file_destination()

