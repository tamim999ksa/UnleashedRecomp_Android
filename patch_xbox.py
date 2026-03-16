import os
import re

def patch_file(path, pattern, replacement, flags=re.DOTALL):
    with open(path, 'r') as f:
        content = f.read()
    new_content = re.sub(pattern, replacement, content, flags=flags)
    with open(path, 'w') as f:
        f.write(new_content)

# xbox.h: _XXOVERLAPPED unnamed struct
# From:
# struct
# {
#     be<uint32_t> Error;
#     be<uint32_t> Length;
# };
# To:
# struct _ErrorLength
# {
#     be<uint32_t> Error;
#     be<uint32_t> Length;
# } errorLength;

patch_file('tools/XenonRecomp/XenonUtils/xbox.h',
           r'struct\s+\{\s+be<uint32_t> Error;\s+be<uint32_t> Length;\s+\};',
           'struct { be<uint32_t> Error; be<uint32_t> Length; } _s1;')

# Fix usages of Error and Length if any (unnamed structs usually have transparent access but Error/Length have constructors)
# Wait, if I name it, I have to update usages.
# Let's see if I can just use a regular uint32_t and ByteSwap it manually or something?
# No, be<T> is used everywhere.
# Actually, the error is "member ... with constructor not allowed in anonymous aggregate".
# This is a C++ restriction for anonymous members in unions.
# If I just give the union a name, or the struct a name, it might work.
# But these are often used as bit-fields or for binary layout.

# Let's try to just make the union not anonymous?
# No, let's try to make the struct not anonymous.
