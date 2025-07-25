// rlevideo.cpp
// Extension lib defines
#define LIB_NAME "RLEVideo"
#define MODULE_NAME "rlevideo"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <vector>

typedef struct ColorRGB {
    uint8_t r,g,b;
} ColorRGB;

typedef struct VideoInfo {
    uint32_t    width;
    uint32_t    height;
    uint32_t    fps;
    uint32_t    frames;
    ColorRGB    palette[256];    // Note this can be replaced during decode!
    std::vector<uint32_t>   frame_offsets;
    std::vector<uint8_t>    rle_data;
} VideoInfo;

// A list of videos loaded to play in game
static std::vector<VideoInfo>   g_videos;

static int VideoAdd(lua_State *L)
{
    VideoInfo   info;

    DM_LUA_STACK_CHECK(L, 1);
    const char *pal = luaL_checkstring(L, 1);
    int palsize = luaL_checknumber(L, 2) / 3;
    uint8_t *rletex = (uint8_t *)luaL_checkstring(L, 3);
    uint32_t datasize = luaL_checknumber(L, 4);

    // Copy palette direct into the palette array 
    memset( info.palette, 0, palsize * 3);
    for(int i = 0; i< palsize; i++) {
        info.palette[i].r = (uint8_t)pal[i * 3];
        info.palette[i].g = (uint8_t)pal[i * 3 + 1];
        info.palette[i].b = (uint8_t)pal[i * 3 + 2];
        
        //printf("Palette[%d] = (%d, %d, %d)\n", i, info.palette[i].r, info.palette[i].g, info.palette[i].b);
    }

    // Parse the rletex header. 
    uint32_t *ptr = (uint32_t *) rletex;
    info.width      = *ptr++;
    info.height     = *ptr++;
    info.fps        = *ptr++;
    info.frames     = *ptr++;
    for(int i=0; i<info.frames; i++) {
        info.frame_offsets.push_back(*ptr++);
    }

    datasize -= (16 + info.frames * 4);

    // Copy the rledata in
    uint8_t *rledata = (uint8_t *)ptr;
    while(datasize-- > 0) {
        info.rle_data.push_back(*rledata++);
    }

    // Populate info table for return and add to the video list.
    g_videos.push_back(info);

    // Create a new table
    lua_newtable(L);

    // id
    lua_pushstring(L, "id");
    lua_pushinteger(L, g_videos.size() - 1);
    lua_settable(L, -3);

    // width
    lua_pushstring(L, "width");
    lua_pushinteger(L, info.width);
    lua_settable(L, -3);

    // height
    lua_pushstring(L, "height");
    lua_pushinteger(L, info.height);
    lua_settable(L, -3);

    // fps
    lua_pushstring(L, "fps");
    lua_pushinteger(L, info.fps);
    lua_settable(L, -3);

    // frames
    lua_pushstring(L, "frames");
    lua_pushinteger(L, info.frames);
    lua_settable(L, -3);
    
    // palette size
    lua_pushstring(L, "palsize");
    lua_pushinteger(L, palsize);
    lua_settable(L, -3);

    return 1;
}

static int VideoDecode(lua_State *L)
{
    int vid = luaL_checknumber(L, 1);
    int frameno = luaL_checknumber(L, 2);
    const char *pal = luaL_checkstring(L, 3);
    int palsize = luaL_checknumber(L, 4) / 3;
    dmScript::LuaHBuffer *buffer = dmScript::CheckBuffer(L, 5);

    // Get video info 
    VideoInfo info = g_videos[vid];

    // Get the frame from the video, and decode the full frame to rgb (uising palette lookup)
    // for(int i = 0; i< palsize; i++) {
    //     info.palette[i].r = (uint8_t)pal[i * 3];
    //     info.palette[i].g = (uint8_t)pal[i * 3 + 1];
    //     info.palette[i].b = (uint8_t)pal[i * 3 + 2];
    // }    
    
    int framesize = info.width * info.height;

    uint8_t* stream = 0;
    uint32_t count = 0;
    uint32_t component_size = 0;
    uint32_t stride = 0;

    // Get the stream by name, e.g., "rgba"
    dmBuffer::Result r = dmBuffer::GetStream(buffer->m_Buffer, dmHashString64("rgba"), (void**)&stream, &count, &component_size, &stride);
    if (r != dmBuffer::RESULT_OK)
    {
        return luaL_error(L, "Failed to get buffer stream: %d", r);
    }

    int frameoff = info.frame_offsets[frameno] * 2;
    uint8_t rlerun = info.rle_data[frameoff];
    uint8_t rlepal = info.rle_data[frameoff + 1];
    // Fill data (RGB format — 4 components per pixel)
    for (uint32_t i = 0; i < count; ++i)
    {
        ColorRGB col = info.palette[rlepal];
        uint32_t offset = i * 4;
        stream[offset + 0] = col.r;
        stream[offset + 1] = col.g;  
        stream[offset + 2] = col.b;   
        stream[offset + 3] = 0xff;   

        rlerun--;
        if(rlerun < 1) {
            frameoff += 2;
            rlerun = info.rle_data[frameoff];
            rlepal = info.rle_data[frameoff + 1];
        }
    }

    r = dmBuffer::ValidateBuffer(buffer->m_Buffer);
    return 0;
}

static int VideoClear(lua_State *L)
{
    g_videos.clear();
    return 0;
}


// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"videoadd", VideoAdd},
    {"videodecode", VideoDecode},
    {"videoclear", VideoClear},

    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeRLEVideo(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializeRLEVideo");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeRLEVideo(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeRLEVideo(dmExtension::AppParams* params)
{
    dmLogInfo("AppFinalizeRLEVideo");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeRLEVideo(dmExtension::Params* params)
{
    dmLogInfo("FinalizeRLEVideo");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateRLEVideo(dmExtension::Params* params)
{
    dmLogInfo("OnUpdateRLEVideo");
    return dmExtension::RESULT_OK;
}

static void OnEventRLEVideo(dmExtension::Params* params, const dmExtension::Event* event)
{
    switch(event->m_Event)
    {
        case dmExtension::EVENT_ID_ACTIVATEAPP:
            dmLogInfo("OnEventRLEVideo - EVENT_ID_ACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_DEACTIVATEAPP:
            dmLogInfo("OnEventRLEVideo - EVENT_ID_DEACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_ICONIFYAPP:
            dmLogInfo("OnEventRLEVideo - EVENT_ID_ICONIFYAPP");
            break;
        case dmExtension::EVENT_ID_DEICONIFYAPP:
            dmLogInfo("OnEventRLEVideo - EVENT_ID_DEICONIFYAPP");
            break;
        default:
            dmLogWarning("OnEventRLEVideo - Unknown event id");
            break;
    }
}

// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// RLEVideo is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(RLEVideo, LIB_NAME, AppInitializeRLEVideo, AppFinalizeRLEVideo, InitializeRLEVideo, OnUpdateRLEVideo, OnEventRLEVideo, FinalizeRLEVideo)
