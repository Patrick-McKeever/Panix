//#include "gpt.h"
//
//struct GPTHeaderAndEntries {
//    GPTHeader *hdr_;
//    char *entries_;
//};
//
//static ds::Optional<GPTHeaderAndEntries> ReadGPT(SATAPort &port)
//{
//    void *lba1 = port.Read(1, 1);
//    auto gpt_hdr = (GPTHeader*) lba1;
//    if(gpt_hdr->signature_ != GPT_MAGIC) {
//        KHeap::Free(lba1);
//        return ds::NullOpt;
//    }
//
//    ds::DynArray<DiskRange> ranges;
//    size_t arr_size = gpt_hdr->num_part_entries_ * gpt_hdr->entry_size_;
//    size_t no_sectors = arr_size / 512 + (arr_size % 512 > 0 ? 1 : 0);
//    void *entry_arr = port.Read(gpt_hdr->entry_arr_lba_, no_sectors);
//
//    return (GPTHeaderAndEntries) {
//        .hdr_ = (GPTHeader*) lba1,
//        .entries_ = (char*) entry_arr
//    };
//}
//
//ds::Optional<GPTEntry> FindPartition(SATAPort &port, uint64_t part_guid_lo,
//                                     uint64_t part_guid_hi)
//{
//    if(ds::Optional<GPTHeaderAndEntries> gpt_opt = ReadGPT(port)) {
//        GPTHeaderAndEntries gpt = *gpt_opt;
//        for (size_t i = 0; i < gpt.hdr_->num_part_entries_; ++i) {
//            auto entry = (GPTEntry *) (gpt.entries_ + gpt.hdr_->entry_size_ * i);
//
//            if (part_guid_lo == entry->part_type_guid_lo_ &&
//                part_guid_hi == entry->part_type_guid_hi_)
//            {
//                return *entry;
//            }
//        }
//    }
//
//    return ds::NullOpt;
//}
//
//ds::Optional<GPTEntry> GetNthPartition(SATAPort &port, size_t n)
//{
//    if(ds::Optional<GPTHeaderAndEntries> gpt_opt = ReadGPT(port)) {
//        GPTHeaderAndEntries gpt = *gpt_opt;
//        if (n < gpt.hdr_->num_part_entries_) {
//            return ds::NullOpt;
//        }
//
//
//        KHeap::Free(gpt.hdr_);
//        KHeap::Free(gpt.entries_);
//        auto entry = (GPTEntry *) (gpt.entries_ + gpt.hdr_->entry_size_ * n);
//        return *entry;
//    }
//
//    return ds::NullOpt;
//}
//