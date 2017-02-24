#include <iostream>

using namespace std;

#include "reaper/reaper_plugin.h"
#include <ossia/ossia.hpp>

#define BASE_NAMESPACE "/reaper"

// VAR & MACROS -------------------------------------------------------------

REAPER_PLUGIN_HINSTANCE g_hInst;
HWND g_parent;
HWND ossia_dockwindow;

// REAPER FUNCTION POINTERS  ----------------------------------------------

HWND (*GetMainHwnd)();
void (*DockWindowAdd)(HWND hwnd, const char* name, int pos, bool allowShow);

int (*GetNumTracks)();
MediaTrack* (*GetTrack)(ReaProject* proj, int trackidx);

int (*TrackFX_GetCount)(MediaTrack* track);
bool (*TrackFX_GetFXName)(MediaTrack* track, int fx, char* buf, int buf_sz);

int (*TrackFX_GetNumParams)(MediaTrack* track, int fx);
double (*TrackFX_GetParam)(MediaTrack* track, int fx, int param, double* minvalOut, double* maxvalOut);
bool (*TrackFX_GetParameterStepSizes)(MediaTrack* track, int fx, int param, double* stepOut,
                                      double* smallstepOut, double* largestepOut,
                                      bool* istoggleOut);
bool (*TrackFX_GetParamName)(MediaTrack* track, int fx, int param, char* buf, int buf_sz);
bool (*TrackFX_SetParam)(MediaTrack *track, int fx, int param, double val);
HWND (*TrackFX_GetFloatingWindow)(MediaTrack* track, int index);

// OSSIA ENTRYPOINT  ------------------------------

bool initOSSIAConfiguration() {
    // loading default device
    ossia::net::generic_device osc_default_device(
                std::make_unique<ossia::net::osc_protocol>("127.0.0.1", 9088, 9089), "ossia_osc");

    return true;é

}



// REAPER PLUGIN ENTRYPOINT  ------------------------------

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int
REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance,
                         reaper_plugin_info_t *rec) {

    g_hInst = hInstance;

    if(rec) {

        if(rec->caller_version != REAPER_PLUGIN_VERSION
                || !rec->GetFunc) return 0;

        *((void **)&GetMainHwnd) = rec->GetFunc("GetMainHwnd");
        *((void **)&GetNumTracks) = rec->GetFunc("GetNumTracks");
        *((void **)&GetTrack) = rec->GetFunc("GetTrack");
        *((void **)&TrackFX_GetCount) = rec->GetFunc("TrackFX_GetCount");
        *((void **)&TrackFX_GetFXName) = rec->GetFunc("TrackFX_GetFXName");
        *((void **)&TrackFX_GetNumParams) = rec->GetFunc("TrackFX_GetNumParams");
        *((void **)&TrackFX_GetParam) = rec->GetFunc("TrackFX_GetParam");
        *((void **)&TrackFX_GetParameterStepSizes) = rec->GetFunc("TrackFX_GetParameterStepSizes");
        *((void **)&TrackFX_GetParamName) = rec->GetFunc("TrackFX_GetParamName");
        *((void **)&TrackFX_SetParam) = rec->GetFunc("TrackFX_SetParam");
        *((void **)&TrackFX_GetFloatingWindow) = rec->GetFunc("TrackFX_GetFloatingWindow");
        *((void **)&DockWindowAdd) = rec->GetFunc("DockWindowAdd");

        // safety check for bindings

        if (    !GetMainHwnd ||
                !GetNumTracks ||
                !GetTrack ||
                !TrackFX_GetCount ||
                !TrackFX_GetFXName ||
                !TrackFX_GetNumParams ||
                !TrackFX_GetParam ||
                !TrackFX_GetParameterStepSizes ||
                !TrackFX_GetParamName ||
                !TrackFX_SetParam ||
                !TrackFX_GetFloatingWindow ||
                !DockWindowAdd

                ) return 0;

        // test: deploy 1st track & 1st fx & 1st fx parameter
        char* fxname_buf;
        MediaTrack* first_track = GetTrack(0, 0);
        TrackFX_GetFXName(first_track, 0, fxname_buf, 1024);

        // docker window
        //DockWindowAdd(ossia_dockwindow);

    } else return 0;
}
}
