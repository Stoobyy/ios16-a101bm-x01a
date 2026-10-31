"""Add startup diagnostics to generated ath10k source; reference trees stay untouched."""
from pathlib import Path
import re
import sys
p = Path(sys.argv[1])
s = p.read_text()
if 'LX10K_STARTUP_CHECKPOINTS' in s:
    raise SystemExit(0)
p.with_suffix('.c.before-checkpoints').write_text(s)
for start, end in [('int ath10k_core_start(', 'EXPORT_SYMBOL(ath10k_core_start)'), ('static int ath10k_core_probe_fw(', 'static void ath10k_core_register_work(')]:
    a = s.index(start)
    z = s.find(end, a)
    if z < 0:
        raise SystemExit('missing boundary: ' + end)
    part = s[a:z]
    # Literal source newlines and C escapes are kept distinct below.
    def checkpoint(m):
        indent, statement, call = m.groups()
        return f'{indent}lx_tell("lx10k-check: BEFORE {call}\\n");\n{indent}{statement}\n{indent}lx_tell("lx10k-check: AFTER {call}\\n");'
    part = re.sub(r'(?m)^(\t+)((?:ret|status) = (ath10k_\w+)\([^;\n]*\);)$', checkpoint, part)
    s = s[:a] + part + s[z:]
s = '/* LX10K_STARTUP_CHECKPOINTS */\nextern void lx_tell(const char *format, ...);\n' + s
p.write_text(s)
