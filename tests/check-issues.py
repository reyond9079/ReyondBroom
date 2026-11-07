from pathlib import Path
import re
import sys
root=Path(sys.argv[1]);log=(root/'qconsole.log').read_text(errors='replace')
assert not re.search(r'FTEBROOM_(ISSUES|PREFS)_FAIL|CSQC_Abort|runaway loop|stack overflow',log),log[-20000:]
assert log.count('FTEBROOM_ISSUES_PASS ')==12 and log.count('FTEBROOM_PREFS_PASS ')==6,log[-20000:]
assert (root/'issues.png').exists() and (root/'preferences.png').exists()
print('PASS: issue detection, undoable fixes, stale issue protection, shortcut conflicts and saved preferences')
