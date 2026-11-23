#ifndef _JSYSTEM_JUT_JUTNAMEPAD_H
#define _JSYSTEM_JUT_JUTNAMEPAD_H

#include "port/be.h"
#include "types.h"

struct ResNTAB
{
    inline const char *getName(u32 index) const { return ((const char *)mEntries) + mEntries[index].mOffs - 4; }

    BE<u16> mEntryNum; // _00
    BE<u16> mPad0;     // _02
    struct Entry
    {
        BE<u16> mKeyCode; // _04
        BE<u16> mOffs;    // _06
    } mEntries[1];
};

struct JUTNameTab
{
    JUTNameTab();
    JUTNameTab(const ResNTAB *pNameTable);

    virtual ~JUTNameTab() {}

    void setResource(const ResNTAB *pNameTable);
    int getIndex(const char *) const;
    const char *getName(u16 index) const;
    u16 calcKeyCode(const char *pName) const;

    const ResNTAB *getResNameTable() const { return mNameTable; }
    inline void clearResNameTable() { delete[] mNameTable; }

    // _00 VTBL
    const ResNTAB *mNameTable; // _04
    const char *mStrData;      // _08
    u16 mNameNum;              // _0C
};

#endif /* JUTNAMETAB_H */
