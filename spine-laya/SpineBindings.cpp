
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



template<typename T>
T val_as(const val& v) {
    return v.as<T>();
}

template<typename T>
inline val createMemoryView(Vector<T> &data) {
    return val(typed_memory_view(data.size(), data.buffer()));
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

spine::SpineExtension* spine::getDefaultExtension() {
	return new LayaExtension();
}

inline std::string getString(const String& str) {
     if(str.length()<=0){
        return "";
    }else{
        return std::string(str.buffer());
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
        call<void>("load", page, getString(path));
    }

    void unload(void* texture) override {
        call<void>("unload", texture);
    }
};

class AnimationStateListenerObjectWarpper : public wrapper<AnimationStateListenerObject> {
public:
    EMSCRIPTEN_WRAPPER(AnimationStateListenerObjectWarpper);

    void callback(AnimationState* state, EventType type, TrackEntry* entry, Event* event) override {
        call<void>("callback", state, type, entry, event);
    }
};


SkeletonClipping clipper = SkeletonClipping();
Color color = Color(1, 1, 1, 1);
Vector<float> vbHelpBuffer = Vector<float>();
Vector<float>* vbBuffer =new Vector<float>();
Vector<unsigned short>* ibBuffer =new Vector<unsigned short>();

float *_vbBuffer = nullptr;
unsigned short *_ibBuffer = nullptr;
unsigned short _maxVb = 0;
unsigned short _maxIb = 0;
unsigned short _ibIndex = 0;
unsigned short totalVertexCount = 0;


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
        delete _vbBuffer;
    }
    if(_ibBuffer != nullptr){
        delete _ibBuffer;
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
    if((verticesCount+totalVertexCount)*vertexSize > _maxVb){
        return false;
    }
    if(indexCount + _ibIndex > _maxIb){
        return false;
    }
    return true;
}

bool renderBatch(const val &drawhander ,unsigned vertexSize,const AtlasPage* lastTexture, const BlendMode& blendMode){
    if(totalVertexCount>0&& _ibIndex>0){
        drawhander(totalVertexCount*vertexSize,_ibIndex,getString(lastTexture->texturePath),blendMode);
        _ibIndex = 0;
        totalVertexCount = 0;
        return false;
    }
    return false;
}


void mergeBuffer(Vector<float>* vertices,size_t verticesCount, Vector<unsigned short>* indices,bool twoColorTint,unsigned vertexSize,Vector<float>* uvs,Color *color){
    for (int i = 0; i < verticesCount; i++) {
        size_t bufferOff = (totalVertexCount+i)*vertexSize;
        int index = i*2;
        _vbBuffer[bufferOff] = (*vertices)[index];
        _vbBuffer[bufferOff+1] = (*vertices)[index+1];
        _vbBuffer[bufferOff+2] = color->r;
        _vbBuffer[bufferOff+3] = color->g;
        _vbBuffer[bufferOff+4] = color->b;
        _vbBuffer[bufferOff+5] = color->a;
        _vbBuffer[bufferOff+6] = (*uvs)[index];
        _vbBuffer[bufferOff+7] = (*uvs)[index+1];
        if(twoColorTint){
            _vbBuffer[bufferOff+8] = 0;
            _vbBuffer[bufferOff+9] = 0;
            _vbBuffer[bufferOff+10] = 0;
            _vbBuffer[bufferOff+11] = 0;
        }
    }
    
    for (int ii = 0; ii < (int) indices->size(); ii++){
        _ibBuffer[_ibIndex++] = totalVertexCount+(*indices)[ii];
    }
    totalVertexCount += verticesCount;
}


void drawSkeleton(val drawhander , Skeleton* skeleton,bool twoColorTint,float slotRangeStart = -1,float slotRangeEnd = -1) {
    Vector<unsigned short> quadIndices = Vector<unsigned short>();
    quadIndices.add(0);
    quadIndices.add(1);
    quadIndices.add(2);
    quadIndices.add(2);
    quadIndices.add(3);
    quadIndices.add(0);
   

    size_t vertexSize = 8;
    if(twoColorTint)  vertexSize = 12;

    AtlasPage* lastTexture = nullptr;
    BlendMode blendMode;
	for (unsigned i = 0; i < skeleton->getSlots().size(); ++i) {
		Slot &slot = *skeleton->getDrawOrder()[i];
		Attachment *attachment = slot.getAttachment();
		if (!attachment) {
			clipper.clipEnd(slot);
			continue;
		}

		// Early out if the slot color is 0 or the bone is not active
		if (slot.getColor().a == 0 || !slot.getBone().isActive()) {
			clipper.clipEnd(slot);
			continue;
		}

        size_t clippedVertexSize = clipper.isClipping() ? 2 : vertexSize;

		Vector<float>* vertices = &vbHelpBuffer;
		int verticesCount = 0;
		Vector<float> *uvs = NULL;
		Vector<unsigned short> *indices = nullptr;
		int indicesCount = 0;
		Color *attachmentColor;
        AtlasPage* texture = nullptr;

		if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
			RegionAttachment *regionAttachment = (RegionAttachment *) attachment;
			attachmentColor = &regionAttachment->getColor();

			// Early out if the slot color is 0
			if (attachmentColor->a == 0) {
				clipper.clipEnd(slot);
				continue;
			}

            vertices->setSize(8, 0);
			regionAttachment->computeWorldVertices(slot.getBone(), *vertices, 0, 2);
			verticesCount = 4;
			uvs = &regionAttachment->getUVs();
			indices = &quadIndices;
			indicesCount = 6;
			texture =((AtlasRegion*)regionAttachment->getRendererObject())->page;

		} else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
			MeshAttachment *mesh = (MeshAttachment *) attachment;
			attachmentColor = &mesh->getColor();

			// Early out if the slot color is 0
			if (attachmentColor->a == 0) {
				clipper.clipEnd(slot);
				continue;
			}

            vertices->setSize(mesh->getWorldVerticesLength(), 0);
			mesh->computeWorldVertices(slot, 0, mesh->getWorldVerticesLength(), *vertices, 0, 2);
			texture =((AtlasRegion*)mesh->getRendererObject())->page;
			verticesCount = mesh->getWorldVerticesLength() >> 1;
			uvs = &mesh->getUVs();
			indices = &mesh->getTriangles();
			indicesCount = indices->size();

		} else if (attachment->getRTTI().isExactly(ClippingAttachment::rtti)) {
			ClippingAttachment *clip = (ClippingAttachment *) slot.getAttachment();
			clipper.clipStart(slot, clip);
			continue;
		} else
			continue;

        BlendMode slotBlendMode = slot.getData().getBlendMode();
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
		if (clipper.isClipping()) {
			clipper.clipTriangles(*vertices, *indices, *uvs, 2);
			vertices = &clipper.getClippedVertices();
			verticesCount = clipper.getClippedVertices().size() >> 1;
			uvs = &clipper.getClippedUVs();
			indices = &clipper.getClippedTriangles();
			indicesCount = clipper.getClippedTriangles().size();
		}

        color.r = skeleton->getColor().r * slot.getColor().r * attachmentColor->r ;
		color.g = skeleton->getColor().g * slot.getColor().g * attachmentColor->g ;
		color.b = skeleton->getColor().b * slot.getColor().b * attachmentColor->b ;
		color.a = skeleton->getColor().a * slot.getColor().a * attachmentColor->a ;
        if(!canMergeToBatch(verticesCount,indices->size(),vertexSize)){
           renderBatch(drawhander,vertexSize,lastTexture,blendMode);
        }
        mergeBuffer(vertices,verticesCount,indices,twoColorTint,vertexSize,uvs,&color);
		
		clipper.clipEnd(slot);
	}
	clipper.clipEnd();
    renderBatch(drawhander,vertexSize,lastTexture,blendMode);
}

SkeletonClipping* getClipper(){
    return &clipper;
}

EMSCRIPTEN_BINDINGS(spine)
{
    register_vector<char>("CharVector");
    register_vector<unsigned char>("VectorUnsignedChar");
    register_vector<std::string>("StringVector");
    register_vector<float>("FloatVector");
    register_vector<Event*>("EventVetor");
    function("drawSkeleton", &drawSkeleton,allow_raw_pointer<arg<0>>());
    function("createBuffer", &createBuffer,allow_raw_pointer<arg<0>>());
    function("getVertexsBuffer", &getVertexsBuffer);
    function("getIndexsBuffer", &getIndexsBuffer);

    function("getClipper", &getClipper,allow_raw_pointer<arg<0>>());

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
        .property<val>("stringValue",optional_override([](EventData data)
                {return val::module_property(data.getStringValue().buffer()); }))
        .property<val>("audioPath",optional_override([](EventData data)
                {return val::module_property(data.getAudioPath().buffer()); }))
        .property<val>("eventName",optional_override([](EventData data)
                {return val::module_property(data.getName().buffer()); }))
        ;

    class_<Event>("Event")
        .function("getData", &Event::getData,allow_raw_pointers())
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
    
    class_<AtlasAttachmentLoader>("AtlasAttachmentLoader")
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
        .function("getRendererObject", &RegionAttachment::getRendererObject, allow_raw_pointers())
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
                {return ((AtlasRegion*)att->getRendererObject())->page;}), allow_raw_pointers())
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
                {return ((AtlasRegion*)mesh->getRendererObject())->page;}), allow_raw_pointers())
        .function("getBones",optional_override([](MeshAttachment *mesh)
                {return vectorToArray(mesh->getBones());}),allow_raw_pointers())
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
                    val datas = val::array();
                    Skin::AttachmentMap::Entries attachments = skin->getAttachments();
                    size_t lastslotIndex = -1;
                    val arr ;
                    size_t index = 0;
                    while (attachments.hasNext()) {
                        Skin::AttachmentMap::Entry entry = attachments.next();
                        if(lastslotIndex != entry._slotIndex){
                            if(lastslotIndex!=-1){
                                datas.set(lastslotIndex,arr);
                            }
                            lastslotIndex = entry._slotIndex;
                            index = 0;
                            arr = val::array();
                        }
                        arr.set(entry._name,entry._attachment);
                        index++;
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
                    return  vectorToArray(timeline->getFrames());
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
                    return  vectorToArray(timeline->getFrames());
                }),allow_raw_pointers())
        .function("setFrame", optional_override([](ColorTimeline *timeline, int frameIndex, float time, Color &color)
                {
                    timeline->setFrame(frameIndex, time, color.r,color.g,color.b,color.a);
                }),allow_raw_pointers())
        .function("getSlotIndex", &ColorTimeline::getSlotIndex)
        .function("setSlotIndex", &ColorTimeline::setSlotIndex)
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
        ;
        
    class_<SkeletonBinary>("SkeletonBinary")
        .constructor<AtlasAttachmentLoader*,bool>()
        .function("readSkeletonData", optional_override([](SkeletonBinary *binary, val data)
                {
                    const std::vector<unsigned char> dataVec = convertJSArrayToNumberVector<unsigned char>(data);
                    return binary->readSkeletonData(dataVec.data(), dataVec.size()); 
                }), allow_raw_pointers())
        ;

    class_<SkeletonJson>("SkeletonJson")
        .constructor<AtlasAttachmentLoader*,bool>()
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
        .function("setAnimation", optional_override([](AnimationState *stat, val trackIndex, std::string animationName, bool loop)
                {
                    return  stat->setAnimation(val_as<size_t>(trackIndex), createString(animationName), loop);
                }),allow_raw_pointers())
        .function("getCurrent", optional_override([](AnimationState *stat, val trackIndex)
                {
                    return  stat->getCurrent(val_as<size_t>(trackIndex));
                }),allow_raw_pointers())
        ;
}