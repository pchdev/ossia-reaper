#include <reaper_plugin.h>
#include <csurf.h>

#include <ossia/ossia.hpp>
#include <ossia/network/oscquery/oscquery_server.hpp>

// CONTROL SURFACE EXTENDED CALLBACKS ------------------------------------------------------------------------------------------------------

#define CSURF_EXT_RESET 0x0001FFFF                  // clear all surface state and reset (harder reset than SetTrackListChange)
#define CSURF_EXT_SETINPUTMONITOR 0x00010001        // parm1=(MediaTrack*)track, parm2=(int*)recmonitor
#define CSURF_EXT_SETMETRONOME 0x00010002           // parm1=0 to disable metronome, !0 to enable
#define CSURF_EXT_SETAUTORECARM 0x00010003          // parm1=0 to disable autorecarm, !0 to enable
#define CSURF_EXT_SETRECMODE 0x00010004             // parm1=(int*)record mode: 0=autosplit and create takes, 1=replace (tape) mode
#define CSURF_EXT_SETSENDVOLUME 0x00010005          // parm1=(MediaTrack*)track, parm2=(int*)sendidx, parm3=(double*)volume
#define CSURF_EXT_SETSENDPAN 0x00010006             // parm1=(MediaTrack*)track, parm2=(int*)sendidx, parm3=(double*)pan
#define CSURF_EXT_SETFXENABLED 0x00010007           // parm1=(MediaTrack*)track, parm2=(int*)fxidx, parm3=0 if bypassed, !0 if enabled
#define CSURF_EXT_SETFXPARAM 0x00010008             // parm1=(MediaTrack*)track, parm2=(int*)(fxidx<<16|paramidx), parm3=(double*)normalized value
#define CSURF_EXT_SETLASTTOUCHEDFX 0x0001000A       // parm1=(MediaTrack*)track, parm2=(int*)mediaitemidx (may be NULL), parm3=(int*)fxidx. all parms NULL=clear last touched FX
#define CSURF_EXT_SETFOCUSEDFX 0x0001000B           // parm1=(MediaTrack*)track, parm2=(int*)mediaitemidx (may be NULL), parm3=(int*)fxidx. all parms NULL=clear focused FX
#define CSURF_EXT_SETLASTTOUCHEDTRACK 0x0001000C    // parm1=(MediaTrack*)track
#define CSURF_EXT_SETMIXERSCROLL 0x0001000D         // parm1=(MediaTrack*)track, leftmost track visible in the mixer
#define CSURF_EXT_SETBPMANDPLAYRATE 0x00010009      // parm1=*(double*)bpm (may be NULL), parm2=*(double*)playrate (may be NULL)
#define CSURF_EXT_SETPAN_EX 0x0001000E              // parm1=(MediaTrack*)track, parm2=(double*)pan, parm3=(int*)mode 0=v1-3 balance, 3=v4+ balance, 5=stereo pan, 6=dual pan. for modes 5 and 6, (double*)pan points to an array of two doubles. if a csurf supports CSURF_EXT_SETPAN_EX, it should ignore CSurf_SetSurfacePan.
#define CSURF_EXT_SETRECVVOLUME 0x00010010          // parm1=(MediaTrack*)track, parm2=(int*)recvidx, parm3=(double*)volume
#define CSURF_EXT_SETRECVPAN 0x00010011             // parm1=(MediaTrack*)track, parm2=(int*)recvidx, parm3=(double*)pan
#define CSURF_EXT_SETFXOPEN 0x00010012              // parm1=(MediaTrack*)track, parm2=(int*)fxidx, parm3=0 if UI closed, !0 if open
#define CSURF_EXT_SETFXCHANGE 0x00010013            // whenever FX are added, deleted, or change order


// GLOBALS --------------------------------------------------------------------
REAPER_PLUGIN_HINSTANCE g_hInst;
HWND g_parent;
HWND g_dockwindow;

// FUNCTION POINTERS ----------------------------------------------------------

// projects -------------------------------------------------------------------
void (*GetProjectName)(ReaProject* project, char* buf, int buf_sz);

// windows --------------------------------------------------------------------
HWND (*GetMainHwnd)();
void (*DockWindowAdd) (HWND hwnd, char* name, int pos, bool allow_show);

// tracks ---------------------------------------------------------------------
int (*GetNumTracks)();
MediaTrack* (*GetTrack)(ReaProject* proj, int track_index);

// fx -------------------------------------------------------------------------
int (*TrackFX_GetCount)(MediaTrack* track);
bool (*TrackFX_GetFXName)(MediaTrack* track, int fx, char* buf, int sz);

// parameters -----------------------------------------------------------------
int (*TrackFX_GetNumParams)(MediaTrack* track, int fx);
double (*TrackFX_GetParam)(MediaTrack* track, int fx, int param, double* min_val_out, double* max_val_out);

bool (*TrackFX_GetParameterStepSizes)(MediaTrack* track, int fx, int param, double* step_out,
                                      double* small_step_out, double* large_step_out,
                                      bool* is_toggle_out);

bool (*TrackFX_GetParamName)(MediaTrack* track, int fx, int param, char* buf, int sz);
bool (*TrackFX_SetParam)(MediaTrack* track, int fx, int param, double val);

using namespace ossia::net;
using namespace ossia::oscquery;
using namespace std;

class OssiaControlSurface : public IReaperControlSurface
{
    public:
    OssiaControlSurface(uint32_t oscport = 1234, uint32_t wsport = 5678)
    {
        // create oscquery device
        m_device = make_unique<generic_device>(
                    make_unique<oscquery_server_protocol>(
                        oscport, wsport ), "Reaper");

        // --------------------- parse open project

        uint16_t ntracks        = GetNumTracks();
        char prjname            ; GetProjectName ( 0, &prjname, 1024 );

        auto prjnode            = &create_node(*m_device, &prjname);

        for ( uint16_t t = 0; t < ntracks; ++t )
        {

        }
    }

    ~OssiaControlSurface()                  { m_device.reset(); }
    virtual void CloseNoReset()             { m_device.reset(); }
    virtual const char* GetTypeString()     { return "OSSIA"; }
    virtual const char* GetDescString()     { return "OSSIA OSCQuery device"; }
    virtual const char* GetConfigString()   { return "??"; }

    virtual void SetTrackListChange()
    {

    }

    virtual void SetSurfaceVolume(MediaTrack *trackid, double volume)
    {

    }

    virtual void SetSurfacePan(MediaTrack *trackid, double pan)
    {

    }

    virtual void SetSurfaceMute(MediaTrack *trackid, bool mute)
    {

    }

    virtual void SetSurfaceSelected(MediaTrack *trackid, bool selected)
    {

    }

    virtual void SetSurfaceSolo(MediaTrack *trackid, bool solo)
    {

    }

    virtual void SetSurfaceRecArm(MediaTrack *trackid, bool recarm)
    {

    }

    virtual void SetPlayState(bool play, bool pause, bool rec)
    {

    }

    virtual void SetRepeatState(bool rep)
    {

    }

    virtual void SetTrackTitle(MediaTrack *trackid, const char *title)
    {

    }

    virtual bool GetTouchState(MediaTrack *trackid, int isPan)
    {

    }

    virtual void SetAutoMode(int mode)
    {

    }

    virtual void OnTrackSelection(MediaTrack *trackid)
    {

    }

    virtual bool IsKeyDown(int key)
    {

    }

    virtual int Extended(int call, void *parm1, void *parm2, void *parm3)
    {
        switch(call)
        {
        case CSURF_EXT_RESET:  // clear ossia device
        {
            break;
        }
        case CSURF_EXT_SETAUTORECARM:
        {
            break;
        }
        case CSURF_EXT_SETBPMANDPLAYRATE:
        {
            break;
        }
        case CSURF_EXT_SETFOCUSEDFX:
        {
            break;
        }
        case CSURF_EXT_SETFXCHANGE:
        {
            break;
        }
        case CSURF_EXT_SETFXENABLED:
        {
            break;
        }
        case CSURF_EXT_SETFXOPEN:
        {
            break;
        }
        case CSURF_EXT_SETFXPARAM:
        {
            break;
        }
        case CSURF_EXT_SETINPUTMONITOR:
        {
            break;
        }
        case CSURF_EXT_SETLASTTOUCHEDFX:
        {
            break;
        }
        case CSURF_EXT_SETLASTTOUCHEDTRACK:
        {
            break;
        }
        case CSURF_EXT_SETMETRONOME:
        {
            break;
        }
        case CSURF_EXT_SETMIXERSCROLL:
        {
            break;
        }
        case CSURF_EXT_SETPAN_EX:
        {
            break;
        }
        case CSURF_EXT_SETRECMODE:
        {
            break;
        }
        case CSURF_EXT_SETRECVPAN:
        {
            break;
        }
        case CSURF_EXT_SETRECVVOLUME:
        {
            break;
        }
        case CSURF_EXT_SETSENDPAN:
        {
            break;
        }
        case CSURF_EXT_SETSENDVOLUME:
        {
            break;
        }
        }
    }

    private:
    unique_ptr<generic_device> m_device;
};

static WDL_DLGRET dlg_proc(HWND dlg, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    return 0;
}

static IReaperControlSurface* create_surface(const char* type_string, const char* config_string, int* errStats)
{
    uint32_t ports[2]; // parse ports from config string
    return new OssiaControlSurface();
}

static HWND configure_surface(const char* type_string, HWND parent, const char* config_string)
{
    return CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SURFACEEDIT_MCU), parent, dlg_proc, (LPARAM)config_string);
}

reaper_csurf_reg_t csurf_ossia_reg =
{
  "OSSIA",
  "OSSIA OSCQuery device",
  create_surface,
  configure_surface,
};

#define REAPER_FUNCPTR_GET(func) *((void **) & func) = rec->GetFunc(#func)

extern "C" {

    REAPER_PLUGIN_DLL_EXPORT int
    REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance,
                             reaper_plugin_info_t* rec)
    {
        g_hInst = hInstance;
        if ( !rec  || rec->caller_version != REAPER_PLUGIN_VERSION ||
             !rec->GetFunc ) return 0;

        REAPER_FUNCPTR_GET ( GetProjectName );
        REAPER_FUNCPTR_GET ( GetMainHwnd );
        REAPER_FUNCPTR_GET ( DockWindowAdd );
        REAPER_FUNCPTR_GET ( GetNumTracks );
        REAPER_FUNCPTR_GET ( GetTrack );
        REAPER_FUNCPTR_GET ( TrackFX_GetCount );
        REAPER_FUNCPTR_GET ( TrackFX_GetFXName );
        REAPER_FUNCPTR_GET ( TrackFX_GetNumParams );
        REAPER_FUNCPTR_GET ( TrackFX_GetParam );
        REAPER_FUNCPTR_GET ( TrackFX_GetParameterStepSizes );
        REAPER_FUNCPTR_GET ( TrackFX_GetParamName );
        REAPER_FUNCPTR_GET ( TrackFX_SetParam );

        if ( !GetMainHwnd ||
             !DockWindowAdd ||
             !GetNumTracks ||
             !GetTrack ||
             !TrackFX_GetCount ||
             !TrackFX_GetFXName ||
             !TrackFX_GetNumParams ||
             !TrackFX_GetParam ||
             !TrackFX_GetParameterStepSizes ||
             !TrackFX_GetParamName ||
             !TrackFX_SetParam ) std::cerr << "error!!!!" << std::endl;

        rec->Register("csurf", &csurf_ossia_reg);

        return 0;
    }
}
