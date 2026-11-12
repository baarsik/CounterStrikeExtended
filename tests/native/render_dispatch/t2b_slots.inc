// Additive exact ABI surface. These slots are supplied by qgl.cpp in the engine.
#define T2B_CORE_SLOTS(X) \
    X(BindTexture) X(DeleteTextures) X(GenTextures) X(PixelStorei) X(TexImage2D) X(TexSubImage2D) \
    X(TexParameteri) X(GenLists) X(NewList) X(EndList) X(CallList) X(DeleteLists) \
    X(EnableClientState) X(TexCoordPointer) X(VertexPointer) X(DrawArrays) X(Scissor) X(PolygonOffset)
#define T2B_DEFINE_SLOT(name) decltype(&gl##name) qgl##name = nullptr;
T2B_CORE_SLOTS(T2B_DEFINE_SLOT)
#undef T2B_DEFINE_SLOT
#define T2B_SIGNATURE(name) \
    static_assert(std::is_same<decltype(&CseRender##name), decltype(&gl##name)>::value, "T2b GL ABI: " #name);
T2B_CORE_SLOTS(T2B_SIGNATURE)
#undef T2B_SIGNATURE
using T2bColorTableProcedure = void (APIENTRY*)(GLenum, GLenum, GLsizei, GLenum, GLenum, const void*);
T2bColorTableProcedure qglColorTableEXT = nullptr;
static_assert(std::is_same<decltype(&CseRenderColorTableEXT), T2bColorTableProcedure>::value,
    "T2b GL ABI: ColorTableEXT");
static_assert(std::is_same<decltype(&CseRenderTexParameterf), decltype(&glTexParameterf)>::value,
    "T2b GL ABI: existing TexParameterf");
