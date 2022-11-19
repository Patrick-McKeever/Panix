#include <cstdint>
#include <sys/fs/vfs.h>
#include <sys/fs/ext2_vnode.h>
#include <sys/gpt.h>
#include <ds/hash_map.h>
#include <stddef.h>
#include <sys/acpi.h>
#include <sys/buddy_allocator.h>
#include <sys/kheap.h>
#include <sys/log.h>
#include <sys/page_map.h>
#include <sys/pcie_tree.h>
#include <sys/sata_port.h>
#include <sys/stivale2.h>

static uint8_t stack[8192];

static struct stivale2_header_tag_smp smp_hdr_tag = {
    .tag = {
		.identifier = STIVALE2_HEADER_TAG_SMP_ID,
		.next = 0,
	},
	.flags = 0
};

static struct stivale2_header_tag_framebuffer framebuffer_hdr_tag = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
		.next = (uintptr_t) &smp_hdr_tag
    },
    .framebuffer_width  = 1920,
    .framebuffer_height = 1080,
    .framebuffer_bpp    = 16 
};

static struct stivale2_struct_tag_kernel_base_address base_addr_tag = {
    .tag = {
		.identifier = STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID,
		.next = (uintptr_t) &framebuffer_hdr_tag
	}
};

static struct stivale2_struct_tag_pmrs pmr_tag = {
	.tag = {
		.identifier = STIVALE2_STRUCT_TAG_PMRS_ID,
		.next = (uintptr_t) &base_addr_tag
	}
};

static struct stivale2_struct_tag_modules modules_tag = {
	.tag = {
		.identifier = STIVALE2_STRUCT_TAG_MODULES_ID,
		.next = (uintptr_t) &pmr_tag
	}
};

static struct stivale2_struct_tag_rsdp rsdp_tag = {
	.tag = {
		.identifier = STIVALE2_STRUCT_TAG_RSDP_ID,
		.next = (uintptr_t) &modules_tag
	}
};

static struct stivale2_struct_tag_hhdm hhdm_tag = {
    .tag = {
        .identifier = STIVALE2_STRUCT_TAG_HHDM_ID,
        .next = (uintptr_t) & rsdp_tag
    }
};

__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header stivale_hdr = {
    .entry_point = 0,
    .stack = (uintptr_t)stack + sizeof(stack),
    .flags = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),
    .tags = (uintptr_t) &hhdm_tag
};
 

void *stivale2_get_tag(struct stivale2_struct *stivale2_struct, uint64_t id)
{
    struct stivale2_tag *current_tag = (stivale2_tag*) stivale2_struct->tags;
    for (;;) {
        if (current_tag == NULL) {
            return NULL;
        }
 
        if (current_tag->identifier == id) {
            return current_tag;
        }

        current_tag = (stivale2_tag*) current_tag->next;
    }
}

ds::Optional<int> Test(bool a)
{
    if(a) {
        return 1;
    } else {
        return ds::NullOpt;
    }
}

extern "C"
void _start(struct stivale2_struct *stivale2_struct)
{
    {
    static constexpr size_t mmap_id = STIVALE2_STRUCT_TAG_MEMMAP_ID;
    static constexpr size_t pmrs_id = STIVALE2_STRUCT_TAG_PMRS_ID;
    static constexpr size_t base_id = STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID;
    static constexpr size_t rsdp_id = STIVALE2_STRUCT_TAG_RSDP_ID;

    auto *memmap    = (stivale2_struct_tag_memmap *)
            stivale2_get_tag(stivale2_struct, mmap_id);
    auto *pmrs      = (stivale2_struct_tag_pmrs *)
            stivale2_get_tag(stivale2_struct, pmrs_id);
    auto *kern_base = (stivale2_struct_tag_kernel_base_address *)
            stivale2_get_tag(stivale2_struct, base_id);
    auto *rsdp_tag  = (stivale2_struct_tag_rsdp *)
            stivale2_get_tag(stivale2_struct, rsdp_id);

    BuddyAllocator::InitBuddyAllocator(*memmap);
    PageMap kernel_page_map(memmap, kern_base, pmrs);
    kernel_page_map.Load();
    KHeap::Init(1024, &kernel_page_map);

    ds::Optional<ds::HashMap<ds::String, void *>> acpi_tabs;
    if (acpi_tabs = ACPI::ParseRoot(rsdp_tag)) {
        if (ds::Optional<void *> addr = acpi_tabs->Lookup("MCFG")) {
            PCIeTree                 pcie_tree(*addr);
            ds::DynArray<PCIeDevice> hba_controllers =
                                             pcie_tree.GetDevicesBySubclass(
                                                     0x01, 0x06);

            if (hba_controllers.Size()) {
                PCIeDevice::PCIHeader    *header        = hba_controllers.Lookup(
                        0).header_;
                uint32_t                 *header_dwords = reinterpret_cast<uint32_t *>(header);
                auto                     hba_mmio_base  = (uintptr_t) header_dwords[0x9];
                ds::DynArray<SATAPort *> a              =
                                                 EnumerateDevices(
                                                         (volatile void *) hba_mmio_base);
                SATAPort                 *port          = a.Lookup(0);

                ds::Optional<GPTEntry> ext2_opts;
                if (ext2_opts = port->FindPartition(ESP_GUID_LO, ESP_GUID_HI)) {
                    VFS vfs(port, *ext2_opts);
                    ds::Optional<FileHandle> ho = vfs.Open("/test_dir/test_dir2/test_dir3/test.txt");
                    ds::Optional<FileHandle> h1o = vfs.Open("/test_dir/test_dir2/test_dir3/test.txt");
                    FileHandle h1 = *h1o;
                    ds::Optional<FileHandle> h2o = vfs.Open("/test_dir/test_dir2/test_dir3/test.txt");
                    FileHandle h2 = *h2o;
                    vfs.Close(h1);
                    //ds::Optional<FileHandle> ho = vfs.Open("/test_dir/test_dir2/test_dir3/test.txt");
                    if(ho) {
                        FileHandle h = *ho;
                        ds::Optional<ds::String> text = h2.Read(h.GetLength());
                        if(text) {
                            Log("%s", *text);
                        }
                        vfs.Close(h);
                    }
                    vfs.Close(h2);


                    //Ext2Mount mnt(port, ext2_opts->Lookup(0).start_lba_);

                    //VNode *root = mnt.ReadRootDir();
                    ////ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>>
                    ////        files;
                    ////files = root->ReadDir();
                    ////ds::Optional<ds::RefCntPtr<VNode>> kernel_file_opt;
                    ////kernel_file_opt = files->Lookup("empty_dir");
                    ////(*kernel_file_opt)->ReadDir();


                    ////if(ds::Optional<ds::RefCntPtr<VNode>> ndir = root->MkDir("dir_test")) {
                    ////    for(int i = 0; i < 200; ++i) {
                    ////        ds::String f_name = "   .txt";
                    ////        f_name[0] = (char) (i % 26 + 65);
                    ////        f_name[1] = (char) (i / 10 + 48);
                    ////        f_name[2] = (char) (i % 10 + 48);
                    ////        (*ndir)->Touch(f_name);
                    ////    }
                    ////}

                    ////root->Link("hard_link", 17);
                    ////root->SymLink("sym_link", "limine.cfg");
//                  //  root->ReadDir();
//                  //  Log("Rm");
//                  //  root->Remove("test.txt");
//                  //  root->ReadDir();
                    ////auto wbuff = (char*) KHeap::Allocate(1024*15);
                    ////for(size_t i = 0; i < 1024*15; i += 4) {
                    ////    wbuff[i + 0] = 'P';
                    ////    wbuff[i + 1] = 'J';
                    ////    wbuff[i + 2] = 'M';
                    ////    wbuff[i + 3] = '\n';
                    ////}
                    ////ds::Optional<ds::RefCntPtr<VNode>> new_file =
                    ////        root->Touch("123.txt");
                    ////(*new_file)->Write(wbuff, 1024*15);
                    ////Log("Len after is %d bytes\n",
                    ////    (*new_file)->GetLength());

                    //ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>>
                    //        files;
                    //files = root->ReadDir();
                    //if (files) {
                    //    Log("Files\n");
                    //    ds::Optional<ds::RefCntPtr<VNode>> kernel_file_opt;
                    //    kernel_file_opt = files->Lookup("test.txt");
                    //    if (kernel_file_opt) {
                    //        ds::RefCntPtr<VNode> kernel_file = *kernel_file_opt;
                    //        uint64_t             len         = kernel_file->GetLength();
                    //        Log("File len is 0x%x\n", len);
                    //        char *buff = (char *) KHeap::Allocate(len);
                    //        kernel_file->Read(buff, len);

                    //        Log("Text\n");
                    //        for(size_t i = 0; i < len; ++i) {
                    //            Log("%c", buff[i]);
                    //        }
                    //        Log("\n\n");
                    //    }
                    //}
                }

                delete port;
            }

            Log("PCIE parsed\n\n");
        } else {
            Log("MCFG not found.\n");
        }
    } else {
        Log("Error parsing root\n\n");
    }

    Log("SUCCESS\n");
    }

	while(1) {
	    __asm__("hlt");
	}
}
