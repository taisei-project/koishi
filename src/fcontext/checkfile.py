#!/usr/bin/env python3

import sys
import os.path

p = sys.argv[1]
quit(int(not os.path.exists(p)))
