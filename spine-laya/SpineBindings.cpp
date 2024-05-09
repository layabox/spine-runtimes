
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

std::string getString(String* str) {
     if(str->length()<=0){
        return "";
    }else{
        return std::string(str->buffer());
    }
}

std::string getString(String str) {
    return getString(&str);
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


String* createString(const std::string str) {
    return new String(const_cast<char*>(str.c_str()));
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

template<typename T>
T val_as(const val& v) {
    return v.as<T>();
}


SkeletonClipping clipper = SkeletonClipping();
Color color = Color(1, 1, 1, 1);
Vector<float> vbHelpBuffer = Vector<float>();
void drawSkeleton(val drawhander , Skeleton* skeleton,bool twoColorTint,float slotRangeStart = -1,float slotRangeEnd = -1) {
    Vector<unsigned short> quadIndices = Vector<unsigned short>();
    quadIndices.add(0);
    quadIndices.add(1);
    quadIndices.add(2);
    quadIndices.add(2);
    quadIndices.add(3);
    quadIndices.add(0);
    Vector<float> vbBuffer = Vector<float>();
    Vector<unsigned short> ibBuffer = Vector<unsigned short>();

    size_t vertexSize = 8;
    if(twoColorTint)  vertexSize = 12;

    AtlasPage* lastTexture = nullptr;
    BlendMode blendMode;
    size_t totalVertexCount = 0;
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
            if(vbBuffer.size()>0&& ibBuffer.size()>0){
                drawhander(typed_memory_view(vbBuffer.size(), vbBuffer.buffer()),typed_memory_view(ibBuffer.size(), ibBuffer.buffer()),getString(lastTexture->texturePath),blendMode);
            }
            vbBuffer.clear();
            ibBuffer.clear();
            lastTexture = texture;
            blendMode = slotBlendMode;
            totalVertexCount = 0;
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

		for (int ii = 0; ii < verticesCount << 1; ii += 2) {
            vbBuffer.add((*vertices)[ii]);
            vbBuffer.add((*vertices)[ii + 1]);
            vbBuffer.add(color.r);
            vbBuffer.add(color.g);
            vbBuffer.add(color.b);
            vbBuffer.add(color.a);
            vbBuffer.add((*uvs)[ii]);
            vbBuffer.add((*uvs)[ii+1]);
            if(twoColorTint){
                vbBuffer.add(0);
                vbBuffer.add(0);
                vbBuffer.add(0);
                vbBuffer.add(0);
            }
		}

		for (int ii = 0; ii < (int) indices->size(); ii++)
			ibBuffer.add(totalVertexCount+(*indices)[ii]);

		totalVertexCount +=verticesCount;
		clipper.clipEnd(slot);
	}
	clipper.clipEnd();

    if(vbBuffer.size()>0&& ibBuffer.size()>0){
        drawhander(typed_memory_view(vbBuffer.size(), vbBuffer.buffer()),typed_memory_view(ibBuffer.size(), ibBuffer.buffer()),getString(lastTexture->texturePath),blendMode);
    }
    
}

EMSCRIPTEN_BINDINGS(spine)
{
    register_vector<char>("CharVector");
    register_vector<unsigned char>("VectorUnsignedChar");
    register_vector<std::string>("StringVector");
    function("drawSkeleton", &drawSkeleton,allow_raw_pointer<arg<0>>());


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

    class_<String>("String")
        .constructor<>()
        .constructor(&createString)
        .function("length", &String::length)
        .function("isEmpty", &String::isEmpty)
        .function("buffer",optional_override([](String &str)
                                                 {return getString(&str); }));

    class_<Event>("Event")
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
        .property("name", &AtlasPage::name)
        .property("texturePath", &AtlasPage::texturePath)
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
    
    class_<SkeletonData>("SkeletonData")
        .function("getSkinIndexByName", optional_override([](SkeletonData *data, std::string name)
                                                     {
                                                        String str = *createString(name);
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
                                                     }),
                  allow_raw_pointers())
        .function("getAnimationsSize", optional_override([](SkeletonData *data)
                                                     {
                                                        return data->getAnimations().size();
                                                     }),
                  allow_raw_pointers())
        .function("getAnimationByIndex", optional_override([](SkeletonData *data, int index)
                                                     {
                                                        return data->getAnimations()[index];
                                                     }),
                  allow_raw_pointers())
        ;
    class_<Animation>("Animation")
        .function("getName",optional_override([](Animation *anim)
                                                     {
                                                        const String name = anim->getName();
                                                        return getString(name);
                                                     }),allow_raw_pointers())
        .function("getDuration", &Animation::getDuration)
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
                                                            return binary->readSkeletonData(dataVec.data(), dataVec.size()); }),
                  allow_raw_pointers())
        ;

    class_<Skeleton>("Skeleton")
        .constructor<SkeletonData*>()
        .function("data",&Skeleton::getData,allow_raw_pointer<arg<0>>())
        .function("updateWorldTransform",&Skeleton::updateWorldTransform)
        .function("showSkinByIndex", optional_override([](Skeleton *skin, int index)
                                                     {
                                                        
                                                        skin->setSkin(skin->getData()->getSkins()[index]);
                                                             }),
                  allow_raw_pointers())
        .function("setToSetupPose",&Skeleton::setToSetupPose)
        .function("setBonesToSetupPose",&Skeleton::setBonesToSetupPose)
        .function("setSlotsToSetupPose",&Skeleton::setSlotsToSetupPose)
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
                                                             }),
                  allow_raw_pointers())
        .function("setAnimation", optional_override([](AnimationState *stat, val trackIndex, std::string animationName, bool loop)
                                                     {
                                                        return  stat->setAnimation(val_as<size_t>(trackIndex), *createString(animationName), loop);
                                                        }),
                  allow_raw_pointers())
        .function("getCurrent", optional_override([](AnimationState *stat, val trackIndex)
                                                     {
                                                        return  stat->getCurrent(val_as<size_t>(trackIndex));
                                                        }),
                  allow_raw_pointers())
        ;
}