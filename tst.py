import os

EMPTY_SECTOR = 512 * b'\0'
with open("build/bin/image.hdd", "rb") as f:
    f.seek(2050*512+(282 -1) * 2)
    print(f.read(1024))
    # f.seek(2050*512)
    # sblock = f.read(1024)
    # print("NO INODES: ", int.from_bytes(sblock[0:4], byteorder='little'))
    # f.seek(2052*512)
    # bgdt = f.read(1024)

    # blk1_inode_bmp_ind = int.from_bytes(bgdt[4:8],  byteorder='little')
    # print("BLK 1 INODE BMP: ", blk1_inode_bmp_ind)
    # f.seek(2056*512)
    # blk1_inode_bmp = f.read(1024)
    # print(blk1_inode_bmp)
    # blk1_inode_tab_ind = int.from_bytes(bgdt[8:12], byteorder='little')
    # print("ind is %d" % blk1_inode_tab_ind)
    # f.seek(2058*512)
    # blk1_inode_tab = f.read(1024*10)

    # for i in range(0, 30):
    #    entry = blk1_inode_tab[i*128:(i+1)*128]
    #    if int.from_bytes(entry[0:2], byteorder='little') != 0:
    #        print("INODE %d: " % (i + 1))
    #        print("\ti_mode: ", int.from_bytes(entry[0:2], byteorder='little'))
    #        print("\ti_uid: ", int.from_bytes(entry[2:2+2], byteorder='little'))
    #        print("\ti_size: ", int.from_bytes(entry[4:4:4], byteorder='little'))
    #        print("\ti_atime: ", int.from_bytes(entry[8:8+4], byteorder='little'))
    #        print("\ti_ctime: ", int.from_bytes(entry[12:12+4], byteorder='little'))
    #        print("\ti_mtime: ", int.from_bytes(entry[16:16+4], byteorder='little'))
    #        print("\ti_dtime: ", int.from_bytes(entry[20:20+4], byteorder='little'))
    #        print("\ti_gid: ", int.from_bytes(entry[24:24+2], byteorder='little'))
    #        print("\ti_links_count: ", int.from_bytes(entry[26:26+2], byteorder='little'))
    #        print("\ti_blocks: ", int.from_bytes(entry[28:28+4], byteorder='little'))
    #        print("\ti_flags: ", int.from_bytes(entry[32:32+4], byteorder='little'))
    #        print("\ti_osd1: ", int.from_bytes(entry[36:26+4], byteorder='little'))
    #        #print(\t"i_block: ", int.from_bytes(entry[40:40+60], byteorder='little'))
    #        print("\ti_generation: ", int.from_bytes(entry[100:100+4], byteorder='little'))
    #        print("\ti_file_acl: ", int.from_bytes(entry[104:104+4], byteorder='little'))
    #        print("\ti_dir_acl: ", int.from_bytes(entry[108:108+4], byteorder='little'))
    #        print("\ti_faddr: ", int.from_bytes(entry[112:112+4], byteorder='little'))
    #        print("\ti_osd2: ", int.from_bytes(entry[116:116+12], byteorder='little'))
