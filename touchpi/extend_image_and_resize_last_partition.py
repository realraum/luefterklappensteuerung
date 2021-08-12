#!/usr/bin/python3

## apt install python3-parted
import parted
import sys
import os

__author__ = """Bernhard Tittelbach"""
__copyright__ = """Copyright 2018 Bernhard Tittelbach"""

def resizeToMaxExt4PartitionOptimizedForMicroSDHC(parted_disk, desired_size_bytes=None):
    ## get largest free region
    # parted_largestfreeregion_geometry = max(parted_disk.getFreeSpaceRegions(),key=lambda x: x.getLength())

    ## define optimum settings for micrSDHC card (first 4MiB contains only MBR and everything aligned to 4MiB)
    optAlignmentForRasperry=parted_disk.device.optimumAlignment
    optAlignmentForRasperry.offset=int(4*1024**2/parted_disk.device.sectorSize) #8192
    optAlignmentForRasperry.grainSize=int(4*1024**2/parted_disk.device.sectorSize) #8192
    assert(optAlignmentForRasperry.offset == 8192)
    assert(optAlignmentForRasperry.grainSize == 8192)

    lastpart_start = parted_disk.partitions[-1].geometry.start

    geom = parted.Geometry(device=parted_disk.device, start=lastpart_start, length=parted_disk.device.length-lastpart_start)
    constraint = parted.Constraint(exactGeom=geom)
    parted_disk.maximizePartition(partition=parted_disk.partitions[-1], constraint=constraint)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("This tool will extend the last partition in a Raspbian image file")
        print("")
        print("Usage:")
        print("\t%s <raspbian.img> [+image_bytes]" % (sys.argv[0]))
        sys.exit(1)

    image_newsize = None
    cur_filesize = 0
    image_filepath = sys.argv[1]
      
    if not os.path.isfile(image_filepath):
        print("Raspbian Image '%s' not found" % image_filepath)
        sys.exit(2)

    ####### save the MBR from libparted's mangling transgressions and disk id changing #######

    mbr=b''
    with open(image_filepath,"rb") as ifh:
        mbr=ifh.read(446)
        ifh.seek(0,2)
        cur_filesize=ifh.tell()

    ####### check what to do

    if len(sys.argv) > 2:
        if sys.argv[2][0] == '+':
            image_newsize = cur_filesize + int(sys.argv[2][1:])
        else:
            image_newsize = int(sys.argv[2])

    ####### resize image file #######

    if image_newsize and cur_filesize < image_newsize:
        os.truncate(image_filepath, image_newsize)

    ####### partion image file #######

    parted_dev = parted.getDevice(image_filepath)
    parted_disk = parted.Disk(parted_dev)
   
    if len(parted_disk.partitions)+1 > parted_disk.maxSupportedPartitionCount:
        sys.exit(1)

    # resizeToMaxExt4PartitionOptimizedForMicroSDHC(parted_disk, part_home_size) #part3 Home
    resizeToMaxExt4PartitionOptimizedForMicroSDHC(parted_disk) #part4 Var, max remaining size
    parted_disk.commit()

    ####### restore MBR and disk id #######

    with open(image_filepath,"r+b") as ifh:
        ifh.seek(0,0)
        ifh.write(mbr)
