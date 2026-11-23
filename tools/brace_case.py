#!/usr/bin/env python3
"""brace_case.py FILE LINE: wrap the switch case whose body contains LINE in { }.
Scans up to the nearest case/default label and down to the next label at the same indent."""
import re, sys
f, n = sys.argv[1], int(sys.argv[2])
src = open(f).read().split('\n')
lab = re.compile(r'^(\s*)(case\b.*|default\s*):\s*(//.*)?$')
i = n - 1
while not lab.match(src[i]):
    i -= 1
ind = lab.match(src[i]).group(1)
j = n
while j < len(src):
    m = lab.match(src[j])
    if m and m.group(1) == ind:
        break
    if src[j].startswith(ind + '}') and len(src[j]) - len(src[j].lstrip()) == len(ind) - 4:
        break  # end of the switch
    if re.match(r'^\s*\}\s*$', src[j]) and len(src[j]) - len(src[j].lstrip()) < len(ind):
        break
    j += 1
m = lab.match(src[i])
src[i] = (src[i][:src[i].index('//')].rstrip() + ' { ' + m.group(3)) if m.group(3) else src[i].rstrip() + ' {'
src.insert(j, ind + '}')
open(f, 'w').write('\n'.join(src))
print(f'{f}: braced lines {i+1}..{j+1}')
