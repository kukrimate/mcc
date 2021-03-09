#!/usr/bin/python3
import sys

for l in sys.stdin:
    l = l.replace("\\", "\\\\")
    l = l.replace("\"", "\\\"")
    l = l.replace("\n", "\\n")
    print("\"%s\"" %l)
