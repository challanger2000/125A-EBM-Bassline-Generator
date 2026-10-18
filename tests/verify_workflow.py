"""Guard the paid Windows workflow's transitive local source coverage."""
from pathlib import Path
import fnmatch
import re

root = Path(__file__).resolve().parents[1]
workflow = (root / '.github/workflows/windows-vst3.yml').read_text()
paths = workflow.split('    paths:\n', 1)[1].split('  workflow_dispatch:', 1)[0]
patterns = re.findall(r"^\s+- '([^']+)'\s*$", paths, re.MULTILINE)
required = [root / 'CMakeLists.txt', root / '.github/workflows/windows-vst3.yml']
for directory in ('src', 'include', 'tests', 'tools', 'resource'):
    required.extend(p for p in (root / directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
uncovered = [p.relative_to(root).as_posix() for p in required
             if not any(fnmatch.fnmatchcase(p.relative_to(root).as_posix(), pattern) for pattern in patterns)]
if uncovered:
    raise SystemExit('Windows source coverage missing: ' + ', '.join(uncovered))
if 'fix/p1-codex-qa-2026-10-15' not in workflow or 'ebm_repair_tests ebm_qa_exports' not in workflow:
    raise SystemExit('Repair branch or required test/export build targets missing')
print(f'PASS: Windows filter covers all {len(required)} local source/test/resource inputs; repair/export targets included')
