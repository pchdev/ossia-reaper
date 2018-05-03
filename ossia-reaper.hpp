#include <csurf.h>
#include <WDL/swell/swell-dlggen.h>
#include <WDL/swell/swell-menugen.h>
#include <WDL/mutex.h>
#include <WDL/ptrlist.h>

#include <ossia/ossia.hpp>
#include <ossia/network/oscquery/oscquery_server.hpp>

#include <string>
#include <vector>
#include <array>

class threaded_midi_output : public midi_Output
{
    public: //--------------------------------------

    threaded_midi_output(midi_Output* out);
    virtual ~threaded_midi_output();

    virtual void SendMsg ( MIDI_event_t* msg, int frame_offset );
    virtual void Send ( unsigned char status, unsigned char d1, unsigned char d2, int frame_offset);
    //  frame_offset can be <0 for "instant" if supported

    static DWORD WINAPI thread_proc(LPVOID p);

    WDL_Mutex       m_mutex;
    HANDLE          m_hthread;
    bool            m_quit;
    midi_Output*    m_output;

    WDL_PtrList<WDL_HeapBuf> m_full, m_empty;

};

midi_Output *CreateThreadedMIDIOutput(midi_Output *output);

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

// OSSIA CONTROL SURFACE ------------------------------------------------------------------------------------------------------

using namespace ossia::net;
using namespace ossia::oscquery;
using namespace std;

namespace ossia     {
namespace reaper    {

class track_hdl;
class control_surface;

class fx_hdl
{
    friend class ossia::reaper::track_hdl;

    public:
    fx_hdl          ( track_hdl& parent, std::string& name, uint16_t index);
    ~fx_hdl         ( );

    void update_parameter_value ( std::string& name, float value );
    bool alive () const;

    protected:
    uint16_t m_index;
    std::string get_path() const;
    std::string m_name;
    track_hdl& m_parent;
};

class track_hdl
{
    friend class ossia::reaper::control_surface;
    friend class ossia::reaper::fx_hdl;

    friend bool operator==(const MediaTrack& lhs, const track_hdl& rhs)
    {
        return &lhs == rhs.m_track;
    }

    friend bool operator==(const track_hdl& lhs, const MediaTrack& rhs)
    {
        return lhs.m_track == &rhs;
    }

    friend bool operator!=(const MediaTrack& lhs, const track_hdl& rhs)
    {
        return &lhs != rhs.m_track;
    }

    friend bool operator!=(const track_hdl& lhs, const MediaTrack& rhs)
    {
        return lhs.m_track != &rhs;
    }

    public: //------------------------------------------------------------------

    track_hdl       ( control_surface& parent, MediaTrack* track );
    ~track_hdl      ( );

    template<typename T>
    void update_common_ossia_parameter  ( std::string name, const T& value );
    void update_title                   ( );

    void resolve_index          ( );
    void resolve_fxs            ( );
    void resolve_added_fxs      ( );
    void resolve_missing_fxs    ( );
    fx_hdl* get_fx              ( std::string& name );

    bool alive                      ( ) const;
    std::string get_path            ( ) const;
    std::string get_fx_root_path    ( ) const;

    private: //---------------------------------------------------------------
    MediaTrack*     m_track;
    std::string     m_name;
    std::string     m_path;
    uint8_t         m_index;

    std::vector<fx_hdl*> m_fxs;
    control_surface& csurf;
};

class control_surface : public IReaperControlSurface
{
    friend class ossia::reaper::track_hdl;

    public:
    control_surface   ( uint32_t osc_port = 1234, uint32_t ws_port = 5678 );
    ~control_surface  ( );

    node_base* get_node                 ( const std::string& path );
    parameter_base* get_parameter       ( const std::string& path );
    parameter_base& make_parameter      ( std::string name, ossia::val_type ty );
    std::string get_project_path        ( ) const;
    std::string get_tracks_root_path    ( ) const;

    static const std::string get_track_name         ( MediaTrack& track );
    static const std::string get_fx_name            ( MediaTrack& track, int fx );
    static const std::string get_parameter_name     ( MediaTrack& track, int fx, int parameter );

    //----------------------------------------------------------------------------------------------
    virtual const char* GetTypeString()     { return "Ossia"; }
    virtual const char* GetDescString()     { return "Ossia"; }
    virtual const char* GetConfigString()   { return "??"; }

    virtual void CloseNoReset() override;
    virtual void Run ( ) override;

    //----------------------------------------------------------------------------------------------

    virtual void SetTrackListChange     ( )                                         override;
    virtual void SetSurfaceVolume       ( MediaTrack *trackid, double volume )      override;
    virtual void SetSurfacePan          ( MediaTrack *trackid, double pan )         override;
    virtual void SetSurfaceSolo         ( MediaTrack *trackid, bool solo )          override;
    virtual void SetSurfaceMute         ( MediaTrack *trackid, bool mute )          override;
    virtual void SetSurfaceSelected     ( MediaTrack *trackid, bool selected )      override;
    virtual void SetSurfaceRecArm       ( MediaTrack *trackid, bool recarm )        override;
    virtual void SetPlayState           ( bool play, bool pause, bool rec )         override;
    virtual void SetRepeatState         ( bool rep )                                override;
    virtual void SetTrackTitle          ( MediaTrack *trackid, const char *title )  override;
    virtual bool GetTouchState          ( MediaTrack *trackid, int isPan )          override;
    virtual void SetAutoMode            ( int mode )                                override;
    virtual void OnTrackSelection       ( MediaTrack *trackid )                     override;
    virtual bool IsKeyDown              ( int key )                                 override;

    virtual int Extended                ( int call, void *parm1, void *parm2, void *parm3 ) override;

    protected: //------------------------------------------------------------------------------------

    void            expose_project_transport     ( );

    uint16_t        get_num_tracks    ( );
    track_hdl*      get_ossia_track   ( MediaTrack* track );
    track_hdl*      get_ossia_track   ( std::string& track );

    void                         resolve_track_indexes      ( );
    void                         resolve_missing_tracks     ( );
    std::vector<MediaTrack*>     resolve_added_tracks       ( );

    private: //-------------------------------------------------------------------------------------

    unique_ptr<generic_device> m_device;
    HWND* m_dockwindow;
    std::string m_project_path;
    midi_Output* m_midi_out;
    std::vector<track_hdl*> m_tracks;

};

bool operator==(const MediaTrack& lhs, const track_hdl& rhs);
bool operator==(const track_hdl& lhs, const MediaTrack& rhs);

bool operator!=(const MediaTrack& lhs, const track_hdl& rhs);
bool operator!=(const track_hdl& lhs, const MediaTrack& rhs);


}
}



