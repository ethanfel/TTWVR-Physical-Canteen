// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace canteen::host {
// Only initialized section bytes are searched. The loader's singleton may
// also live in the zero-initialized tail of .data.
struct Span {
    BYTE *p{};
    size_t size{};
    bool has(const void *ptr,size_t n) const {
        auto at=reinterpret_cast<uintptr_t>(ptr), start=reinterpret_cast<uintptr_t>(p);
        return p && at>=start && n<=size && at-start<=size-n;
    }
    BYTE *unique(const void *bytes,size_t n) const {
        if (!p || !n || n>size) return nullptr;
        auto *pattern=static_cast<const BYTE *>(bytes);
        auto *end=p+size;
        auto *hit=std::search(p,p+size,pattern,pattern+n);
        if (hit==end || std::search(hit+1,p+size,pattern,pattern+n)!=end) return nullptr;
        return hit;
    }
};

template<class T> inline T read(const void *p) {
    T value;
    std::memcpy(&value,p,sizeof(value));
    return value;
}

struct Image {
    BYTE *base{};
    DWORD ntFile{}, roFile{};
    Span code,ro,initData,data;

    explicit Image(HMODULE module) : base(reinterpret_cast<BYTE *>(module)) {
        const auto *dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);
        if (dos->e_magic!=IMAGE_DOS_SIGNATURE) return;
        const auto *nt=reinterpret_cast<IMAGE_NT_HEADERS32 *>(base+dos->e_lfanew);
        if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
            nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC) return;
        ntFile=dos->e_lfanew;
        const auto *s=IMAGE_FIRST_SECTION(nt);
        for (unsigned i=0;i<nt->FileHeader.NumberOfSections;++i,++s) {
            if (s->VirtualAddress>nt->OptionalHeader.SizeOfImage ||
                s->Misc.VirtualSize>nt->OptionalHeader.SizeOfImage-s->VirtualAddress) continue;
            Span bytes{base+s->VirtualAddress,std::min(s->SizeOfRawData,s->Misc.VirtualSize)};
            if (!std::memcmp(s->Name,".text\0",6)) code=bytes;
            if (!std::memcmp(s->Name,".rdata\0",7)) { ro=bytes; roFile=s->PointerToRawData; }
            if (!std::memcmp(s->Name,".data\0",6)) { initData=bytes; data={bytes.p,s->Misc.VirtualSize}; }
        }
    }
    BYTE *relative(BYTE *op) const {
        if (!code.has(op,5)) return nullptr;
        const auto dest=reinterpret_cast<uintptr_t>(op)+5+read<int32_t>(op+1);
        auto *p=reinterpret_cast<BYTE *>(dest);
        return code.has(p,1) ? p : nullptr;
    }
    BYTE *body(BYTE *p) const {
        if (!code.has(p,1)) return nullptr;
        return *p==0xe9 ? relative(p) : p;
    }
    bool pollMethod(BYTE *method) const {
        auto *p=body(method);
        if (!code.has(p,0x1000)) return false;
        // Both methods initialize a 0x130-byte caller buffer with rep movsd
        // and write the pose validity byte at +0x12c.
        constexpr BYTE count[]{0xb9,0x4c,0,0,0}, copy[]{0xf3,0xa5}, valid[]{0x88,0x86,0x2c,1,0,0};
        auto *n=Span{p,0x300}.unique(count,sizeof(count));
        return n && Span{n,128}.unique(copy,sizeof(copy)) &&
               std::search(p,p+0x1000,valid,valid+sizeof(valid))!=p+0x1000;
    }
    // If a mod already owns the live slot, verify the original method from
    // the loaded module's backing file and leave the live chain intact.
    BYTE *originalMethod(void **slot) const {
        wchar_t path[32768];
        DWORD length=GetModuleFileNameW(reinterpret_cast<HMODULE>(base),path,32768);
        if (!length || length>=32768) return nullptr;
        HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                             nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (f==INVALID_HANDLE_VALUE) return nullptr;
        auto readAt=[f](DWORD offset,void *out,DWORD count) {
            LARGE_INTEGER pos{};
            pos.QuadPart=offset;
            DWORD got=0;
            return SetFilePointerEx(f,pos,nullptr,FILE_BEGIN) && ReadFile(f,out,count,&got,nullptr) && got==count;
        };
        // Wine can rewrite the mapped OptionalHeader.ImageBase on relocation.
        // The table entry on disk still uses the original preferred base.
        DWORD va=0;
        IMAGE_NT_HEADERS32 disk{};
        const auto offset=roFile+(reinterpret_cast<BYTE *>(slot)-ro.p);
        bool ok=readAt(ntFile,&disk,sizeof(disk)) && readAt(offset,&va,sizeof(va));
        CloseHandle(f);
        auto *mapped=reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base+ntFile);
        if (!ok || disk.Signature!=IMAGE_NT_SIGNATURE ||
            disk.FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
            disk.FileHeader.TimeDateStamp!=mapped->FileHeader.TimeDateStamp ||
            disk.OptionalHeader.SizeOfImage!=mapped->OptionalHeader.SizeOfImage ||
            va<disk.OptionalHeader.ImageBase) return nullptr;
        auto *method=reinterpret_cast<BYTE *>(reinterpret_cast<uintptr_t>(base)+(va-disk.OptionalHeader.ImageBase));
        return code.has(method,1) ? method : nullptr;
    }
    void **pollSlot(const char *type) const {
        auto *name=initData.unique(type,std::strlen(type)+1);
        if (!name || name-initData.p<8 || ro.size<20) return nullptr;
        auto *descriptor=name-8;
        void **result=nullptr;
        // MSVC x86 complete-object locator: signature, offsets, type, hierarchy.
        for (size_t i=0;i<=ro.size-20;i+=4) {
            auto *locator=ro.p+i;
            if (read<BYTE *>(locator+12)!=descriptor || read<DWORD>(locator) ||
                read<DWORD>(locator+4) || read<DWORD>(locator+8) ||
                !ro.has(read<BYTE *>(locator+16),16)) continue;
            auto *ref=ro.unique(&locator,sizeof(locator));
            if (!ref || !ro.has(ref,8*sizeof(void *))) continue;
            auto **slot=reinterpret_cast<void **>(ref)+1+6;
            if (!pollMethod(static_cast<BYTE *>(*slot)) && !pollMethod(originalMethod(slot))) continue;
            if (result) return nullptr;
            result=slot;
        }
        return result;
    }

    BYTE **xrInstance() const {
        // Identify the embedded loader's xrEndFrame by both the command name
        // passed to instance lookup and the two validated dispatch offsets.
        constexpr BYTE dispatch[]{0x8b,0x40,0x2c,0xff,0x75,8,0x8b,0x80,0x8c,0,0,0,0xff,0xd0};
        auto *end=code.unique(dispatch,sizeof(dispatch));
        if (!end || static_cast<size_t>(end-code.p)<96) return nullptr;
        constexpr BYTE lookup[]{0x50,0xc7,0x45,0xfc,0,0,0,0,0xe8};
        auto *args=Span{end-96,96}.unique(lookup,sizeof(lookup));
        if (!args || args-code.p<5 || args[-5]!=0x68) return nullptr;
        auto *command=read<BYTE *>(args-4);
        if (!ro.has(command,sizeof("xrEndFrame")) || std::memcmp(command,"xrEndFrame",sizeof("xrEndFrame"))) return nullptr;
        auto *get=body(relative(args+8));
        if (!code.has(get,128)) return nullptr;
        constexpr BYTE out[]{0x8b,0x00,0x89,0x06,0x85,0xc0};
        auto *store=Span{get+5,123}.unique(out,sizeof(out));
        if (!store || store[-5]!=0xe8) return nullptr;
        auto *access=body(relative(store-5));
        if (!code.has(access,64)) return nullptr;
        BYTE **result=nullptr;
        for (size_t i=0;i+11<=64;++i) {
            auto *ins=access+i;
            if (*ins!=0xb8 || ins[5]!=0xc3 || ins[6]!=0x68) continue;
            auto *global=read<BYTE *>(ins+1);
            if (!data.has(global,8) || read<BYTE *>(ins+7)!=global+4) continue;
            if (result) return nullptr;
            result=reinterpret_cast<BYTE **>(global);
        }
        return result;
    }
};
}
