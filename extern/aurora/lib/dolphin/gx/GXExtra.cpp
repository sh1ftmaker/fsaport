#include "../../host_alloc.hpp"
#include "gx.hpp"
#include "__gx.h"
#include "dolphin/gx/GXAurora.h"

extern "C" {
void GXDestroyTexObj(GXTexObj* obj_) {
  AURORA_HOST_ALLOC;
  auto* obj = reinterpret_cast<GXTexObj_*>(obj_);
  if (obj->texObjId != 0) {
    GX_WRITE_AURORA(GX_LOAD_AURORA_DESTROY_TEXOBJ);
    GX_WRITE_U32(obj->texObjId);
  }
  obj->texObjId = 0;
}

void GXDestroyTlutObj(GXTlutObj* obj_) {
  AURORA_HOST_ALLOC;
  auto* obj = reinterpret_cast<GXTlutObj_*>(obj_);
  if (obj->tlutObjId != 0) {
    GX_WRITE_AURORA(GX_LOAD_AURORA_DESTROY_TLUT);
    GX_WRITE_U32(obj->tlutObjId);
  }
  obj->tlutObjId = 0;
}

void GXDestroyCopyTex(void* dest) {
  AURORA_HOST_ALLOC;
  if (dest != nullptr) {
    GX_WRITE_AURORA(GX_LOAD_AURORA_DESTROY_COPY_TEX);
    GX_WRITE_U64(reinterpret_cast<u64>(dest));
  }
}
}
