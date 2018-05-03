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
bool ( *CSurf_OnRecArmChange )  ( MediaTrack* trackid, int recarm );
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
void (*CSurf_SetSurfaceRecArm)  ( MediaTrack *trackid, bool recarm, IReaperControlSurface* ignoresurf );

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

//-------------------------------------------------------------------------------------------------
// INLINES_UTILITIES
//-------------------------------------------------------------------------------------------------
using namespace ossia::reaper;

inline const std::string control_surface::get_track_name(MediaTrack& track)
{
    char trackname [ 1024 ];

    GetTrackName        ( &track, trackname, 1024 );
    string trackstr     = trackname;
    std::replace        ( trackstr.begin(), trackstr.end(), ' ', '_' );

    return trackstr;
}

inline const std::string control_surface::get_fx_name(MediaTrack& track, int fx)
{
    char fxname [ 1024 ];

    TrackFX_GetFXName   ( &track, fx, fxname, 1024 );
    string fxstr        = fxname;
    std::replace        ( fxstr.begin(), fxstr.end(), ' ', '_');

    return fxstr;
}

inline const std::string control_surface::get_parameter_name(
        MediaTrack& track, int fx, int param )
{
    char param_name [ 1024 ];

    TrackFX_GetParamName    ( &track, fx, param, param_name, 1024 );
    string paramstr         = param_name;
    std::replace            ( paramstr.begin(), paramstr.end(), ' ', '_' );

    return paramstr;
}

//-------------------------------------------------------------------------------------------------
// CONTROL_SURFACE_NON_INTERFACE
//-------------------------------------------------------------------------------------------------

using namespace ossia::net;
using namespace ossia::oscquery;
using namespace std;

ossia::reaper::control_surface::control_surface(uint32_t osc_port, uint32_t ws_port)
    : m_project_path ("/")
{
    // TODO: make menu to choose protocol + ports
    // create oscquery device
    m_device = make_unique<generic_device>(
                make_unique<oscquery_server_protocol>(
                    osc_port, ws_port ), "Reaper");

    //m_midi_out = CreateThreadedMIDIOutput(CreateMIDIOutput(0, false, NULL));

    char project_name [ 1024 ];
    GetProjectName ( 0, project_name, 1024 );

    m_project_path += project_name;

    expose_project_transport();
    // no need to call 'SetTrackListChange',
    // as it is already done by reaper after csurf constructor
}

ossia::reaper::control_surface::~control_surface()  { m_device.reset(); }
void ossia::reaper::control_surface::CloseNoReset() { m_device.reset(); }

inline std::string control_surface::get_project_path() const
{
    return m_project_path;
}

inline std::string control_surface::get_tracks_root_path() const
{
    return m_project_path + "/tracks";
}

inline parameter_base& control_surface::make_parameter(std::string name, ossia::val_type ty)
{
    auto& node = ossia::net::create_node(*m_device.get(), name);
    return *node.create_parameter(ty);
}

inline parameter_base* control_surface::get_parameter(const std::string& path)
{
    auto node = ossia::net::find_node(*m_device.get(), path);
    if ( node ) return node->get_parameter();
    else return 0;
}

inline node_base* control_surface::get_node(const string &path)
{
    return ossia::net::find_node(*m_device.get(), path);
}

inline uint16_t control_surface::get_num_tracks()
{
    return m_tracks.size();
}

inline ossia::reaper::track_hdl* control_surface::get_ossia_track(MediaTrack* mtrack)
{
    for ( const auto& track : m_tracks )
        if ( *track == *mtrack )
            return track;

    return 0;
}

inline ossia::reaper::track_hdl* control_surface::get_ossia_track(string& track_name )
{
    for ( const auto& track : m_tracks )
        if ( track->m_name == track_name )
            return track;

    return 0;
}

//-------------------------------------------------------------------------------------------------
// FX
//-------------------------------------------------------------------------------------------------
ossia::reaper::fx_hdl::fx_hdl(track_hdl &parent, string& name, uint16_t index) :
    m_name(name), m_parent(parent), m_index(index)
{
    // expose fxparameters
    uint16_t nparams = TrackFX_GetNumParams(m_parent.m_track, index);

    for ( uint8_t i = 0; i < nparams; ++i )
    {
        auto pname = control_surface::get_parameter_name(*m_parent.m_track, index, i);
        m_parent.csurf.make_parameter(get_path()+"/"+pname, ossia::val_type::FLOAT);
    }

}

ossia::reaper::fx_hdl::~fx_hdl()
{
    auto fx_root_node = m_parent.csurf.get_node(m_parent.get_fx_root_path());
    fx_root_node->remove_child(m_name);
}

std::string ossia::reaper::fx_hdl::get_path() const
{
    return m_parent.get_fx_root_path() + "/" + m_name;
}


bool ossia::reaper::fx_hdl::alive() const
{
    uint16_t nfx = TrackFX_GetCount(m_parent.m_track);
    for ( uint16_t i = 0; i < nfx; ++i )
    {
        auto fxn = control_surface::get_fx_name(*m_parent.m_track, i);
        if ( fxn == m_name )
            return true;
    }

    return false;
}

void ossia::reaper::fx_hdl::update_parameter_value(string &name, float value)
{
    auto& parameter = *m_parent.csurf.get_parameter(get_path()+"/"+name);
    auto domain = parameter.get_domain();

    // unnormalize value
    value *= domain.get_max<float>();
    value += domain.get_min<float>();

    parameter.set_value_quiet(value);
    parameter.get_node().get_device().get_protocol().push(parameter);
}

//-------------------------------------------------------------------------------------------------
// TRACK
//-------------------------------------------------------------------------------------------------

#define SET_COMMON_CALLBACK(p,func)                             \
    p.add_callback([=](const ossia::value& v) {                 \
        MediaTrack* tr = CSurf_TrackFromID(m_index, true);      \
        func                                                    \
    });

#define SET_COMMON_FLOAT_CALLBACK(p, setter, update)                                \
    SET_COMMON_CALLBACK(p, setter(tr, update(tr, v.get<float>(), false), NULL);)

#define SET_COMMON_BOOL_CALLBACK(p, setter, update)                                 \
    SET_COMMON_CALLBACK(p, setter(tr, update(tr, v.get<bool>()), NULL);)

#define for_each_reaper_track(var) \
    for ( int var = 0; var < GetNumTracks()+1; ++var )

//-------------------------------------------------------------------------------------------------

ossia::reaper::track_hdl::track_hdl(control_surface &parent, MediaTrack* tr)
    : csurf(parent), m_track(tr), m_path(parent.get_tracks_root_path())
{
    m_index   = CSurf_TrackToID(tr, false);
    m_path    += "/";
    m_path    += csurf.get_track_name(*tr);

    std::cout << m_path << std::endl;

    auto& level    = csurf.make_parameter(m_path + "/common/level", ossia::val_type::FLOAT );
    auto& pan      = csurf.make_parameter(m_path + "/common/pan", ossia::val_type::FLOAT );
    auto& mute     = csurf.make_parameter(m_path + "/common/mute", ossia::val_type::BOOL );
    auto& solo     = csurf.make_parameter(m_path + "/common/solo", ossia::val_type::BOOL );
    auto& recarm   = csurf.make_parameter(m_path + "/common/recarm", ossia::val_type::BOOL );

    SET_COMMON_FLOAT_CALLBACK   ( level, CSurf_SetSurfaceVolume, CSurf_OnVolumeChange );
    SET_COMMON_FLOAT_CALLBACK   ( pan, CSurf_SetSurfacePan, CSurf_OnPanChange );
    SET_COMMON_BOOL_CALLBACK    ( mute, CSurf_SetSurfaceMute, CSurf_OnMuteChange );
    SET_COMMON_BOOL_CALLBACK    ( solo, CSurf_SetSurfaceSolo, CSurf_OnSoloChange );
    SET_COMMON_BOOL_CALLBACK    ( recarm, CSurf_SetSurfaceRecArm, CSurf_OnRecArmChange );

    resolve_fxs();
}

ossia::reaper::track_hdl::~track_hdl()
{
    // unexpose track
    auto& track_node = *ossia::net::find_node(*csurf.m_device.get(), csurf.get_tracks_root_path());
    track_node.remove_child(m_name);

    for ( const auto& fx : m_fxs )
        delete fx;
}

template<typename T>
void ossia::reaper::track_hdl::update_common_ossia_parameter(std::string pname, const T& value)
{
    std::cout << get_path() + "/common/" + pname << std::endl;

    if ( ! csurf.get_parameter(get_path()+"/common/"+pname ))
        std::cout << "could not find parameter\n";

    auto& parameter = *csurf.get_parameter(get_path()+"/common/"+pname);
    parameter.set_value_quiet(value);
    parameter.get_node().get_device().get_protocol().push(parameter);
}

fx_hdl *ossia::reaper::track_hdl::get_fx(std::string& name)
{
    for ( const auto& fx : m_fxs )
    {
        if ( fx->m_name == name )
            return fx;
    }

    return 0;
}

void ossia::reaper::track_hdl::resolve_fxs()
{
    uint16_t ossia_num_fx = m_fxs.size();
    uint16_t reaper_num_fx = TrackFX_GetCount(m_track);

    if ( reaper_num_fx > ossia_num_fx )
        resolve_added_fxs();

    else if ( reaper_num_fx < ossia_num_fx )
        resolve_missing_fxs();
}

void ossia::reaper::track_hdl::resolve_added_fxs()
{
    uint16_t nfx = TrackFX_GetCount(m_track);

    for ( uint16_t i = 0; i < nfx; ++ i )
    {
        auto fxn = control_surface::get_fx_name(*m_track, i);
        if ( !get_fx(fxn) ) m_fxs.push_back(new fx_hdl(*this, fxn, i));
    }
}

void ossia::reaper::track_hdl::resolve_missing_fxs()
{
    for ( const auto& fx : m_fxs )
    {
        if ( !fx->alive() )
        {
            delete fx;
            m_fxs.erase(std::remove(m_fxs.begin(), m_fxs.end(), fx), m_fxs.end());
        }
    }
}

bool ossia::reaper::track_hdl::alive() const
{
    for_each_reaper_track ( i )
    {
        auto& mtrack = *CSurf_TrackFromID(i,false);
        if ( *this == mtrack )
            return true;
    }

    return false;
}

inline void ossia::reaper::track_hdl::resolve_index()
{
    m_index = CSurf_TrackToID(m_track, false);
}

inline std::string ossia::reaper::track_hdl::get_path() const
{
    return m_path;
}

inline std::string ossia::reaper::track_hdl::get_fx_root_path() const
{
    return get_path() + "/fx";
}

//-------------------------------------------------------------------------------------------------
// EXPOSE
//-------------------------------------------------------------------------------------------------

void ossia::reaper::control_surface::expose_project_transport()
{
    auto& play = make_parameter(m_project_path+"/transport/play", ossia::val_type::IMPULSE);
    auto& stop = make_parameter(m_project_path+"/transport/stop", ossia::val_type::IMPULSE);

    play.add_callback([&](const ossia::value& v) { CSurf_OnPlay(); });
    stop.add_callback([&](const ossia::value& v) { CSurf_OnStop(); });

    // TODO: add rewind, scrub, etc.
    // TODO: add marker & item navigaton
}

//-----------------------------------------------------------------------------------------------------
// REAPER_TRACK_UPDATES
//-----------------------------------------------------------------------------------------------------

std::vector<MediaTrack*> ossia::reaper::control_surface::resolve_added_tracks()
{
    // GetNumTracks is not including the Master track
    std::vector<MediaTrack*> added_tracks;

    for_each_reaper_track( i )
    {
        auto mtrack = CSurf_TrackFromID(i, false);
        if ( !get_ossia_track(mtrack) )
            added_tracks.push_back(mtrack);
    }

    return added_tracks;
}

std::vector<track_hdl*> ossia::reaper::control_surface::resolve_missing_tracks()
{
    std::vector<track_hdl*> missing_tracks;

    for ( const auto& otrack : m_tracks )
    {
        if ( !otrack->alive() )
        {
            delete otrack;
            missing_tracks.push_back(otrack);
        }
    }

    return missing_tracks;
}

inline void ossia::reaper::control_surface::resolve_track_indexes()
{
    for ( const auto& otrack : m_tracks )
        otrack->resolve_index();
}

//-----------------------------------------------------------------------------------------------------
// INTERFACES
//-----------------------------------------------------------------------------------------------------

void ossia::reaper::control_surface::SetTrackListChange()
{
    // compare number of tracks on each side
    uint16_t n_reaper_tracks    = GetNumTracks()+1;
    uint16_t n_ossia_tracks     = get_num_tracks();

    if ( n_reaper_tracks > n_ossia_tracks )
    {
        // track or tracks have been added
        for ( const auto& added_track : resolve_added_tracks() )
            m_tracks.push_back ( new track_hdl(*this, added_track));
    }

    else if ( n_reaper_tracks < n_ossia_tracks )
    {
        // track or tracks have been removed
        for ( const auto& missing_track : resolve_missing_tracks() )
            m_tracks.erase(std::remove(m_tracks.begin(), m_tracks.end(), missing_track), m_tracks.end());
    }
    else
    {
        // track or tracks have been moved
        resolve_track_indexes();
    }
}

void ossia::reaper::control_surface::Run() {}

void ossia::reaper::control_surface::SetSurfaceVolume(MediaTrack *trackid, double volume)
{
    // amp scale: 0 = -inf, 4 = 12dB
    if ( auto track = get_ossia_track(trackid) )
        track->update_common_ossia_parameter<double>("level", volume);
}

void ossia::reaper::control_surface::SetSurfacePan(MediaTrack *trackid, double pan)
{
    // -1.0 to 1.0
    if ( auto track = get_ossia_track(trackid) )
        track->update_common_ossia_parameter<double>("pan", pan);
}

void ossia::reaper::control_surface::SetSurfaceSolo(MediaTrack *trackid, bool solo)
{
    if ( auto track = get_ossia_track(trackid) )
        track->update_common_ossia_parameter<bool>("solo", solo);
}

void ossia::reaper::control_surface::SetSurfaceMute(MediaTrack *trackid, bool mute)
{
    if ( auto track = get_ossia_track(trackid) )
        track->update_common_ossia_parameter<double>("mute", mute);
}

void ossia::reaper::control_surface::SetSurfaceSelected(MediaTrack *trackid, bool selected)
{

}


void ossia::reaper::control_surface::SetSurfaceRecArm(MediaTrack *trackid, bool recarm)
{
    if ( auto track = get_ossia_track(trackid) )
        track->update_common_ossia_parameter<bool>("recarm", recarm);
}

void ossia::reaper::control_surface::SetPlayState(bool play, bool pause, bool rec)
{

}

void ossia::reaper::control_surface::SetRepeatState(bool rep)
{

}

void ossia::reaper::control_surface::SetTrackTitle(MediaTrack *trackid, const char *title)
{
    auto track = get_ossia_track(trackid);
    if ( track ) track->m_name = title;
}

bool ossia::reaper::control_surface::GetTouchState(MediaTrack *trackid, int isPan)
{

}

void ossia::reaper::control_surface::SetAutoMode(int mode)
{

}

void ossia::reaper::control_surface::OnTrackSelection(MediaTrack *trackid)
{

}

bool ossia::reaper::control_surface::IsKeyDown(int key)
{

}

int ossia::reaper::control_surface::Extended(int call, void *parm1, void *parm2, void *parm3)
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
        auto otrack = get_ossia_track((MediaTrack*) parm1);
        otrack->resolve_fxs();
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

        MediaTrack* track   = (MediaTrack*) parm1;
        double value        = *(double*) parm3;
        int packed          = *(int*)parm2;

        auto track_name     = get_track_name        ( *track );
        auto fx_name        = get_fx_name           ( *track, packed>>16 );
        auto param_name     = get_parameter_name    ( *track, packed>>16, packed&15 );

        auto fx = get_ossia_track(track)->get_fx(fx_name);
        fx->update_parameter_value(param_name, value);

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
    return new ossia::reaper::control_surface();
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
    REAPER_FUNCPTR_GET ( CSurf_OnRecArmChange );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfaceSolo );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfaceMute );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfacePan );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfaceVolume );
    REAPER_FUNCPTR_GET ( CSurf_SetSurfaceRecArm );
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
