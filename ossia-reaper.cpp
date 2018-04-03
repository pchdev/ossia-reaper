#include <csurf.h>
#include "ossia-reaper.hpp"

#include <ossia/network/domain/domain_functions.hpp>

// GLOBALS --------------------------------------------------------------------
REAPER_PLUGIN_HINSTANCE g_hInst;
HWND g_parent;

// FUNCTION POINTERS --------------------------------------------------------------

// -------------------------------------------------------------------------------- PROJECTS
void ( *GetProjectName )        ( ReaProject* project, char* buf, int buf_sz );

//--------------------------------------------------------------------------------- WINDOWS
HWND ( *GetMainHwnd )           ( );
void ( *DockWindowAdd )         ( HWND hwnd, char* name, int pos, bool allow_show );

// -------------------------------------------------------------------------------- TRACKS
int ( *GetNumTracks )           ( );
MediaTrack* ( *GetTrack )       ( ReaProject* proj, int track_index );
bool ( *GetTrackName )          ( MediaTrack* track, char* buf, int buf_sz );

bool ( *CSurf_OnMuteChange )    ( MediaTrack *trackid, int mute );
bool ( *CSurf_OnSoloChange )    ( MediaTrack *trackid, int solo );
bool ( *CSurf_OnFXChange )      ( MediaTrack *trackid, int en );
void ( *CSurf_OnPlay )          ( );
void ( *CSurf_OnStop )          ( );
void ( *CSurf_GoStart )         ( );
void ( *CSurf_GoEnd )           ( );

double (*CSurf_OnVolumeChange)  ( MediaTrack *trackid, double volume, bool relative );
double (*CSurf_OnPanChange)     ( MediaTrack *trackid, double pan, bool relative );

void (*CSurf_SetSurfaceSolo)    ( MediaTrack *trackid, bool solo, IReaperControlSurface *ignoresurf );
void (*CSurf_SetSurfaceMute)    ( MediaTrack *trackid, bool mute, IReaperControlSurface *ignoresurf );
void (*CSurf_SetSurfaceVolume)  ( MediaTrack *trackid, double volume, IReaperControlSurface *ignoresurf );
void (*CSurf_SetSurfacePan)     ( MediaTrack *trackid, double pan, IReaperControlSurface *ignoresurf );

MediaTrack*(*CSurf_TrackFromID) ( int idx, bool mcpView );
int (*CSurf_TrackToID)          ( MediaTrack *track, bool mcpView );

//-------------------------------------------------------------------------------- FX
int ( *TrackFX_GetCount )       ( MediaTrack* track );
bool ( *TrackFX_GetFXName )     ( MediaTrack* track, int fx, char* buf, int sz );

// ------------------------------------------------------------------------------- PARAMETERS

int ( *TrackFX_GetNumParams )   ( MediaTrack* track, int fx );
double ( *TrackFX_GetParam )    ( MediaTrack* track, int fx, int param, double* min_val_out, double* max_val_out ) ;
bool ( *TrackFX_GetParamName )  ( MediaTrack* track, int fx, int param, char* buf, int sz );
bool ( *TrackFX_SetParam )      ( MediaTrack* track, int fx, int param, double val);

bool ( *TrackFX_SetParamNormalized )    ( MediaTrack* track, int fx, int param, double val);
bool (*TrackFX_GetParameterStepSizes)   ( MediaTrack* track, int fx, int param, double* step_out,
                                          double* small_step_out, double* large_step_out,
                                          bool* is_toggle_out );

bool (*TrackFX_GetEnabled )     ( MediaTrack* track, int fx );
bool (*TrackFX_SetEnabled )     ( MediaTrack* track, int fx, bool enabled );

//-------------------------------------------------------------------------------

using namespace ossia::net;
using namespace ossia::oscquery;
using namespace std;

// the aim is to make a dockwindow interface, and select parameters
// that we want to expose, with the possibility of a custom address

OssiaControlSurface::OssiaControlSurface(uint32_t osc_port, uint32_t ws_port)
{
    // TODO: make menu to choose protocol + ports
    // create oscquery device
    m_device = make_unique<generic_device>(
                make_unique<oscquery_server_protocol>(
                    osc_port, ws_port ), "Reaper");

    //m_midi_out = CreateThreadedMIDIOutput(CreateMIDIOutput(0, false, NULL));

    create_rootnode();
}

OssiaControlSurface::~OssiaControlSurface()         { m_device.reset(); }
void OssiaControlSurface::CloseNoReset()            { m_device.reset(); }

void OssiaControlSurface::create_rootnode() //------------------------------------------ ROOT_NODE
{
    char prjname            ; GetProjectName ( 0, &prjname, 1024 );
    string prjstr           = &prjname;

    if  ( prjstr == "" )
        prjstr = "untitled project";

    m_project_node = &create_node(*m_device, prjstr);

    auto transport_n = m_project_node->create_child("transport");
    m_tracks_n = m_project_node->create_child("tracks");

    auto play_n = transport_n->create_child("play");
    auto stop_n = transport_n->create_child("stop");

    auto play_p = play_n->create_parameter(ossia::val_type::IMPULSE);
    auto stop_p = stop_n->create_parameter(ossia::val_type::IMPULSE);

    play_p->add_callback([&](const ossia::value& v) { CSurf_OnPlay(); });
    stop_p->add_callback([&](const ossia::value& v) { CSurf_OnStop(); });

    // TODO: create master track

}

inline const std::string get_track_name(MediaTrack* track, bool formated = false )
{
    char trackname [ 1024 ];

    GetTrackName        ( track, trackname, 1024 );
    string trackstr     = trackname;
    std::replace        ( trackstr.begin(), trackstr.end(), ' ', '_' );

    return trackstr;
}

inline std::string get_fx_name(MediaTrack* track, int fx, bool formated = false )
{
    char fxname [ 1024 ];

    TrackFX_GetFXName   ( track, fx, fxname, 1024 );
    string fxstr        = fxname;

    if ( formated ) std::replace ( fxstr.begin(), fxstr.end(), ' ', '_');

    return fxstr;
}

inline std::string get_parameter_name(MediaTrack* track, int fx, int param, bool formated = false)
{
    char param_name [ 1024 ];

    TrackFX_GetParamName    ( track, fx, param, param_name, 1024 );
    string paramstr         = param_name;

    if ( formated ) std::replace ( paramstr.begin(), paramstr.end(), ' ', '_' );

    return paramstr;
}

inline parameter_base* make_parameter(node_base* node, std::string name, ossia::val_type ty)
{
    if ( !node ) return 0;

    auto n = node->create_child(name);
    auto p = n->create_parameter(ty);

    return p;
}

template<typename T>
void OssiaControlSurface::update_track_parameter ( MediaTrack* track, ossia::string_view name, T const& value )
{
    int tr = CSurf_TrackToID ( track, true );
    if ( tr == 0 || !m_tracks_n ) return;

    auto track_str          = get_track_name(track, true);
    auto track_n            = m_tracks_n->find_child(track_str);
    node_base* target_n     = 0;

    if   ( track_n ) target_n  = track_n->find_child(name);
    else return;

    if ( target_n )
    {
        auto target_p   = target_n->get_parameter();
        target_p        ->set_value_quiet(value);
        target_p        ->get_node().get_device().get_protocol().push(*target_p);
    }
}

// INTERFACES -----------------------------------------------------------------------------

void OssiaControlSurface::SetTrackListChange()
{
    // not sure comparing changes would go faster, to be tested...
    m_tracks_n  -> clear_children();
    uint16_t ntracks = GetNumTracks();

    for ( uint16_t t = 0; t < ntracks; ++t )
    {
        // note that Master Track is always track 0,
        // but is not part of the GetNumTracks result
        MediaTrack* track   = GetTrack(0,t);
        auto trackstr       = get_track_name(track, true);

        auto tracknode = m_tracks_n->find_child(trackstr);
        if (!tracknode) tracknode = m_tracks_n->create_child(trackstr);

        // constant parameters ------------------------

        auto level_p    = make_parameter( tracknode, "level", ossia::val_type::FLOAT );
        auto pan_p      = make_parameter( tracknode, "pan", ossia::val_type::FLOAT );
        auto mute_p     = make_parameter( tracknode, "mute", ossia::val_type::BOOL );
        auto solo_p     = make_parameter( tracknode, "solo", ossia::val_type::BOOL );

        // bounding ------------------------------------

        // callbacks -----------------------------------
        level_p->add_callback([=](const ossia::value& v) {
            MediaTrack* tr = CSurf_TrackFromID(t+1, true);
            CSurf_SetSurfaceVolume(tr,
                CSurf_OnVolumeChange(tr, v.get<float>(), false),
                NULL );
        });

        pan_p->add_callback([=](const ossia::value& v) {
            MediaTrack* tr = CSurf_TrackFromID(t+1, true);
            CSurf_SetSurfacePan(tr, CSurf_OnPanChange(tr, v.get<float>(), false), NULL);
        });

        mute_p->add_callback([=](const ossia::value& v) {
            MediaTrack* tr = CSurf_TrackFromID(t+1, true);
            CSurf_SetSurfaceMute(tr, CSurf_OnMuteChange(tr, v.get<bool>()), NULL);
        });

        solo_p->add_callback([=](const ossia::value& v) {
            MediaTrack* tr = CSurf_TrackFromID(t+1, true);
            CSurf_SetSurfaceSolo(tr, CSurf_OnSoloChange(tr, v.get<bool>()), NULL);
        });

        tracknode->create_child("fx");
        parse_track_fx(track);
    }
}

void OssiaControlSurface::parse_track_fx(MediaTrack *track)
{
    auto track_id        = CSurf_TrackToID(track, false);
    auto track_str       = get_track_name(track, true);

    auto track_node      = m_tracks_n->find_child(track_str);
    auto fxn             = track_node->find_child("fx");

    fxn                 ->clear_children( );

    for ( uint8_t i = 0; i <  TrackFX_GetCount(track); ++i )
    {
        auto fx_name = get_fx_name ( track, i, true );
        auto fx_node = fxn->create_child ( fx_name );

        // enabled --------------------------------------------------------------------

        auto enabled_n = fx_node->create_child("enabled");
        auto enabled_p = enabled_n->create_parameter(ossia::val_type::BOOL);

        bool enabled = TrackFX_GetEnabled(track, i);
        enabled_p->set_value ( enabled );

        enabled_p->add_callback([=](const ossia::value& v) {
            MediaTrack* tr = CSurf_TrackFromID(track_id, true);
           TrackFX_SetEnabled(tr, i, v.get<bool>());
        });

        auto params_node = fx_node->create_child("parameters");

        // scan for parameters --------------------------------------------------------

        for ( uint8_t p = 0; p < TrackFX_GetNumParams(track, i); ++p )
        {
            auto param_name = get_parameter_name(track, i, p, true);
            auto param_node = params_node->create_child(param_name);
            auto param_p    = param_node->create_parameter ( ossia::val_type::FLOAT );

            double param_min, param_max;
            auto param_value = TrackFX_GetParam(track, i, p, &param_min, &param_max);

            auto domain = ossia::make_domain(param_min, param_max);
            ossia::net::set_domain(*param_node, domain);

            param_p->set_value(param_value);

            param_p->add_callback([=](const ossia::value& v) {
                MediaTrack* tr = CSurf_TrackFromID(track_id, true);
                TrackFX_SetParam(tr, i, p, v.get<float>());
            });
        }
    }
}

void OssiaControlSurface::Run() {}

void OssiaControlSurface::SetSurfaceVolume(MediaTrack *trackid, double volume)
{
    // amp scale: 0 = -inf, 4 = 12dB
    update_track_parameter<double>(trackid, "level", volume);
}

void OssiaControlSurface::SetSurfacePan(MediaTrack *trackid, double pan)
{
    // -1.0 to 1.0
    update_track_parameter<double>(trackid, "pan", pan);
}

void OssiaControlSurface::SetSurfaceSolo(MediaTrack *trackid, bool solo)
{
    update_track_parameter<bool>(trackid, "solo", solo);
}

void OssiaControlSurface::SetSurfaceMute(MediaTrack *trackid, bool mute)
{
    update_track_parameter<bool>(trackid, "mute", mute);
}

void OssiaControlSurface::SetSurfaceSelected(MediaTrack *trackid, bool selected)
{

}


void OssiaControlSurface::SetSurfaceRecArm(MediaTrack *trackid, bool recarm)
{

}

void OssiaControlSurface::SetPlayState(bool play, bool pause, bool rec)
{

}

void OssiaControlSurface::SetRepeatState(bool rep)
{

}

void OssiaControlSurface::SetTrackTitle(MediaTrack *trackid, const char *title)
{
    SetTrackListChange();
}

bool OssiaControlSurface::GetTouchState(MediaTrack *trackid, int isPan)
{

}

void OssiaControlSurface::SetAutoMode(int mode)
{

}

void OssiaControlSurface::OnTrackSelection(MediaTrack *trackid)
{

}

bool OssiaControlSurface::IsKeyDown(int key)
{

}

int OssiaControlSurface::Extended(int call, void *parm1, void *parm2, void *parm3)
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
        MediaTrack* track = ( MediaTrack* ) parm1;
        parse_track_fx ( track );
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
    // parm1=(MediaTrack*)track, parm2=(int*)(fxidx<<16|paramidx), parm3=(double*)normalized value


        // TODO ------------ GET PARAMETERS PER NUMBER INSTEAD OF FIND_NODE

        MediaTrack* track   = (MediaTrack*) parm1;
        double value        = *(double*) parm3;

        int packed          = *(int*)parm2;
        int paramidx        = packed&15;
        int fxidx           = packed>>16;

        auto track_name     = get_track_name        ( track, true );
        auto fx_name        = get_fx_name           ( track, fxidx, true );
        auto param_name     = get_parameter_name    ( track, fxidx, paramidx, true );

        auto track_node     = m_tracks_n->find_child(track_name);
        auto fxdir_node     = track_node->find_child("fx");
        auto fx_node        = fxdir_node->find_child(fx_name);

        auto paramdir_node  = fx_node->find_child("parameters");
        auto param_node     = paramdir_node->find_child(param_name);

        auto domain         = ossia::net::get_domain(*param_node);

        // unnormalize value
        value *= domain.get_max<float>();
        value += domain.get_min<float>();

        auto target_p       = param_node->get_parameter();
        target_p            ->set_value_quiet(value);
        target_p            ->get_node().get_device().get_protocol().push(*target_p);

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
    "OSSIA device",
    create_surface,
    configure_surface,
};

#define REAPER_FUNCPTR_GET(func) *((void **) & func) = (void*) rec->GetFunc(#func)

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
    REAPER_FUNCPTR_GET ( GetTrackName );
    REAPER_FUNCPTR_GET ( TrackFX_GetCount );
    REAPER_FUNCPTR_GET ( TrackFX_GetFXName );
    REAPER_FUNCPTR_GET ( TrackFX_GetNumParams );
    REAPER_FUNCPTR_GET ( TrackFX_GetParam );
    REAPER_FUNCPTR_GET ( TrackFX_GetParameterStepSizes );
    REAPER_FUNCPTR_GET ( TrackFX_GetParamName );
    REAPER_FUNCPTR_GET ( TrackFX_SetParam );
    REAPER_FUNCPTR_GET ( TrackFX_SetParamNormalized );
    REAPER_FUNCPTR_GET ( TrackFX_GetEnabled );
    REAPER_FUNCPTR_GET ( TrackFX_SetEnabled );
    REAPER_FUNCPTR_GET ( CSurf_OnVolumeChange );
    REAPER_FUNCPTR_GET ( CSurf_OnPanChange );
    REAPER_FUNCPTR_GET ( CSurf_OnMuteChange );
    REAPER_FUNCPTR_GET ( CSurf_OnSoloChange );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfaceSolo );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfaceMute );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfacePan );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfaceVolume );
    REAPER_FUNCPTR_GET ( CSurf_TrackFromID );
    REAPER_FUNCPTR_GET ( CSurf_TrackToID );
    REAPER_FUNCPTR_GET ( CSurf_OnPlay );
    REAPER_FUNCPTR_GET ( CSurf_OnStop );

    if ( !GetMainHwnd ||
         !DockWindowAdd ||
         !GetNumTracks ||
         !GetTrack ||
         !GetTrackName ||
         !TrackFX_GetCount ||
         !TrackFX_GetFXName ||
         !TrackFX_GetNumParams ||
         !TrackFX_GetParam ||
         !TrackFX_GetParameterStepSizes ||
         !TrackFX_GetParamName ||
         !TrackFX_SetParam ||
         !CSurf_OnVolumeChange ||
         !CSurf_OnPanChange ||
         !CSurf_OnMuteChange ||
         !CSurf_OnSoloChange ||
         !CSurf_SetSurfaceSolo ||
         !CSurf_SetSurfaceMute ||
         !CSurf_SetSurfaceVolume ||
         !CSurf_SetSurfacePan ||
         !CSurf_TrackFromID ||
         !CSurf_TrackToID ||
         !CSurf_OnPlay ||
         !CSurf_OnStop ) std::cerr << "error!!!!" << std::endl;

    rec->Register("csurf", &csurf_ossia_reg);

    return 0;
}
}

//-------------------------------------------------------------------------------------------------------
// MIDI_OUTPUT
//-------------------------------------------------------------------------------------------------------

threaded_midi_output::threaded_midi_output(midi_Output *out)
{
    DWORD id;

    m_output    = out;
    m_quit      = false;
    m_hthread   = CreateThread( NULL, 0, thread_proc, this, 0, &id );
}

threaded_midi_output::~threaded_midi_output()
{
    if (m_hthread)
    {
        m_quit = true;
        WaitForSingleObject ( m_hthread, INFINITE );
        CloseHandle ( m_hthread );
        m_hthread = 0;
        Sleep(30);
    }

    delete m_output;
    m_empty.Empty(true);
    m_full.Empty(true);
}

void threaded_midi_output::SendMsg(MIDI_event_t *msg, int frame_offset)
{
    if ( !msg ) return;

    WDL_HeapBuf *b = NULL;
    if (m_empty.GetSize())
    {
        m_mutex.Enter();
        b = m_empty.Get(m_empty.GetSize()-1);
        m_empty.Delete(m_empty.GetSize()-1);
        m_mutex.Leave();
    }
    if ( !b && m_empty.GetSize()+m_full.GetSize()<500 )
        b = new WDL_HeapBuf(256);

    if ( b )
    {
        int sz = msg->size;
        if ( sz < 3 ) sz = 3;

        int len = msg->midi_message + sz - (unsigned char*) msg;
        memcpy( b->Resize ( len, false), msg, len);
        m_mutex.Enter();
        m_full.Add(b);
        m_mutex.Leave();
    }
}

void threaded_midi_output::Send(unsigned char status, unsigned char d1, unsigned char d2, int frame_offset)
{
    MIDI_event_t evt = { 0, 3, status, d1, d2 };
    SendMsg ( &evt,frame_offset );
}

DWORD WINAPI threaded_midi_output::thread_proc(LPVOID p)
{
    WDL_HeapBuf* lastbuf = NULL;
    threaded_midi_output* _this = (threaded_midi_output*) p;
    unsigned int scnt=0;
    for (;;)
    {
        if (_this->m_full.GetSize()||lastbuf)
        {
            _this->m_mutex.Enter();
            if (lastbuf) _this->m_empty.Add(lastbuf);
            lastbuf=_this->m_full.Get(0);
            _this->m_full.Delete(0);
            _this->m_mutex.Leave();

            if (lastbuf) _this->m_output->SendMsg((MIDI_event_t*)lastbuf->Get(),-1);
            scnt=0;
        }
        else
        {
            Sleep(1);
            if (_this->m_quit&&scnt++>3) break; //only quit once all messages have been sent
        }
    }
    delete lastbuf;
    return 0;
}


midi_Output *CreateThreadedMIDIOutput(midi_Output *output)
{
    if ( !output ) return output;
    return new threaded_midi_output( output );
}
