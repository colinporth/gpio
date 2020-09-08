// cSnapshot.cpp
#include "cSnapshot.h"

#include <bcm_host.h>

#include "../../shared/utils/utils.h"
#include "../../shared/utils/cLog.h"
using namespace std;

namespace {
  DISPMANX_DISPLAY_HANDLE_T mDisplay;
  DISPMANX_MODEINFO_T mModeInfo;
  DISPMANX_RESOURCE_HANDLE_T mSnapshot;
  VC_RECT_T mVcRect;
  }

// public
//{{{
cSnapshot::cSnapshot (const uint16_t width, const uint16_t height) : mWidth(width), mHeight(height) {

  // dispmanx init
  bcm_host_init();

  mDisplay = vc_dispmanx_display_open (0);
  if (!mDisplay) {
    // error return
    cLog::log (LOGERROR, "vc_dispmanx_display_open failed");
    return;
    }

  if (vc_dispmanx_display_get_info (mDisplay, &mModeInfo)) {
    // error return
    cLog::log (LOGERROR, "vc_dispmanx_display_get_info failed");
    return;
    }

  uint32_t imageHandle;
  mSnapshot = vc_dispmanx_resource_create (VC_IMAGE_RGB565, width, height, &imageHandle);
  if (!mSnapshot) {
    // error return
    cLog::log (LOGERROR, "vc_dispmanx_resource_create failed");
    return;
    }

  vc_dispmanx_rect_set (&mVcRect, 0, 0, width, height);

  cLog::log (LOGINFO, "display %dx%d", mModeInfo.width, mModeInfo.height);
  }
//}}}
//{{{
cSnapshot::~cSnapshot() {
  vc_dispmanx_resource_delete (mSnapshot);
  vc_dispmanx_display_close (mDisplay);
  }
//}}}

//{{{
void cSnapshot::snap (uint16_t* frameBuf) {

  vc_dispmanx_snapshot (mDisplay, mSnapshot, DISPMANX_TRANSFORM_T(0));
  vc_dispmanx_resource_read_data (mSnapshot, &mVcRect, frameBuf, mWidth * 2);
  }
//}}}
