import sys
import re
from datetime import datetime

def check_log_order(filename):
    print(f"Checking {filename}...")
    
    # Regex to capture timestamp: 2025-01-28 09:03:08
    # Based on format: "%Y-%m-%d %H:%M:%S"
    timestamp_pattern = re.compile(r'^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})')
    
    last_dt = None
    line_num = 0
    errors = 0
    
    with open(filename, 'r') as f:
        for line in f:
            line_num += 1
            match = timestamp_pattern.match(line)
            if match:
                dt_str = match.group(1)
                try:
                    current_dt = datetime.strptime(dt_str, "%Y-%m-%d %H:%M:%S")
                    
                    if last_dt and current_dt < last_dt:
                        print(f"Error at line {line_num}: Time went backwards! {last_dt} -> {current_dt}")
                        print(f"Line: {line.strip()}")
                        errors += 1
                        
                    last_dt = current_dt
                except ValueError as e:
                    print(f"Parse error line {line_num}: {e}")
            
            # Optional: Check if lines are interleaved/corrupted (simple heuristic)
            if "load test" not in line and "Thread" not in line:
                 # This might happen if a line was cut off
                 pass

    if errors == 0:
        print("SUCCESS: Logs are in chronological order.")
        sys.exit(0)
    else:
        print(f"FAILED: Found {errors} ordering errors.")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: verify_logs.py <logfile>")
        sys.exit(1)
    
    # We might need to find the actual log file name if it has a timestamp suffix
    # The C code rotates it, so it's likely stress_test_log.<timestamp>
    import glob
    import os
    
    target = sys.argv[1]
    if not os.path.exists(target):
         # Try to find the latest log with that prefix
         candidates = glob.glob(f"{target}.*")
         if candidates:
             target = max(candidates, key=os.path.getctime)
             print(f"Targeting latest log file: {target}")
         else:
             print(f"Could not find log file {target}")
             sys.exit(1)
             
    check_log_order(target)
