#ifndef VMOUNT_H
#define VMOUNT_H

#include <ds/hash_map.h>
#include <ds/ref_cnt_ptr.h>
#include <ds/string.h>
#include <sys/fs/ext2_mount.h>
#include <sys/sata_port.h>
#include <sys/gpt.h>

class VMount {
public:
    virtual ds::RefCntPtr<VNode> ReadRootDir() = 0;
};

#endif
