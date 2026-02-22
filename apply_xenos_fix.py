import sys
import re

file_path = 'build_overrides/XenosRecomp/XenosRecomp/shader_recompiler.cpp'
with open(file_path, 'r') as f:
    lines = f.readlines()

# Locate the union definition inside the loop (around line 1720)
# We look for "VertexFetchInstruction vertexFetch;" inside a union
start_idx = -1
for i, line in enumerate(lines):
    if "VertexFetchInstruction vertexFetch;" in line:
        # verify it is inside union
        if "union" in lines[i-2]:
            start_idx = i - 2
            break

if start_idx == -1:
    print("Could not find union definition")
    sys.exit(1)

# Find the end of the union
# It has a nested struct.
# union {
#   ...
#   struct { ... };
# };

# Skip to the nested struct
struct_end_idx = -1
union_end_idx = -1

for i in range(start_idx, len(lines)):
    if "uint32_t code2;" in lines[i]:
        # next line should be };
        if "};" in lines[i+1]:
            struct_end_idx = i + 1
            if "};" in lines[i+2]:
                union_end_idx = i + 2
                break

if union_end_idx == -1:
    print("Could not find union end")
    sys.exit(1)

# Modify struct end
lines[struct_end_idx] = lines[struct_end_idx].replace("};", "} raw;")

# Modify union end
lines[union_end_idx] = lines[union_end_idx].replace("};", "} inst;")

# Now replace usages in the following lines until end of loop?
# Actually just replaces the immediate assignments and usages.

# code0 = instructionCode[0]; -> inst.raw.code0 = instructionCode[0];
# code1 = instructionCode[1];
# code2 = instructionCode[2];

for i in range(union_end_idx + 1, union_end_idx + 20):
    lines[i] = lines[i].replace("code0 =", "inst.raw.code0 =")
    lines[i] = lines[i].replace("code1 =", "inst.raw.code1 =")
    lines[i] = lines[i].replace("code2 =", "inst.raw.code2 =")

# Now replace usages of vertexFetch, textureFetch, alu
# Be careful not to replace member names in the definition (lines start_idx to union_end_idx)
# But we are iterating over the whole file or just the function?
# The variables are local to the loop.
# But wait, if we changed the union to be named , the members are no longer directly accessible as local variables.
# So we MUST replace all occurrences in the scope of this union.

# The scope is the for loop: for (uint32_t i = 0; i < count; i++) { ... }
# We need to find where this loop ends.
# It starts around line 1723 (based on earlier cat output).

# Let's just do a simple replacement for the lines following the union declaration.
# We can scan forward until we see the end of the loop or valid replacements.

current_line = union_end_idx + 1
brace_count = 0 # We are inside the for loop which has braces.
# Actually we can just replace specific patterns that we know are usages.

patterns = [
    (r'vertexFetch\.', 'inst.vertexFetch.'),
    (r'recompile\(vertexFetch', 'recompile(inst.vertexFetch'),
    (r'textureFetch\.', 'inst.textureFetch.'),
    (r'recompile\(textureFetch', 'recompile(inst.textureFetch'),
    (r'recompile\(alu\)', 'recompile(inst.alu)'),
    (r'recompile\(inst\.textureFetch', 'recompile(inst.textureFetch'), # Handle already patched lines (idempotent-ish)
    (r'recompile\(inst\.alu\)', 'recompile(inst.alu)'),
    (r'inst\.inst\.', 'inst.') # Fix double application if any
]

# We need to apply this only until the end of the function or loop.
# The loop ends when brace count goes back to initial level.
# But finding brace matching is hard without parsing.
# However, these variable names are likely unique enough in this file or at least in this section.
# Let's apply to the next 100 lines.

for i in range(union_end_idx + 1, union_end_idx + 150):
    for pat, repl in patterns:
        lines[i] = re.sub(pat, repl, lines[i])

# Check for double application like inst.inst.
for i in range(union_end_idx + 1, union_end_idx + 150):
    lines[i] = lines[i].replace("inst.inst.", "inst.")

with open(file_path, 'w') as f:
    f.writelines(lines)

print("Applied fixes")
