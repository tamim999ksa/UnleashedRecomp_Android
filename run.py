import re
with open('UnleashedRecomp/mod/mod_loader.cpp', 'r') as f:
    content = f.read()

# I am ensuring Mod has ModType and std::vector defined
