
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten/html5.h>
#include <emscripten/heap.h>
#include <emscripten/em_asm.h>

#include <iostream>
#include <string>
#include <spine/spine.h>
#include <spine/ContainerUtil.h>

void print(const std::string &str) {
    std::cout << str << std::endl;
}

using namespace emscripten;

using namespace spine;

struct VertexDatas
{
    float vertices[2];
    float colors[4];
    float uvs[2];
};

class LayaAttachmentVertices {
public:
	LayaAttachmentVertices (void* page, size_t verticesCount, size_t indexsCount):m_trianglesCount(indexsCount),m_verticesCount(verticesCount),m_page(page) {
        m_vertexDatas = new VertexDatas[verticesCount];
        m_triangles = new unsigned short[indexsCount];
    }
	virtual ~LayaAttachmentVertices (){
        delete[] m_vertexDatas;
        delete[] m_triangles;
    }

	void* m_page = nullptr;
    size_t m_verticesCount;
    size_t m_trianglesCount;
    VertexDatas* m_vertexDatas;
    unsigned short* m_triangles;
};

static void deleteAttachmentVertices (void* vertices) {
	delete (LayaAttachmentVertices *) vertices;
}

static unsigned short quadTriangles[6] = {0, 1, 2, 2, 3, 0};

static void setAttachmentVertices(RegionAttachment* attachment) {
	LayaAttachmentVertices* attachmentVertices = new LayaAttachmentVertices(attachment->getRendererObject(), 4, 6);
    std::memcpy(attachmentVertices->m_triangles, quadTriangles,  6 * sizeof(unsigned short));
	VertexDatas* vertices = attachmentVertices->m_vertexDatas;
	for (int i = 0, ii = 0; i < 4; ++i, ii += 2) {
        vertices[i].uvs[0] = attachment->getUVs()[ii];
        vertices[i].uvs[1] = attachment->getUVs()[ii + 1];
	}
	attachment->setRendererObject(attachmentVertices, deleteAttachmentVertices);
}

static void setAttachmentVertices(MeshAttachment* attachment) {
	LayaAttachmentVertices* attachmentVertices = new LayaAttachmentVertices(attachment->getRendererObject(),
																	attachment->getWorldVerticesLength() >> 1, attachment->getTriangles().size());
    std::memcpy(attachmentVertices->m_triangles, attachment->getTriangles().buffer(),  attachment->getTriangles().size() * sizeof(unsigned short));                                                       
	VertexDatas* vertices = attachmentVertices->m_vertexDatas;
	for (int i = 0, ii = 0, nn = attachment->getWorldVerticesLength(); ii < nn; ++i, ii += 2) {
        vertices[i].uvs[0] = attachment->getUVs()[ii];
        vertices[i].uvs[1] = attachment->getUVs()[ii + 1];
	}
	attachment->setRendererObject(attachmentVertices, deleteAttachmentVertices);
}


template<typename T>
T val_as(const val& v) {
    return v.as<T>();
}

template<typename T>
inline auto createMemoryView(Vector<T> &data) {
    return val(typed_memory_view(data.size(), data.buffer()));
}

template<typename T>
T* wrapPointer(size_t ptr){
    return (T*)ptr;
}

class LayaExtension: public DefaultSpineExtension {
	public:
		LayaExtension(){
            
        }
		
		virtual ~LayaExtension() {
        }
		
	protected:
		virtual char *_readFile(const String &path, int *length) override{
            return nullptr;
        }
};


class LayaAtlasAttachmentLoader: public AtlasAttachmentLoader {
public:
    LayaAtlasAttachmentLoader(Atlas* atlas):AtlasAttachmentLoader(atlas){}
    virtual ~LayaAtlasAttachmentLoader(){}
    void configureAttachment(Attachment* attachment) override{
        if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
            setAttachmentVertices((RegionAttachment*)attachment);
        } else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
            setAttachmentVertices((MeshAttachment*)attachment);
        }
    }
};
spine::SpineExtension* spine::getDefaultExtension() {
	return new LayaExtension();
}

inline val getString(const String& str) {
     if(str.length()<=0){
        return val("");
    }else{
        return val(str.buffer());
    }
}

template<typename T>
val vectorToArray(Vector<T> &vec){
    val new_array = val::array();
    for(int i = 0; i < vec.size(); i++){
        new_array.call<void>("push", vec[i]);
    }
    return new_array;
}
val crateVec2Object(float &x ,float &y){
    val obj = val::object();
    obj.set("x",x);
    obj.set("y",y);
    return obj;
}

inline String createString(const std::string &str) {
    return String(const_cast<char*>(str.c_str()));
}


template<typename T>
Vector<T>* createVector(const std::vector<T> &data) {
    Vector<T> *vec = new Vector<T>();
    for (int i = 0; i < data.size(); i++) {
        vec->add(data.at(i));
    }
    return vec;
}

Atlas* creatAtlas(const std::string data, const std::string dir, TextureLoader* textureLoader, bool createTexture) {
    return new Atlas(data.c_str(), data.length(), dir.c_str(), textureLoader, createTexture);
}


class TextureLoaderWrapper : public wrapper<TextureLoader> {
public:
    EMSCRIPTEN_WRAPPER(TextureLoaderWrapper);

    void load(AtlasPage& page, const String& path) override {
        page.texturePath = path;
        call<void>("load", (size_t)&page, getString(path));
    }

    void unload(void* texture) override {
        call<void>("unload", texture);
    }
};

class AnimationStateListenerObjectWarpper : public wrapper<AnimationStateListenerObject> {
public:
    EMSCRIPTEN_WRAPPER(AnimationStateListenerObjectWarpper);

    void callback(AnimationState* state, EventType type, TrackEntry* entry, Event* event) override {
        if(event == nullptr){
            call<void>("callback", (size_t)&state, type, (size_t)&entry, (size_t)nullptr);
        }
        else{
            call<void>("callback", (size_t)&state, type, (size_t)&entry, (size_t)&event);
        }
    }
};




Color color = Color(1, 1, 1, 1);
Vector<float> vbHelpBuffer = Vector<float>();
Vector<unsigned short> ibHelpBuffer = Vector<unsigned short>();
SkeletonClipping clipper = SkeletonClipping();
    

float *_vbBuffer = nullptr;
unsigned short *_ibBuffer = nullptr;
unsigned short _maxVb = 0;
unsigned short _maxIb = 0;
unsigned short _totalIbCount = 0;
unsigned short _totalVertexCount = 0;


bool createBuffer(size_t maxVb,size_t maxIb){
    if(maxVb<=0||maxIb<=0){
        return false;
    }
    if(_maxVb == maxVb && _maxIb == maxIb){
        return true;
    }
    _maxVb = maxVb;
    _maxIb = maxIb;
    if(_vbBuffer != nullptr){
        SpineExtension::free(_vbBuffer,__FILE__, __LINE__);
    }
    if(_ibBuffer != nullptr){
        SpineExtension::free(_ibBuffer,__FILE__, __LINE__);
    }
    _vbBuffer = SpineExtension::calloc<float>(maxVb,__FILE__, __LINE__);
    _ibBuffer = SpineExtension::calloc<unsigned short>(maxIb,__FILE__, __LINE__);
    
    return true;
}


val getVertexsBuffer(){
    return val(typed_memory_view(_maxVb, _vbBuffer));
}

val getIndexsBuffer(){
    return val(typed_memory_view(_maxIb, _ibBuffer));
}

bool canMergeToBatch(size_t verticesCount,size_t indexCount ,unsigned vertexSize){
    if((verticesCount+_totalVertexCount)*vertexSize > _maxVb){
        return false;
    }
    if(indexCount + _totalIbCount > _maxIb){
        return false;
    }
    return true;
}

bool renderBatch(const val &drawhander ,unsigned vertexSize,const AtlasPage* lastTexture, const BlendMode& blendMode){
    if(_totalVertexCount>0&& _totalIbCount>0&& lastTexture != nullptr){
        drawhander(_totalVertexCount*vertexSize,_totalIbCount,(size_t)lastTexture,blendMode);
        _totalIbCount = 0;
        _totalVertexCount = 0;
        return false;
    }
    return false;
}


void mergeBuffer(float* vbBuffer, size_t verticesCount,size_t vertexSize, unsigned short* indices,size_t indicesCount,bool twoColorTint){
    float* vbPtr = _vbBuffer + _totalVertexCount * vertexSize;
    unsigned short* ibPtr = _ibBuffer + _totalIbCount;

    std::memcpy(vbPtr, vbBuffer, verticesCount * vertexSize * sizeof(float));
     if(_totalVertexCount == 0){
        std::memcpy(ibPtr, indices,  indicesCount * sizeof(unsigned short));
    }else{
        for (size_t i = 0,newCount = size_t(indicesCount/3); i < newCount; i++) {
            *ibPtr++ = _totalVertexCount + *indices++;
            *ibPtr++ = _totalVertexCount + *indices++;
            *ibPtr++ = _totalVertexCount + *indices++;
        }
    }


    _totalIbCount += indicesCount;
    _totalVertexCount += verticesCount;
}

void mergeBuffer(float* pos,float* uv, size_t verticesCount,size_t vertexSize, unsigned short* indices,size_t indicesCount,bool twoColorTint){
    float* vbPtr = _vbBuffer + _totalVertexCount * vertexSize;
    unsigned short* ibPtr = _ibBuffer + _totalIbCount;

    for(int i = 0;i<verticesCount;i++){
        *vbPtr++ = *pos++;
        *vbPtr++ = *pos++;
        *vbPtr++ = color.r;
        *vbPtr++ = color.g;
        *vbPtr++ = color.b;
        *vbPtr++ = color.a;
        *vbPtr++ = *uv++;
        *vbPtr++ = *uv++;
        if(twoColorTint){
            *vbPtr++ = color.r;
            *vbPtr++ = color.g;
            *vbPtr++ = color.b;
            *vbPtr++ = color.a;
        }
    }

    if(_totalVertexCount == 0){
        std::memcpy(ibPtr, indices,  indicesCount * sizeof(unsigned short));
    }else{
        for (size_t i = 0,newCount = size_t(indicesCount/3); i < newCount; i++) {
            *ibPtr++ = _totalVertexCount + *indices++;
            *ibPtr++ = _totalVertexCount + *indices++;
            *ibPtr++ = _totalVertexCount + *indices++;
        }
    }

    _totalIbCount += indicesCount;
    _totalVertexCount += verticesCount;
}

val getUVs(float u, float v, float u2, float v2, bool rotate) {
    auto _uvs = val::array();
	if (rotate) {
        _uvs.set(0,u2);
        _uvs.set(1,v2);
        _uvs.set(2,u);
        _uvs.set(3,v2);
        _uvs.set(4,u);
        _uvs.set(5,v);
        _uvs.set(6,u2);
        _uvs.set(7,v);
	} else {
        _uvs.set(0,u);
        _uvs.set(1,v2);
        _uvs.set(2,u);
        _uvs.set(3,v);
        _uvs.set(4,u2);
        _uvs.set(5,v);
        _uvs.set(6,u2);
        _uvs.set(7,v2);
	}
    return _uvs;
}

bool slotIsOutRange(Slot& slot, int startSlotIndex, int endSlotIndex) {
    if(startSlotIndex < 0 && endSlotIndex < 0){
        return false;
    }
    const int index = slot.getData().getIndex();
    return startSlotIndex > index || endSlotIndex < index;
}

bool nothingToDraw(Slot& slot, int startSlotIndex, int endSlotIndex) {
    Attachment *attachment = slot.getAttachment();
    if (!attachment ||
        slotIsOutRange(slot, startSlotIndex, endSlotIndex) ||
        !slot.getBone().isActive())
        return true;
    const auto& attachmentRTTI = attachment->getRTTI();
    if (attachmentRTTI.isExactly(ClippingAttachment::rtti))
        return false;
    if (slot.getColor().a == 0)
        return true;
    if (attachmentRTTI.isExactly(RegionAttachment::rtti)) {
        if (static_cast<RegionAttachment*>(attachment)->getColor().a == 0)
            return true;
    }
    else if (attachmentRTTI.isExactly(MeshAttachment::rtti)) {
        if (static_cast<MeshAttachment*>(attachment)->getColor().a == 0)
            return true;
    }
    return false;
}
void drawSkeleton(val drawhander , Skeleton* skeleton,bool twoColorTint,float slotRangeStart = -1,float slotRangeEnd = -1) {
    
    size_t vertexSize = 8;
    if(twoColorTint)  vertexSize = 12;

    AtlasPage* lastTexture = nullptr;
    BlendMode blendMode;
    LayaAttachmentVertices* attachmentVertices = nullptr;
    Color *attachmentColor = nullptr;
    float* vertices = nullptr;
    unsigned short* indices = nullptr;
    size_t verticesCount = 0;
    size_t indicesCount = 0;
    AtlasPage* texture = nullptr;

    Slot **drawOrder = skeleton->getDrawOrder().buffer();
	for (unsigned i = 0,n = skeleton->getSlots().size(); i < n; ++i) {
		Slot* slot = drawOrder[i];
		Attachment *attachment = slot->getAttachment();
        if (nothingToDraw(*slot, slotRangeStart, slotRangeEnd))
        {
            clipper.clipEnd(*slot);
            continue;
        }

		if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
			RegionAttachment *regionAttachment = (RegionAttachment *) attachment;
            attachmentVertices = static_cast<LayaAttachmentVertices*>(regionAttachment->getRendererObject());
            vertices = (float*)(attachmentVertices->m_vertexDatas);
            attachmentColor = &regionAttachment->getColor();
			regionAttachment->computeWorldVertices(slot->getBone(), vertices, 0, vertexSize);
			verticesCount = attachmentVertices->m_verticesCount;
			indices = attachmentVertices->m_triangles;
			indicesCount = attachmentVertices->m_trianglesCount;
			texture =((AtlasRegion*)(attachmentVertices->m_page))->page;
		} else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
			MeshAttachment *mesh = (MeshAttachment *) attachment;
			attachmentColor = &mesh->getColor();
            attachmentVertices = static_cast<LayaAttachmentVertices*>(mesh->getRendererObject());
            vertices = (float*)(attachmentVertices->m_vertexDatas);
			mesh->computeWorldVertices(*slot, 0, mesh->getWorldVerticesLength(), vertices, 0, vertexSize);
			verticesCount = attachmentVertices->m_verticesCount;
			indices = attachmentVertices->m_triangles;
			indicesCount = attachmentVertices->m_trianglesCount;
			texture =((AtlasRegion*)(attachmentVertices->m_page))->page;
		} else if (attachment->getRTTI().isExactly(ClippingAttachment::rtti)) {
			ClippingAttachment *clip = (ClippingAttachment *) slot->getAttachment();
			clipper.clipStart(*slot, clip);
			continue;
		} else{
            clipper.clipEnd(*slot);
            continue;
        }

        BlendMode slotBlendMode = slot->getData().getBlendMode();
        bool needNewMat = false;
        if (slotBlendMode != blendMode) {
            needNewMat = true;
        }

        if(lastTexture != texture){
            needNewMat = true;
        }

        if(needNewMat){
            renderBatch(drawhander,vertexSize,lastTexture,blendMode);
            lastTexture = texture;
            blendMode = slotBlendMode;
        }

        color.r = skeleton->getColor().r * slot->getColor().r * attachmentColor->r ;
		color.g = skeleton->getColor().g * slot->getColor().g * attachmentColor->g ;
		color.b = skeleton->getColor().b * slot->getColor().b * attachmentColor->b ;
		color.a = skeleton->getColor().a * slot->getColor().a * attachmentColor->a ;
		if (clipper.isClipping()) {
			clipper.clipTriangles(vertices, indices,(size_t)indicesCount ,vertices+6, vertexSize);
            indicesCount = clipper.getClippedTriangles().size();
            indices = clipper.getClippedTriangles().buffer();

            verticesCount = clipper.getClippedVertices().size() >> 1;
            if(verticesCount == 0){
                clipper.clipEnd(*slot);
                continue;
            }
            if(!canMergeToBatch(verticesCount,indicesCount,vertexSize)){
                renderBatch(drawhander,vertexSize,lastTexture,blendMode);
            }

            float *posPtr = clipper.getClippedVertices().buffer();
            float *uv = clipper.getClippedUVs().buffer();
            mergeBuffer(posPtr,uv,verticesCount,vertexSize,indices,indicesCount,twoColorTint);
           
		}else{
            for(int i=0;i<verticesCount;i++){
                vertices[i*vertexSize+2] = color.r;
                vertices[i*vertexSize+3] = color.g;
                vertices[i*vertexSize+4] = color.b;
                vertices[i*vertexSize+5] = color.a;
            }
            if(!canMergeToBatch(verticesCount,indicesCount,vertexSize)){
                renderBatch(drawhander,vertexSize,lastTexture,blendMode);
            }
            mergeBuffer(vertices,verticesCount,vertexSize,indices,indicesCount,twoColorTint);
        }
		clipper.clipEnd(*slot);
	}
    renderBatch(drawhander,vertexSize,lastTexture,blendMode);
    clipper.clipEnd();
}


SkeletonClipping* getClipper(){
    return &clipper;
}


val crateAnimation(Animation *animation){
    val obj = val::object();
    // obj.set("name", animation->getName().buffer());
    obj.set("duration", animation->getDuration());
    // obj.set("timelines", vectorToArray(animation->getTimelines()));
    return obj;
}

val createTrackEntry(size_t ptr) {
    TrackEntry* entry = (TrackEntry*)ptr;
    val objct = val::object();
    objct.set("animation", crateAnimation(entry->getAnimation()));
    objct.set("loop", entry->getLoop());
    objct.set("delay", entry->getDelay());
    objct.set("trackIndex", entry->getTrackIndex());
    objct.set("mixTime", entry->getMixTime());
    objct.set("mixDuration", entry->getMixDuration());
    objct.set("mixBlend", entry->getMixBlend());
    objct.set("animationTime", entry->getAnimationTime());
   
    objct.set("timeScale", entry->getTimeScale());
    objct.set("alpha", entry->getAlpha());
    objct.set("eventThreshold", entry->getEventThreshold());
    objct.set("attachmentThreshold", entry->getAttachmentThreshold());
    objct.set("drawOrderThreshold", entry->getDrawOrderThreshold());
    objct.set("animationStart", entry->getAnimationStart());
    objct.set("animationEnd", entry->getAnimationEnd());
    objct.set("animationLast", entry->getAnimationLast());
    return objct;
}
EMSCRIPTEN_BINDINGS(spine)
{
    register_vector<std::string>("StringVector");

    function("drawSkeleton", &drawSkeleton,allow_raw_pointer<arg<0>>());
    function("createBuffer", &createBuffer,allow_raw_pointer<arg<0>>());
    function("getVertexsBuffer", &getVertexsBuffer);
    function("getIndexsBuffer", &getIndexsBuffer);

    function("getClipper", &getClipper,allow_raw_pointer<arg<0>>());

    function("wrapEventData",&wrapPointer<EventData>,allow_raw_pointer<arg<0>>());
    function("wrapEvent",&wrapPointer<Event>,allow_raw_pointer<arg<0>>());
    function("wrapAtlas",&wrapPointer<Atlas>,allow_raw_pointer<arg<0>>());
    function("wrapTextureLoader",&wrapPointer<TextureLoader>,allow_raw_pointer<arg<0>>());
    function("wrapAnimationStateListenerObject",&wrapPointer<AnimationStateListenerObject>,allow_raw_pointer<arg<0>>());
    function("wrapAtlasAttachmentLoader",&wrapPointer<LayaAtlasAttachmentLoader>,allow_raw_pointer<arg<0>>());
    function("wrapAtlasPage",&wrapPointer<AtlasPage>,allow_raw_pointer<arg<0>>());
    function("wrapAnimationState",&wrapPointer<AnimationState>,allow_raw_pointer<arg<0>>());
    function("wrapTrackEntry",&wrapPointer<TrackEntry>,allow_raw_pointer<arg<0>>());
    // function("wrapTrackEntry",&createTrackEntry,allow_raw_pointer<arg<0>>());

    enum_<TextureFilter>("TextureFilter")
        .value("Unknown", TextureFilter::TextureFilter_Unknown)
        .value("Nearest", TextureFilter::TextureFilter_Nearest)
        .value("Linear", TextureFilter::TextureFilter_Linear)
        .value("MipMap", TextureFilter::TextureFilter_MipMap)
        .value("MipMapNearestNearest", TextureFilter::TextureFilter_MipMapNearestNearest)
        .value("MipMapLinearNearest", TextureFilter::TextureFilter_MipMapLinearNearest)
        .value("MipMapNearestLinear", TextureFilter::TextureFilter_MipMapNearestLinear)
        .value("MipMapLinearLinear", TextureFilter::TextureFilter_MipMapLinearLinear);
        
    enum_<Format>("Format")
        .value("Alpha", Format::Format_Alpha)
        .value("Intensity", Format::Format_Intensity)
        .value("LuminanceAlpha", Format::Format_LuminanceAlpha)
        .value("RGB565", Format::Format_RGB565)
        .value("RGBA4444", Format::Format_RGBA4444)
        .value("RGB888", Format::Format_RGB888)
        .value("RGBA8888", Format::Format_RGBA8888);

    enum_<TextureWrap>("TextureWrap")
        .value("MirroredRepeat", TextureWrap::TextureWrap_MirroredRepeat)
        .value("ClampToEdge", TextureWrap::TextureWrap_ClampToEdge)
        .value("Repeat", TextureWrap::TextureWrap_Repeat);

    enum_<EventType>("EventType")
        .value("Start", EventType::EventType_Start)
        .value("Interrupt", EventType::EventType_Interrupt)
        .value("End", EventType::EventType_End)
        .value("Complete", EventType::EventType_Complete)
        .value("Dispose", EventType::EventType_Dispose)
        .value("Event", EventType::EventType_Event);
    
    enum_<BlendMode>("BlendMode")
        .value("Normal", BlendMode::BlendMode_Normal)
        .value("Additive", BlendMode::BlendMode_Additive)
        .value("Multiply", BlendMode::BlendMode_Multiply)
        .value("Screen", BlendMode::BlendMode_Screen);
    
    enum_<TransformMode>("TransformMode")
        .value("Normal", TransformMode::TransformMode_Normal)
        .value("OnlyTranslation", TransformMode::TransformMode_OnlyTranslation)
        .value("NoRotationOrReflection", TransformMode::TransformMode_NoRotationOrReflection)
        .value("NoScale", TransformMode::TransformMode_NoScale)
        .value("NoScaleOrReflection", TransformMode::TransformMode_NoScaleOrReflection);

    enum_<MixBlend>("MixBlend")
        .value("MixBlend_Setup", MixBlend::MixBlend_Setup)
        .value("MixBlend_First", MixBlend::MixBlend_First)
        .value("MixBlend_Replace", MixBlend::MixBlend_Replace)
        .value("MixBlend_Add", MixBlend::MixBlend_Add);
    
    enum_<MixDirection>("MixDirection")
        .value("MixDirection_In", MixDirection::MixDirection_In)
        .value("MixDirection_Out", MixDirection::MixDirection_Out);


    class_<String>("String")
        .constructor<>()
        .constructor(&createString)
        .function("length", &String::length)
        .function("isEmpty", &String::isEmpty)
        .function("buffer",optional_override([](String &str)
                                                 {return getString(str); }));
                                            
    value_object<Color>("Color")
        .field("r", &Color::r)
        .field("g", &Color::g)
        .field("b", &Color::b)
        .field("a", &Color::a)
        ;


    class_<EventData>("EventData")
        .property("volume", &EventData::getVolume)
        .property("balance", &EventData::getBalance)
        .property("intValue", &EventData::getIntValue)
        .property("floatValue", &EventData::getFloatValue)
        .function("getStringValue",optional_override([](EventData data)
                {return getString(data.getStringValue()); }))
        .function("getAudioPath",optional_override([](EventData data)
                {return getString(data.getAudioPath()); }))
        .function("getName",optional_override([](EventData data)
                {return getString(data.getName()); }))

        ;

    class_<Event>("Event")
        .function("getData",optional_override([](Event* event)
                {return (size_t)&event->getData(); }),allow_raw_pointer<arg<0>>())
        .function("getTime", &Event::getTime)
        .function("getIntValue", &Event::getIntValue)
        .function("setIntValue", &Event::setIntValue)
        .function("getFloatValue", &Event::getFloatValue)
        .function("setFloatValue", &Event::setFloatValue)
        .function("getStringValue",optional_override([](Event* event)
                {return getString(event->getStringValue()); }), allow_raw_pointers())
        .function("getVolume", &Event::getVolume)
        .function("setVolume", &Event::setVolume)
        .function("getBalance", &Event::getBalance)
        .function("setBalance", &Event::setBalance)
        .function("getAudioPath",optional_override([](Event* event)
                {return getString(event->getData().getAudioPath()); }), allow_raw_pointers())
        .function("getName",optional_override([](Event* event)
                {return getString(event->getData().getName()); }), allow_raw_pointers())
        
        ;

    class_<AtlasPage>("AtlasPage")
        .smart_ptr<std::shared_ptr<AtlasPage>>("shared_ptr<AtlasPage>")
        .constructor<String>()
        .function("getName",optional_override([](AtlasPage* page)
                {return getString(page->name); }), allow_raw_pointers())
        .function("getTexturePath",optional_override([](AtlasPage* page)
                {return getString(page->texturePath); }), allow_raw_pointers())
        .property("format", &AtlasPage::format)
        .property("minFilter", &AtlasPage::minFilter)
        .property("magFilter", &AtlasPage::magFilter)
        .property("uWrap", &AtlasPage::uWrap)
        .property("vWrap", &AtlasPage::vWrap)
        .property("width", &AtlasPage::width)
        .property("height", &AtlasPage::height)
        ;

    class_<TextureLoader>("TextureLoader")
        .smart_ptr<std::shared_ptr<TextureLoader>>("shared_ptr<TextureLoader>")
        .allow_subclass<TextureLoaderWrapper>("TextureLoaderWrapper")
        .function("load", &TextureLoader::load,pure_virtual())
        .function("unload", &TextureLoader::unload,allow_raw_pointer<arg<0>>())
        ;

    class_<AnimationStateListenerObject>("AnimationStateListenerObject")
        .smart_ptr<std::shared_ptr<AnimationStateListenerObject>>("shared_ptr<AnimationStateListenerObject>")
        .allow_subclass<AnimationStateListenerObjectWarpper>("AnimationStateListenerObjectWarpper")
        .function("callback", &AnimationStateListenerObject::callback,allow_raw_pointer<arg<4>>())
        ;

    class_<Atlas>("Atlas")
        .constructor(&creatAtlas);
    
    //  class_<AtlasAttachmentLoader>("AtlasAttachmentLoader")
    //     .constructor<Atlas*>()
    //     ;

    class_<LayaAtlasAttachmentLoader>("AtlasAttachmentLoader")
        .smart_ptr<std::shared_ptr<LayaAtlasAttachmentLoader>>("shared_ptr<LayaAtlasAttachmentLoader>")
        .constructor<Atlas*>()
        ;
        
    class_<SkeletonClipping>("SkeletonClipping")
        .function("clipStart", &SkeletonClipping::clipStart,allow_raw_pointer<arg<0>>())
        .function("clipEndWithSlot",select_overload<void(Slot&)>(&SkeletonClipping::clipEnd),allow_raw_pointers())
        .function("clipEnd",select_overload<void()>(&SkeletonClipping::clipEnd),allow_raw_pointers())
        .function("clipTriangles",optional_override([](SkeletonClipping *clip, const size_t verticesPtr , const  size_t trianglesPtr,size_t trianglesLength,size_t uvsPtr,size_t stride)
                {
                    clip->clipTriangles(reinterpret_cast<float*>(verticesPtr),
                                                reinterpret_cast<unsigned short*>(trianglesPtr) ,
                                                trianglesLength,reinterpret_cast<float*>(uvsPtr), stride); 
                }), allow_raw_pointers())
        .function("isClipping", &SkeletonClipping::isClipping)
        ;

    class_<LayaAttachmentVertices>("LayaAttachmentVertices")
        .constructor<void*,size_t,size_t>()
        .function("getPage" ,optional_override([](LayaAttachmentVertices *att)
                {return ((AtlasRegion*)att->m_page)->page;}), allow_raw_pointers())
        .property("verticesCount", &LayaAttachmentVertices::m_verticesCount)
        .property("trianglesCount", &LayaAttachmentVertices::m_trianglesCount)
        .function("getvertexDatas",optional_override([](LayaAttachmentVertices *att)
                {return val(typed_memory_view(att->m_verticesCount*8, (float*)att->m_vertexDatas));}), allow_raw_pointers())
        .function("getIndexs" ,optional_override([](LayaAttachmentVertices *att)
                {return val(typed_memory_view(att->m_trianglesCount, att->m_triangles));}), allow_raw_pointers())
        ;

    class_<Attachment>("Attachment")
        .function("getName",optional_override([](Attachment *attachment)
                {return getString(attachment->getName()); }), allow_raw_pointers())
        .function("reference", &Attachment::reference)
        .function("dereference", &Attachment::dereference)
        ;

    class_<RegionAttachment,base<Attachment>>("RegionAttachment")
        .function("updateOffset", &RegionAttachment::updateOffset)
        .function("computeWorldVertices",optional_override([](RegionAttachment *att,Bone bone, const size_t dataPtr ,size_t offset,size_t stride)
                {
                    att->computeWorldVertices(bone,reinterpret_cast<float*>(dataPtr), offset, stride); 
                }), allow_raw_pointers())
        .function("getX", &RegionAttachment::getX)
        .function("setX", &RegionAttachment::setX)
        .function("getY", &RegionAttachment::getY)
        .function("setY", &RegionAttachment::setY)
        .function("getScaleX", &RegionAttachment::getScaleX)
        .function("setScaleX", &RegionAttachment::setScaleX)
        .function("getScaleY", &RegionAttachment::getScaleY)
        .function("setScaleY", &RegionAttachment::setScaleY)
        .function("getRotation", &RegionAttachment::getRotation)
        .function("setRotation", &RegionAttachment::setRotation)
        .function("getWidth", &RegionAttachment::getWidth)
        .function("setWidth", &RegionAttachment::setWidth)
        .function("getHeight", &RegionAttachment::getHeight)
        .function("setHeight", &RegionAttachment::setHeight)
        .function("getColor", &RegionAttachment::getColor)
        .function("getPath",optional_override([](RegionAttachment *att)
                {return getString(att->getPath()); }), allow_raw_pointers())
        .function("setPath",optional_override([](RegionAttachment *att, std::string path)
                {
                    att->setPath(createString(path));
                }), allow_raw_pointers())
        .function("getRendererObject", optional_override([](RegionAttachment *att)
                {
                    return  (LayaAttachmentVertices*)att->getRendererObject();
                }), allow_raw_pointers())
        .function("getRegionOffsetX", &RegionAttachment::getRegionOffsetX)
        .function("setRegionOffsetX", &RegionAttachment::setRegionOffsetX)
        .function("getRegionOffsetY", &RegionAttachment::getRegionOffsetY)
        .function("setRegionOffsetY", &RegionAttachment::setRegionOffsetY)
        .function("getRegionWidth", &RegionAttachment::getRegionWidth)
        .function("setRegionWidth", &RegionAttachment::setRegionWidth)
        .function("getRegionHeight", &RegionAttachment::getRegionHeight)
        .function("setRegionHeight", &RegionAttachment::setRegionHeight)
        .function("getRegionOriginalWidth", &RegionAttachment::getRegionOriginalWidth)
        .function("setRegionOriginalWidth", &RegionAttachment::setRegionOriginalWidth)
        .function("getRegionOriginalHeight", &RegionAttachment::getRegionOriginalHeight)
        .function("setRegionOriginalHeight", &RegionAttachment::setRegionOriginalHeight)
        .function("getOffset" ,optional_override([](RegionAttachment *att)
                {return createMemoryView(att->getOffset()); }), allow_raw_pointers())
        .function("getUVs" ,optional_override([](RegionAttachment *att)
                {return createMemoryView(att->getUVs());}), allow_raw_pointers())
        .function("getPage" ,optional_override([](RegionAttachment *att)
                {
                    LayaAttachmentVertices* attachmentVertices = static_cast<LayaAttachmentVertices*>(att->getRendererObject());
			        return ((AtlasRegion*)(attachmentVertices->m_page))->page;
                }), allow_raw_pointers())
        .function("getRotateUVs" ,optional_override([](RegionAttachment *att)
                {
                    LayaAttachmentVertices* attachmentVertices = static_cast<LayaAttachmentVertices*>(att->getRendererObject());
                    AtlasRegion *regionP  = (AtlasRegion*)(attachmentVertices->m_page);
                    return getUVs(regionP->u, regionP->v, regionP->u2, regionP->v2, regionP->rotate);
                }), allow_raw_pointers())
        ;

    class_<MeshAttachment,base<Attachment>>("MeshAttachment")
        .function("updateUVs", &MeshAttachment::updateUVs)
        .function("computeWorldVertices",optional_override([](MeshAttachment *att,Slot slot,size_t start, size_t count, const size_t dataPtr,size_t offset,size_t stride)
                {
                    att->computeWorldVertices(slot,start,count,reinterpret_cast<float*>(dataPtr) , offset, stride); 
                }), allow_raw_pointers())
        .function("getColor", &MeshAttachment::getColor)
        .function("getWorldVerticesLength", &MeshAttachment::getWorldVerticesLength)
        .function("getRegionU", &MeshAttachment::getRegionU)
        .function("setRegionU", &MeshAttachment::setRegionU)
        .function("getRegionV", &MeshAttachment::getRegionV)
        .function("setRegionV", &MeshAttachment::setRegionV)
        .function("getRegionU2", &MeshAttachment::getRegionU2)
        .function("setRegionU2", &MeshAttachment::setRegionU2)
        .function("getRegionV2", &MeshAttachment::getRegionV2)
        .function("setRegionV2", &MeshAttachment::setRegionV2)
        .function("getRegionRotate", &MeshAttachment::getRegionRotate)
        .function("setRegionRotate", &MeshAttachment::setRegionRotate)
        .function("getRegionDegrees", &MeshAttachment::getRegionDegrees)
        .function("setRegionDegrees", &MeshAttachment::setRegionDegrees)
        .function("getRegionOffsetX", &MeshAttachment::getRegionOffsetX)
        .function("setRegionOffsetX", &MeshAttachment::setRegionOffsetX)
        .function("getRegionOffsetY", &MeshAttachment::getRegionOffsetY)
        .function("setRegionOffsetY", &MeshAttachment::setRegionOffsetY)
        .function("getRegionWidth", &MeshAttachment::getRegionWidth)
        .function("setRegionWidth", &MeshAttachment::setRegionWidth)
        .function("getRegionHeight", &MeshAttachment::getRegionHeight)
        .function("setRegionHeight", &MeshAttachment::setRegionHeight)
        .function("getRegionOriginalWidth", &MeshAttachment::getRegionOriginalWidth)
        .function("setRegionOriginalWidth", &MeshAttachment::setRegionOriginalWidth)
        .function("getRegionOriginalHeight", &MeshAttachment::getRegionOriginalHeight)
        .function("setRegionOriginalHeight", &MeshAttachment::setRegionOriginalHeight)
        .function("getEdges", optional_override([](MeshAttachment *mesh)
                {return createMemoryView(mesh->getEdges()); }), allow_raw_pointers())
        .function("getHullLength", &MeshAttachment::getHullLength)
        .function("setHullLength", &MeshAttachment::setHullLength)
        .function("getVertices", optional_override([](MeshAttachment *mesh)
                {return createMemoryView(mesh->getVertices()); }), allow_raw_pointers())
        .function("getWorldVerticesLength", &MeshAttachment::getWorldVerticesLength)
        .function("getUVs", optional_override([](MeshAttachment *mesh) 
                {return createMemoryView(mesh->getUVs()); }), allow_raw_pointers())
        .function("getTriangles", optional_override([](MeshAttachment *mesh)
                {return createMemoryView(mesh->getTriangles()); }), allow_raw_pointers())
        .function("getRegionUvs", optional_override([](MeshAttachment *mesh)
                {return createMemoryView(mesh->getRegionUVs()); }), allow_raw_pointers())
        .function("getPage" ,optional_override([](MeshAttachment *mesh)
                 {
                    LayaAttachmentVertices* attachmentVertices = static_cast<LayaAttachmentVertices*>(mesh->getRendererObject());
			        return ((AtlasRegion*)(attachmentVertices->m_page))->page;
                }), allow_raw_pointers())
        .function("getBones",optional_override([](MeshAttachment *mesh)
                {return createMemoryView(mesh->getBones());}),allow_raw_pointers())
      
        ;
    
    class_<ClippingAttachment,base<Attachment>>("ClippingAttachment")
        .smart_ptr<std::shared_ptr<ClippingAttachment>>("std::shared_ptr<ClippingAttachment>")
        .function("getEndSlot", &ClippingAttachment::getEndSlot,allow_raw_pointers())
        .function("setEndSlot", &ClippingAttachment::setEndSlot,allow_raw_pointers())
        ;

    class_<SlotData>("SlotData")
        .function("getIndex", &SlotData::getIndex)
        .function("getName",optional_override([](SlotData *slotData)
                {return getString(slotData->getName()); }), allow_raw_pointers())
        .function("getBoneData", &SlotData::getBoneData,allow_raw_pointers())
        .function("getColor", &SlotData::getColor)
        .function("getDarkColor", &SlotData::getDarkColor)
        .function("hasDarkColor", &SlotData::hasDarkColor)
        .function("setHasDarkColor", &SlotData::setHasDarkColor)
        .function("getAttachmentName",optional_override([](SlotData *slotData)
                {return getString(slotData->getAttachmentName()); }), allow_raw_pointers())
        .function("setAttachmentName",optional_override([](SlotData *slotData, std::string attachmentName)
                {
                    slotData->setAttachmentName(createString(attachmentName));
                }), allow_raw_pointers())
        .function("getBlendMode", &SlotData::getBlendMode)
        ;

    class_<Slot>("Slot")
        .function("getBone", &Slot::getBone,allow_raw_pointers())
        .function("getSkeleton",&Slot::getSkeleton, allow_raw_pointers())
        .function("getColor",&Slot::getColor, allow_raw_pointers())
        .function("getDarkColor",&Slot::getDarkColor, allow_raw_pointers())
        .function("hasDarkColor",&Slot::hasDarkColor)
        .function("getAttachment", &Slot::getAttachment,allow_raw_pointers())
        .function("setAttachment", &Slot::setAttachment,allow_raw_pointers())
        .function("getAttachmentTime", &Slot::getAttachmentTime)
        .function("setAttachmentTime", &Slot::setAttachmentTime)
        .function("getAttachmentState", &Slot::getAttachmentState,allow_raw_pointers())
        .function("setAttachmentState", &Slot::setAttachmentState,allow_raw_pointers())
        .function("getData", &Slot::getData,allow_raw_pointers())
        ;

    class_<ConstraintData>("ConstraintData")
        .function("getName",optional_override([](ConstraintData *constraintData)
                {return getString(constraintData->getName()); }), allow_raw_pointers())
        .function("getOrder", &ConstraintData::getOrder)
        .function("setOrder", &ConstraintData::setOrder)
        .function("isSkinRequired", &ConstraintData::isSkinRequired)
        .function("setSkinRequired", &ConstraintData::setSkinRequired)
        ;
        
    class_<Skin>("Skin")
        .function("getName",optional_override([](Skin *skin)
                {return getString(skin->getName()); }), allow_raw_pointers())
        .function("getAttachment",optional_override([](Skin *skin, int slotIndex, std::string name)
                {return skin->getAttachment(slotIndex, createString(name));}),allow_raw_pointers())
        .function("setAttachment",optional_override([](Skin *skin, int slotIndex, std::string name, Attachment *attachment)
                {skin->setAttachment(slotIndex, createString(name), attachment);}),allow_raw_pointers())
        .function("getAttachments",optional_override([](Skin *skin)
                {
                    val datas = val::object();
                    Skin::AttachmentMap::Entries attachments = skin->getAttachments();
                    while (attachments.hasNext()) {
                        Skin::AttachmentMap::Entry entry = attachments.next();
                        if(datas.call<bool>("hasOwnProperty",entry._slotIndex)){
                            val arr = datas[entry._slotIndex];
                            arr.set(getString(entry._name),entry._attachment);
                        }else{
                            val arr = val::object();
                            datas.set(entry._slotIndex,arr);
                            arr.set(getString(entry._name),entry._attachment);
                        };
                    }
                    return datas;
                }),allow_raw_pointers())
        .function("getBones",optional_override([](Skin *skin, int slotIndex)
                {return vectorToArray(skin->getBones());}),allow_raw_pointers())
        .function("getConstraints",optional_override([](Skin *skin, std::string slotName)
                {return vectorToArray(skin->getConstraints());}),allow_raw_pointers())
        ;
    class_<Timeline>("Timeline")
        .function("getPropertyId", &Timeline::getPropertyId)
        ;

    class_<AttachmentTimeline,base<Timeline>>("AttachmentTimeline")
        .function("apply",optional_override([](AttachmentTimeline *timeline, Skeleton &skeleton, float lastTime, float time, std::vector<Event *> &pEvents, float alpha,MixBlend blend, MixDirection direction)
                {
                    timeline->apply(skeleton, lastTime, time, createVector(pEvents), alpha, blend, direction);
                }),allow_raw_pointers())
        .function("getPropertyId", &AttachmentTimeline::getPropertyId)
        .function("getFrameCount", &AttachmentTimeline::getFrameCount)
        .function("getFrames", optional_override([](AttachmentTimeline *timeline)
                {
                    return  vectorToArray(timeline->getFrames());
                }),allow_raw_pointers())
        .function("setFrame", optional_override([](AttachmentTimeline *timeline, int frameIndex, float time, std::string attachmentName)
                {
                    timeline->setFrame(frameIndex, time, createString(attachmentName));
                }),allow_raw_pointers())
        .function("getSlotIndex", &AttachmentTimeline::getSlotIndex)
        .function("setSlotIndex", &AttachmentTimeline::setSlotIndex)
        .function("getAttachmentNames", optional_override([](AttachmentTimeline *timeline)
                {
                    val strs = val::array();
                    for (int i = 0; i < timeline->getAttachmentNames().size(); i++) {
                        strs.set(i, getString(timeline->getAttachmentNames()[i]));
                    }
                    return strs;
                }),allow_raw_pointers())
        ;

    class_<DrawOrderTimeline,base<Timeline>>("DrawOrderTimeline")
        .function("apply",optional_override([](DrawOrderTimeline *timeline, Skeleton &skeleton, float lastTime, float time, std::vector<Event *> &pEvents, float alpha,MixBlend blend, MixDirection direction)
                {
                    timeline->apply(skeleton, lastTime, time, createVector(pEvents), alpha, blend, direction);
                }),allow_raw_pointers())
        .function("getPropertyId", &DrawOrderTimeline::getPropertyId)
        .function("getFrameCount", &DrawOrderTimeline::getFrameCount)
        .function("getFrames", optional_override([](DrawOrderTimeline *timeline)
                {
                    return  createMemoryView(timeline->getFrames());
                }),allow_raw_pointers())
        .function("setFrame", optional_override([](DrawOrderTimeline *timeline, int frameIndex, float time, std::vector<int> &drawOrder)
                {
                    timeline->setFrame(frameIndex, time, createVector(drawOrder)[0]);
                }),allow_raw_pointers())
        .function("getDrawOrders", optional_override([](DrawOrderTimeline *timeline)
                {
                    val arr = val::array();
                    for (int i = 0; i < timeline->getDrawOrders().size(); i++) {
                        arr.set(i, vectorToArray(timeline->getDrawOrders()[i]));
                    }
                    return arr;
                }),allow_raw_pointers())
        ;

    class_<ColorTimeline,base<Timeline>>("ColorTimeline")
        .function("apply",optional_override([](ColorTimeline *timeline, Skeleton &skeleton, float lastTime, float time, std::vector<Event *> &pEvents, float alpha,MixBlend blend, MixDirection direction)
                {
                    timeline->apply(skeleton, lastTime, time, createVector(pEvents), alpha, blend, direction);
                }),allow_raw_pointers())
        .function("getPropertyId", &ColorTimeline::getPropertyId)
        .function("getFrameCount", &ColorTimeline::getFrameCount)
        .function("getFrames", optional_override([](ColorTimeline *timeline)
                {
                    return  createMemoryView(timeline->getFrames());
                }),allow_raw_pointers())
        .function("setFrame", optional_override([](ColorTimeline *timeline, int frameIndex, float time, Color &color)
                {
                    timeline->setFrame(frameIndex, time, color.r,color.g,color.b,color.a);
                }),allow_raw_pointers())
        .function("getSlotIndex", &ColorTimeline::getSlotIndex)
        .function("setSlotIndex", &ColorTimeline::setSlotIndex)
        ;
    
    class_<EventTimeline,base<Timeline>>("EventTimeline")
        .function("apply",optional_override([](EventTimeline *timeline, Skeleton &skeleton, float lastTime, float time, std::vector<Event *> &pEvents, float alpha,MixBlend blend, MixDirection direction)
                {
                    timeline->apply(skeleton, lastTime, time, createVector(pEvents), alpha, blend, direction);
                }),allow_raw_pointers())
        .function("getPropertyId", &EventTimeline::getPropertyId)
        .function("getFrameCount", &EventTimeline::getFrameCount)
        .function("setFrame", optional_override([](EventTimeline *timeline, int frameIndex, Event *event)
                {
                    timeline->setFrame(frameIndex, event);
                }),allow_raw_pointers())
        .function("getFrames", optional_override([](EventTimeline *timeline)
                {
                    auto frame  = timeline->getFrames();
                    return  val(typed_memory_view(frame.size(), frame.buffer()));
                }),allow_raw_pointers())
        .function("getEvents", optional_override([](EventTimeline *timeline)
                {
                    return vectorToArray(timeline->getEvents());
                }),allow_raw_pointers())
       

        ;

    class_<Animation>("Animation")
        .function("getName",optional_override([](Animation *anim)
                {return getString( anim->getName());}),allow_raw_pointers())
        .function("getDuration", &Animation::getDuration)
        .function("setDuration", &Animation::setDuration)
        .function("hasTimeline", &Animation::hasTimeline)
        .function("getTimelines", optional_override([](Animation *anim)
                {
                    return vectorToArray(anim->getTimelines());
                }),allow_raw_pointers())
        ;
    
    class_<TrackEntry>("TrackEntry")
        .function("getAnimation", &TrackEntry::getAnimation,allow_raw_pointers())
        .function("getLoop", &TrackEntry::getLoop)
        .function("getAnimationLast", &TrackEntry::getAnimationLast)
        .function("getAnimationTime", &TrackEntry::getAnimationTime)
        .function("getTrackTime", &TrackEntry::getTrackTime)
        .function("setNextAnimationLast", &TrackEntry::setAnimationLast)
        .function("getAnimationStart",&TrackEntry::getAnimationStart)
        .function("getAnimationEnd",&TrackEntry::getAnimationEnd)
        .function("getTimeScale",&TrackEntry::getTimeScale)
        .function("getMixTime",&TrackEntry::getMixTime)
        .function("getAlpha",&TrackEntry::getAlpha)
        ;
        
    class_<SkeletonBinary>("SkeletonBinary")
        .constructor<LayaAtlasAttachmentLoader*,bool>()
        .function("readSkeletonData", optional_override([](SkeletonBinary *binary, val data)
                {
                    const std::vector<unsigned char> dataVec = convertJSArrayToNumberVector<unsigned char>(data);
                    return binary->readSkeletonData(dataVec.data(), dataVec.size()); 
                }), allow_raw_pointers())
        ;

    class_<SkeletonJson>("SkeletonJson")
        .constructor<LayaAtlasAttachmentLoader*,bool>()
        .function("readSkeletonData", optional_override([](SkeletonJson *json, std::string jsondata)
                {
                    return json->readSkeletonData(jsondata.c_str()); 
                }), allow_raw_pointers())
        ;

    class_<BoneData>("BoneData")
        .function("getIndex", &BoneData::getIndex)
        .function("getName",optional_override([](BoneData *boneData)
                {return getString(boneData->getName()); }), allow_raw_pointers())
        .function("getParent", &BoneData::getParent,allow_raw_pointers())
        .function("getLength", &BoneData::getLength)
        .function("setLength", &BoneData::setLength)
        .function("getX", &BoneData::getX)
        .function("setX", &BoneData::setX)
        .function("getY", &BoneData::getY)
        .function("setY", &BoneData::setY)
        .function("getRotation", &BoneData::getRotation)
        .function("setRotation", &BoneData::setRotation)
        .function("getScaleX", &BoneData::getScaleX)
        .function("setScaleX", &BoneData::setScaleX)
        .function("getScaleY", &BoneData::getScaleY)
        .function("setScaleY", &BoneData::setScaleY)
        .function("getShearX", &BoneData::getShearX)
        .function("setShearX", &BoneData::setShearX)
        .function("getShearY", &BoneData::getShearY)
        .function("setShearY", &BoneData::setShearY)
        .function("getTransformMode", &BoneData::getTransformMode)
        .function("setTransformMode", &BoneData::setTransformMode)
        .function("isSkinRequired", &BoneData::isSkinRequired)
        .function("setSkinRequired", &BoneData::setSkinRequired)
        ;

    class_<Bone>("Bone")
        .function("updateWorldTransform",select_overload<void()>(&Bone::updateWorldTransform))
        .function("updateWorldTransform",select_overload<void(float,float,float,float,float,float,float)>(&Bone::updateWorldTransform))
        .function("setToSetupPose", &Bone::setToSetupPose)
        .function("worldToLocal", optional_override([](Bone *bone,float worldX,float worldY)
                {
                    float localX = 0;
                    float localY = 0;
                    bone->worldToLocal(worldX,worldY,localX,localY);
                    return crateVec2Object(localX,localY);
                }),allow_raw_pointers())
        .function("localToWorld", optional_override([](Bone *bone,float localX,float localY)
                {
                    float worldX = 0;
                    float worldY = 0;
                    bone->worldToLocal(localX,localY,worldX,worldY);
                    return crateVec2Object(worldX,worldY);
                }),allow_raw_pointers())
        .function("worldToLocalRotation", optional_override([](Bone *bone,float worldRotation)
                {
                    return bone->worldToLocalRotation(worldRotation);
                }),allow_raw_pointers())
        .function("localToWorldRotation", &Bone::localToWorldRotation)
        .function("rotateWorld", &Bone::rotateWorld)
        .function("getWorldToLocalRotationX", &Bone::getWorldToLocalRotationX)
        .function("getWorldToLocalRotationY", &Bone::getWorldToLocalRotationY)
        .function("getData", &Bone::getData,allow_raw_pointers())
        .function("getChildren", optional_override([](Bone *bone)
                {
                    return vectorToArray(bone->getChildren());
                }),allow_raw_pointers())
        .function("getAShearX", &Bone::getAShearX)
        .function("setAScaleY", &Bone::setAScaleY)
        .function("getAShearY", &Bone::getAShearY)
        .function("setAShearX", &Bone::setAShearX)
        .function("getA", &Bone::getA)
        .function("setA", &Bone::setA)
        .function("getB", &Bone::getB)
        .function("setB", &Bone::setB)
        .function("getC", &Bone::getC)
        .function("setC", &Bone::setC)
        .function("getD", &Bone::getD)
        .function("setD", &Bone::setD)
        .function("getWorldX", &Bone::getWorldX)
        .function("setWorldX", &Bone::setWorldX)
        .function("getWorldY", &Bone::getWorldY)
        .function("setWorldY", &Bone::setWorldY)
        .function("getWorldRotationX", &Bone::getWorldRotationX)
        .function("getWorldRotationY", &Bone::getWorldRotationY)
        .function("getWorldScaleX", &Bone::getWorldScaleX)
        .function("getWorldScaleY", &Bone::getWorldScaleY)
        .function("isAppliedValid", &Bone::isAppliedValid)
        .function("setAppliedValid", &Bone::setAppliedValid)
        .function("isActive", &Bone::isActive)
        .function("setActive", &Bone::setActive)
        
        ;

    class_<SkeletonData>("SkeletonData")
        .function("getName",optional_override([](SkeletonData *data)
                {return getString(data->getName()); }), allow_raw_pointers())
        .function("findBone", optional_override([](SkeletonData *data, std::string boneName)
                {return data->findBone(createString(boneName));}),allow_raw_pointers())
        .function("findBoneIndex", optional_override([](SkeletonData *data, std::string boneName)
                {return data->findBoneIndex(createString(boneName));}),allow_raw_pointers())
        .function("findSlot", optional_override([](SkeletonData *data, std::string slotName)
                {return data->findSlot(createString(slotName));}),allow_raw_pointers())
        .function("findSlotIndex", optional_override([](SkeletonData *data, std::string slotName)
                {return data->findSlotIndex(createString(slotName));}),allow_raw_pointers())
        .function("getSlots", optional_override([](SkeletonData *data)
                {return vectorToArray(data->getSlots());}),allow_raw_pointers())
        .function("getSkins", optional_override([](SkeletonData *data)
                {return vectorToArray(data->getSkins());}),allow_raw_pointers())
        .function("getAnimations", optional_override([](SkeletonData *data)
                {return vectorToArray(data->getAnimations());}),allow_raw_pointers())
        .function("getSkinIndexByName", optional_override([](SkeletonData *data, std::string skinName)
                {
                    String str = createString(skinName);
                    Vector<Skin *> skins = data->getSkins();
                    int index = -1;
                    for(int i = 0; i < skins.size(); i++)
                    {
                        if(skins[i]->getName() == str)
                        {
                            index = i;
                            break;
                        }
                    }
                    return index;
                }), allow_raw_pointers())
        .function("getAnimationsSize", optional_override([](SkeletonData *data)
                {  return data->getAnimations().size();}),allow_raw_pointers())
        .function("getAnimationByIndex", optional_override([](SkeletonData *data, int index)
                { return data->getAnimations()[index];}), allow_raw_pointers())
        ;

    class_<Skeleton>("Skeleton")
        .constructor<SkeletonData*>()
        .function("getData",&Skeleton::getData,allow_raw_pointer<arg<0>>())
        .function("updateWorldTransform",&Skeleton::updateWorldTransform)
        .function("showSkinByIndex", optional_override([](Skeleton *skin, int index)
                                        {
                                            skin->setSkin(skin->getData()->getSkins()[index]);
                                        }), allow_raw_pointers())
        .function("setToSetupPose",&Skeleton::setToSetupPose)
        .function("setBonesToSetupPose",&Skeleton::setBonesToSetupPose)
        .function("setSlotsToSetupPose",&Skeleton::setSlotsToSetupPose)
        .function("getBones", optional_override([](Skeleton *skin)
                                        {
                                            return vectorToArray(skin->getBones());
                                        }),allow_raw_pointers())
        .function("getSlots", optional_override([](Skeleton *skin)
                                        {
                                            return vectorToArray(skin->getSlots());
                                        }),allow_raw_pointers())
        .function("getDrawOrder", optional_override([](Skeleton *skin)
                                        {
                                            return vectorToArray(skin->getDrawOrder());
                                        }),allow_raw_pointers())
        .function("getSkin", &Skeleton::getSkin,allow_raw_pointers())
        .function("setSkin", optional_override([](Skeleton *skin, std::string skinName)
                                        {
                                            skin->setSkin(createString(skinName));
                                        }), allow_raw_pointers())
        .function("getColor", &Skeleton::getColor)
        .function("getTime", &Skeleton::getTime)
        .function("setTime", &Skeleton::setTime)
        .function("setPosition", &Skeleton::setPosition)
        

        ;
    class_<AnimationStateData>("AnimationStateData")
        .constructor<SkeletonData*>()
        ;
    class_<AnimationState>("AnimationState")
        .constructor<AnimationStateData*>()
        .function("update", &AnimationState::update)
        .function("apply", &AnimationState::apply)
        .function("setListener", optional_override([](AnimationState *stat, AnimationStateListenerObject* listener)
                {
                    stat->setListener(listener);
                }), allow_raw_pointers())
        .function("setAnimation", optional_override([](AnimationState *stat, size_t trackIndex, std::string animationName, bool loop)
                {
                    return size_t(stat->setAnimation(trackIndex, createString(animationName), loop));
                }),allow_raw_pointers())
        .function("getCurrent", optional_override([](AnimationState *stat, size_t trackIndex)
                {
                    return size_t(stat->getCurrent(trackIndex));
                }),allow_raw_pointers())
        ;
}