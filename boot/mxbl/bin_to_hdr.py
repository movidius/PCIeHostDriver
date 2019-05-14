#!/usr/bin/python
#
# This script turns a Myriad X application binary into an array of bytes that
# can be embedded in the mxbl driver binary upon compilation.
#
# Copyright 2019 Intel Corporation
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this software
# or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express or
# implied warranties, other than those that are expressly stated in the License.

import os
import sys
import string
import subprocess
from shutil import copyfile

if len(sys.argv) != 2 :
    print "usage : " + sys.argv[0] + " <path/to/binary>"
    exit (1)

proc = subprocess.Popen("xxd -i " + sys.argv[1] + " > temp.h", stdout=subprocess.PIPE, shell=True)
(out, err) = proc.communicate()
if proc.returncode != 0 :
	exit (1)

os.remove("mxbl_first_stage_image.h") if os.path.exists("mxbl_first_stage_image.h") else None

with open("temp.h","r") as temp, open("mxbl_first_stage_image.h", "w") as mxbl:
    mxbl.write("#ifndef MXBL_FIRST_STAGE_IMAGE_HEADER_\n")
    mxbl.write("#define MXBL_FIRST_STAGE_IMAGE_HEADER_\n")
    mxbl.write("#include <linux/types.h>\n")
    mxbl.write("// " + os.path.basename(sys.argv[1]) + "\n")
    mxbl.write("const u8 mxbl_first_stage_image[] = {\n")

    array = temp.readlines()
    for line in array[1:-1] :
        mxbl.write(line)

    mxbl.write("#endif\n")

os.remove("temp.h")
