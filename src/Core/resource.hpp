#pragma once
#include <cstdint>
#include <vector>
#include <map>

namespace ShadyCore {
    class ResourceVisitor;
    class Resource {
    public:
		virtual ~Resource() {}
        virtual void visit(ResourceVisitor* visitor) = 0;
    };

	class TextResource : public Resource {
	protected:
		size_t length;
		char* data = 0;
	public:
		virtual ~TextResource();
		void initialize(size_t);
		void visit(ResourceVisitor* visitor) override;

		inline const size_t& getLength() const { return length; }
		inline const char* getData() const { return data; }
		inline char* getData() { return data; }
	};

    class LabelResource : public Resource {
    protected:
        const char* name = 0;
        uint32_t offset;
        uint32_t size;
    public:
		virtual ~LabelResource();
		void initialize(const char*);
		void visit(ResourceVisitor* visitor) override;

		inline const char* getName() const { return name; }
        inline const uint32_t& getOffset() const { return offset; }
        inline const uint32_t& getSize() const { return size; }
        inline void setOffset(uint32_t o) { offset = o; }
        inline void setSize(uint32_t s) { size = s; }
    };

    class Palette : public Resource {
    protected:
        uint8_t data[256 * 3];
    public:
        void visit(ResourceVisitor* visitor) override;

        inline const uint8_t* getData() const { return data; }
        inline uint8_t* getData() { return data; }
		inline uint32_t getColor(int index) const { index *= 3; return (index == 0 ? 0x00 : 0xff << 24) + ((uint32_t)data[index]) + ((uint32_t)data[index + 1] << 8) + ((uint32_t)data[index + 2] << 16); }
		inline void setColor(int index, uint32_t value) { index *= 3; data[index] = value; data[index + 1] = value >> 8; data[index + 2] = value >> 16; }

		static uint16_t packColor(uint32_t color, bool = false);
		static uint32_t unpackColor(uint16_t color);
    };

    class Image : public Resource {
    protected:
        uint32_t width, height, paddedWidth;
        uint8_t bitsPerPixel;
		uint8_t* raw = 0;
    public:
		virtual ~Image();
		void initialize(uint32_t, uint32_t, uint32_t, uint8_t);
        void visit(ResourceVisitor* visitor) override;

		inline const uint32_t& getWidth() const { return width; }
        inline const uint32_t& getHeight() const { return height; }
        inline const uint32_t& getPaddedWidth() const { return paddedWidth; } // TODO implicit pad creation;
        inline const uint8_t& getBitsPerPixel() const { return bitsPerPixel; }
        inline const uint8_t* getRawImage() const { return raw; }
		inline uint8_t* getRawImage() { return raw; }
	};

	class Sfx : public Resource {
	protected:
		uint16_t channels;
		uint32_t sampleRate;
		uint32_t byteRate;
		uint16_t blockAlign;
		uint16_t bitsPerSample;
		uint32_t size;
		uint8_t* data = 0;
	public:
		virtual ~Sfx();
		void initialize(uint32_t);
		void visit(ResourceVisitor* visitor) override;

		inline const uint16_t& getChannels() const { return channels; }
		inline const uint32_t& getSampleRate() const { return sampleRate; }
		inline const uint32_t& getByteRate() const { return byteRate; }
		inline const uint16_t& getBlockAlign() const { return blockAlign; }
		inline const uint16_t& getBitsPerSample() const { return bitsPerSample; }
		inline void setChannels(uint16_t value) { channels = value; }
		inline void setSampleRate(uint32_t value) { sampleRate = value; }
		inline void setByteRate(uint32_t value) { byteRate = value; }
		inline void setBlockAlign(uint16_t value) { blockAlign = value; }
		inline void setBitsPerSample(uint16_t value) { bitsPerSample = value; }

		inline const uint32_t& getSize() const { return size; }
		inline const uint8_t* getData() const { return data; }
		inline uint8_t* getData() { return data; }
	};

	class GuiRoot;

	class GuiImage : public Resource {
	protected:
		char* name = 0;
		int32_t x, y, w, h;
	public:
		virtual ~GuiImage();
		void initialize(const char*);
		void visit(ResourceVisitor* visitor) override;

		inline const char* getName() const { return name; }
		inline const int32_t& getX() const { return x; }
		inline const int32_t& getY() const { return y; }
		inline const int32_t& getWidth() const { return w; }
		inline const int32_t& getHeight() const { return h; }
		inline void setX(int32_t value) { x = value; }
		inline void setY(int32_t value) { y = value; }
		inline void setWidth(int32_t value) { w = value; }
		inline void setHeight(int32_t value) { h = value; }
	};

	class GuiView : public Resource {
    public:
        enum Type { VIEW_STATIC = 0, VIEW_MUTABLE = 1, VIEW_SLIDERHORZ = 2, VIEW_SLIDERVERT = 3, VIEW_NUMBER = 6 };
        class Impl;
        class Static;
		class Mutable;
        class SliderHorz;
        class SliderVert;
        class Number;
	protected:
		GuiRoot* root;
        Impl* impl = 0;
        Type type;
		uint32_t id;
	public:
		inline GuiView(GuiRoot* root) : root(root) {}
        virtual ~GuiView();
        void initialize(uint32_t, Type);
		void visit(ResourceVisitor* visitor) override;

		inline GuiRoot* getRoot() const { return root; }
		inline const uint32_t& getId() const { return id; }
		inline const Type& getType() const { return type; }

        inline Static& asStatic() { if (type == VIEW_STATIC) return *(Static*)impl; throw; }
        inline Mutable& asMutable() { if (type == VIEW_MUTABLE) return *(Mutable*)impl; throw; }
        inline SliderHorz& asSliderHorz() { if (type == VIEW_SLIDERHORZ) return *(SliderHorz*)impl; throw; }
        inline SliderVert& asSliderVert() { if (type == VIEW_SLIDERVERT) return *(SliderVert*)impl; throw; }
        inline Number& asNumber() { if (type == VIEW_NUMBER) return *(Number*)impl; throw; }
    };

    class GuiView::Impl : public Resource {
    protected:
        GuiView& base;
		const GuiImage* image = 0;
		int32_t x, y;
    public:
        inline Impl(GuiView& base) : base(base) {}
		inline GuiRoot* getRoot() const { return base.getRoot(); }
		inline const uint32_t& getId() const { return base.getId(); }
		inline const Type& getType() const { return base.getType(); }
		inline const GuiImage* getImage() const { return image; }
		inline const int32_t& getX() const { return x; }
		inline const int32_t& getY() const { return y; }
		inline void setImage(const GuiImage* value) { image = value; }
		inline void setX(int32_t value) { x = value; }
		inline void setY(int32_t value) { y = value; }
    };

    class GuiView::Static : public GuiView::Impl {
    protected:
        int32_t mirror;
    public:
        inline Static(GuiView& base) : Impl(base) {}
        void visit(ResourceVisitor* visitor) override;
        inline const int32_t& getMirror() const { return mirror; }
        inline void setMirror(int32_t value) { mirror = value; }
    };

    class GuiView::Mutable : public GuiView::Impl {
    public:
        inline Mutable(GuiView& base) : Impl(base) {}
        void visit(ResourceVisitor* visitor) override;
    };

    class GuiView::SliderHorz : public GuiView::Static {
    public: inline SliderHorz(GuiView& base) : Static(base) {}
    };

    class GuiView::SliderVert : public GuiView::Static {
    public: inline SliderVert(GuiView& base) : Static(base) {}
    };

    class GuiView::Number : public GuiView::Impl {
    protected:
        int32_t width, height, fontSpacing, textSpacing, size, floatSize;
    public:
        inline Number(GuiView& base) : Impl(base) {}
        void visit(ResourceVisitor* visitor) override;
        inline const int32_t& getWidth() const { return width; }
        inline const int32_t& getHeight() const { return height; }
        inline const int32_t& getFontSpacing() const { return fontSpacing; }
        inline const int32_t& getTextSpacing() const { return textSpacing; }
        inline const int32_t& getSize() const { return size; }
        inline const int32_t& getFloatSize() const { return floatSize; }
        inline void setWidth(const int32_t value) { width = value; }
        inline void setHeight(const int32_t value) { height = value; }
        inline void setFontSpacing(const int32_t value) { fontSpacing = value; }
        inline void setTextSpacing(const int32_t value) { textSpacing = value; }
        inline void setSize(const int32_t value) { size = value; }
        inline void setFloatSize(const int32_t value) { floatSize = value; }
    };

	class GuiRoot : public Resource {
	protected:
		std::vector<GuiImage*> images;
		std::vector<GuiView*> views;
	public:
		virtual ~GuiRoot();
		void initialize();
		void visit(ResourceVisitor* visitor) override;

		inline const uint32_t getImageCount() const { return images.size(); }
		inline const GuiImage& getImage(int i) const { return *images[i]; }
		inline GuiImage& getImage(int i) { return *images[i]; }
		inline GuiImage& createImage() { images.push_back(new GuiImage); return *images.back(); }
		inline const uint32_t getViewCount() const { return views.size(); }
		inline const GuiView& getView(int i) const { return *views[i]; }
		inline GuiView& getView(int i) { return *views[i]; }
		inline GuiView& createView() { views.push_back(new GuiView(this)); return *views.back(); }
	};

	class BlendOptions : public Resource {
	protected:
		uint32_t color; // offset 0x02
		uint16_t mode, scaleX, scaleY;
		int16_t flipVert, flipHorz, angle;
	public:
        void visit(ResourceVisitor* visitor) override;

		inline const uint16_t& getMode() const { return mode; }
		inline const uint32_t& getColor() const { return color; }
		inline const uint16_t& getScaleX() const { return scaleX; }
		inline const uint16_t& getScaleY() const { return scaleY; }
		inline const int16_t& getFlipVert() const { return flipVert; }
		inline const int16_t& getFlipHorz() const { return flipHorz; }
		inline const int16_t& getAngle() const { return angle; }
		inline void setMode(const uint16_t value) { mode = value; }
        inline void setColor(const uint32_t value) { color = value; }
        inline void setScaleX(const uint16_t value) { scaleX = value; }
        inline void setScaleY(const uint16_t value) { scaleY = value; }
        inline void setFlipVert(const int16_t value) { flipVert = value; }
        inline void setFlipHorz(const int16_t value) { flipHorz = value; }
        inline void setAngle(const int16_t value) { angle = value; }
	};

	class MoveTraits : public Resource {
	protected:
		uint16_t damage, proration, chipDamage, spiritDamage, untech, power, limit;
		uint16_t onHitPlayerStun, onHitEnemyStun, onBlockPlayerStun, onBlockEnemyStun;
		uint16_t onHitCardGain, onBlockCardGain, onAirHitSetSequence, onGroundHitSetSequence;
		int16_t speedX, speedY;
		uint16_t onHitSfx, onHitEffect, attackLevel;
		uint8_t comboModifier; // offset 0x28 (40)
		uint32_t frameFlags, attackFlags;
	public:
        void visit(ResourceVisitor* visitor) override;

		inline const uint16_t& getDamage() const { return damage; }
		inline const uint16_t& getProration() const { return proration; }
		inline const uint16_t& getChipDamage() const { return chipDamage; }
		inline const uint16_t& getSpiritDamage() const { return spiritDamage; }
		inline const uint16_t& getUntech() const { return untech; }
		inline const uint16_t& getPower() const { return power; }
		inline const uint16_t& getLimit() const { return limit; }
		inline const uint16_t& getOnHitPlayerStun() const { return onHitPlayerStun; }
		inline const uint16_t& getOnHitEnemyStun() const { return onHitEnemyStun; }
		inline const uint16_t& getOnBlockPlayerStun() const { return onBlockPlayerStun; }
		inline const uint16_t& getOnBlockEnemyStun() const { return onBlockEnemyStun; }
		inline const uint16_t& getOnHitCardGain() const { return onHitCardGain; }
		inline const uint16_t& getOnBlockCardGain() const { return onBlockCardGain; }
		inline const uint16_t& getOnAirHitSetSequence() const { return onAirHitSetSequence; }
		inline const uint16_t& getOnGroundHitSetSequence() const { return onGroundHitSetSequence; }
		inline const int16_t& getSpeedX() const { return speedX; }
		inline const int16_t& getSpeedY() const { return speedY; }
		inline const uint16_t& getOnHitSfx() const { return onHitSfx; }
		inline const uint16_t& getOnHitEffect() const { return onHitEffect; }
		inline const uint16_t& getAttackLevel() const { return attackLevel; }
		inline const uint8_t& getComboModifier() const { return comboModifier; }
		inline const uint32_t& getFrameFlags() const { return frameFlags; }
		inline const uint32_t& getAttackFlags() const { return attackFlags; }
        inline void setDamage(const uint16_t value) { damage = value; }
        inline void setProration(const uint16_t value) { proration = value; }
        inline void setChipDamage(const uint16_t value) { chipDamage = value; }
        inline void setSpiritDamage(const uint16_t value) { spiritDamage = value; }
        inline void setUntech(const uint16_t value) { untech = value; }
        inline void setPower(const uint16_t value) { power = value; }
        inline void setLimit(const uint16_t value) { limit = value; }
        inline void setOnHitPlayerStun(const uint16_t value) { onHitPlayerStun = value; }
        inline void setOnHitEnemyStun(const uint16_t value) { onHitEnemyStun = value; }
        inline void setOnBlockPlayerStun(const uint16_t value) { onBlockPlayerStun = value; }
        inline void setOnBlockEnemyStun(const uint16_t value) { onBlockEnemyStun = value; }
        inline void setOnHitCardGain(const uint16_t value) { onHitCardGain = value; }
        inline void setOnBlockCardGain(const uint16_t value) { onBlockCardGain = value; }
        inline void setOnAirHitSetSequence(const uint16_t value) { onAirHitSetSequence = value; }
        inline void setOnGroundHitSetSequence(const uint16_t value) { onGroundHitSetSequence = value; }
        inline void setSpeedX(const int16_t value) { speedX = value; }
        inline void setSpeedY(const int16_t value) { speedY = value; }
        inline void setOnHitSfx(const uint16_t value) { onHitSfx = value; }
        inline void setOnHitEffect(const uint16_t value) { onHitEffect = value; }
        inline void setAttackLevel(const uint16_t value) { attackLevel = value; }
        inline void setComboModifier(const uint8_t value) { comboModifier = value; }
        inline void setFrameFlags(const uint32_t value) { frameFlags = value; }
        inline void setAttackFlags(const uint32_t value) { attackFlags = value; }
	};

	class BBoxList : public Resource {
	public:
		class BBox;
	protected:
		std::vector<BBox> boxes;
		bool attack;
	public:
		inline BBoxList(bool attack) : attack(attack) {}
		inline void initialize() { boxes.clear(); }
		void visit(ResourceVisitor* visitor) override;

		inline bool isAttack() const { return attack; }
		inline const uint32_t getBoxCount() const { return boxes.size(); }
		inline const BBox& getBox(int i) const { return boxes[i]; }
		inline BBox& getBox(int i) { return boxes[i]; }
		inline BBox& createBox() { boxes.emplace_back(); return boxes.back(); }
	};

	class BBoxList::BBox {
	protected:
		int32_t left, up, right, down;
		uint8_t unknown = 0;
	public:
		inline const int32_t& getLeft() const { return left; }
		inline const int32_t& getUp() const { return up; }
		inline const int32_t& getRight() const { return right; }
		inline const int32_t& getDown() const { return down; }
		inline const uint8_t& getUnknown() const { return unknown; }
		inline void setLeft(const int32_t value) { left = value; }
		inline void setUp(const int32_t value) { up = value; }
		inline void setRight(const int32_t value) { right = value; }
		inline void setDown(const int32_t value) { down = value; }
		inline void setUnknown(const uint8_t value) { unknown = value; }
	};

	class MoveEffect : public Resource {
	protected:
		int32_t pivotX, pivotY;
		int32_t positionXExtra, positionYExtra;
		int32_t positionX, positionY;
		uint16_t unknown02RESETSTATE; //TODO
		int16_t speedX, speedY;
	public:
        void visit(ResourceVisitor* visitor) override;

		inline const int32_t& getPivotX() const { return pivotX; }
		inline const int32_t& getPivotY() const { return pivotY; }
		inline const int32_t& getPositionXExtra() const { return positionXExtra; }
		inline const int32_t& getPositionYExtra() const { return positionYExtra; }
		inline const int32_t& getPositionX() const { return positionX; }
		inline const int32_t& getPositionY() const { return positionY; }
		inline const uint16_t& getUnknown02() const { return unknown02RESETSTATE; }
		inline const int16_t& getSpeedX() const { return speedX; }
		inline const int16_t& getSpeedY() const { return speedY; }
        inline void setPivotX(const int32_t value) { pivotX = value; }
        inline void setPivotY(const int32_t value) { pivotY = value; }
        inline void setPositionXExtra(const int32_t value) { positionXExtra = value; }
        inline void setPositionYExtra(const int32_t value) { positionYExtra = value; }
        inline void setPositionX(const int32_t value) { positionX = value; }
        inline void setPositionY(const int32_t value) { positionY = value; }
        inline void setUnknown02(const uint16_t value) { unknown02RESETSTATE = value; }
        inline void setSpeedX(const int16_t value) { speedX = value; }
        inline void setSpeedY(const int16_t value) { speedY = value; }
	};

    class Pattern;

	class Frame : public Resource {
    public:
        enum Type { FRAME_ANIMATION, FRAME_MOVE };
        class Impl;
        class Animation;
        class Move;
	protected:
		Pattern* root;
        Impl* impl = 0;
        Type type;

		const char* imageName = 0;
        uint32_t index;
	public:
		inline Frame(Pattern* root) : root(root) {}
		virtual ~Frame();
        void initialize(const char*, uint32_t);
		void visit(ResourceVisitor* visitor) override;

		inline Pattern* getRoot() const { return root; }
		inline const Type& getType() const { return type; }
        inline const uint32_t& getIndex() const { return index; }
		inline const char* getImageName() const { return imageName; }
        void setImageName(const char* value);

        inline Animation& asAnimation() { if (type == FRAME_ANIMATION) return *(Animation*)impl; throw; }
        inline Move& asMove() { if (type == FRAME_MOVE) return *(Move*)impl; throw; }
	};

    class Frame::Impl : public Resource {
    protected:
        Frame& base;
    public:
        inline Impl(Frame& base) : base(base) {}

		inline Pattern* getRoot() const { return base.getRoot(); }
		inline const Type& getType() const { return base.getType(); }
        inline const uint32_t& getIndex() const { return base.getIndex(); }
        inline const char* getImageName() const { return base.getImageName(); }
        inline void setImageName(const char* value) { return base.setImageName(value); }
    };

    class Frame::Animation : public Frame::Impl {
    protected:
		uint16_t unknown;
		uint16_t texOffsetX;
		uint16_t texOffsetY;
		uint16_t texWidth;
		uint16_t texHeight;
		int16_t offsetX;
		int16_t offsetY;
		uint16_t duration;
		uint8_t renderGroup;

		BlendOptions blendOptions;
    public:
        inline Animation(Frame& base) : Impl(base) {}
        void visit(ResourceVisitor* visitor) override;

		inline const uint16_t& getUnknown() const { return unknown; } // @Unknown &raw[2]; 2Bytes
		inline const uint16_t& getTexOffsetX() const { return texOffsetX; }
		inline const uint16_t& getTexOffsetY() const { return texOffsetY; }
		inline const uint16_t& getTexWidth() const { return texWidth; }
		inline const uint16_t& getTexHeight() const { return texHeight; }
		inline const int16_t& getOffsetX() const { return offsetX; }
		inline const int16_t& getOffsetY() const { return offsetY; }
		inline const uint16_t& getDuration() const { return duration; }
        inline const uint8_t& getRenderGroup() const { return renderGroup; }
		inline void setUnknown(const uint16_t value) { unknown = value; } // @Unknown &raw[2]; 2Bytes
		inline void setTexOffsetX(const uint16_t value) { texOffsetX = value; }
		inline void setTexOffsetY(const uint16_t value) { texOffsetY = value; }
		inline void setTexWidth(const uint16_t value) { texWidth = value; }
		inline void setTexHeight(const uint16_t value) { texHeight = value; }
		inline void setOffsetX(const int16_t value) { offsetX = value; }
		inline void setOffsetY(const int16_t value) { offsetY = value; }
		inline void setDuration(const uint16_t value) { duration = value; }
        inline void setRenderGroup(const uint8_t value) { renderGroup = value; }

        inline const BlendOptions& getBlendOptions() const { if (hasBlendOptions()) return blendOptions; throw; }
        inline BlendOptions& getBlendOptions() { if (hasBlendOptions()) return blendOptions; throw; }
        inline bool hasBlendOptions() const { return renderGroup == 2; }
    };

    class Frame::Move : public Frame::Animation {
    protected:
		MoveTraits traits;
		BBoxList cBoxes;
		BBoxList hBoxes;
		BBoxList aBoxes;
		MoveEffect effect;
	public:
        inline Move(Frame& base) : Animation(base), cBoxes(false), hBoxes(false), aBoxes(true) {}
        void visit(ResourceVisitor* visitor) override;

		inline const MoveTraits& getTraits() const { return traits; }
		inline MoveTraits& getTraits() { return traits; }
		inline const BBoxList& getCollisionBoxes() const { return cBoxes; }
		inline BBoxList& getCollisionBoxes() { return cBoxes; }
		inline const BBoxList& getHitBoxes() const { return hBoxes; }
		inline BBoxList& getHitBoxes() { return hBoxes; }
		inline const BBoxList& getAttackBoxes() const { return aBoxes; }
		inline BBoxList& getAttackBoxes() { return aBoxes; }
		inline const MoveEffect& getEffect() const { return effect; }
        inline MoveEffect& getEffect() { return effect; }
    };

	class Sequence : public Resource {
    public:
        enum Type { SEQUENCE_CLONE, SEQUENCE_ANIMATION, SEQUENCE_MOVE };
        class Impl;
        class Clone;
        class Animation;
        class Move;
	protected:
		Pattern* root;
        Impl* impl = 0;
        Type type;
		uint32_t id;
	public:
		inline Sequence(Pattern* root) : root(root) {}
		virtual ~Sequence();
        void initialize(uint32_t, int32_t);
		void visit(ResourceVisitor* visitor) override;

		inline Pattern* getRoot() const { return root; }
		inline const uint32_t& getId() const { return id; }
		inline const Type& getType() const { return type; }

        inline Clone& asClone() { if (type == SEQUENCE_CLONE) return *(Clone*)impl; throw; }
        inline Animation& asAnimation() { if (type == SEQUENCE_ANIMATION) return *(Animation*)impl; throw; }
        inline Move& asMove() { if (type == SEQUENCE_MOVE) return *(Move*)impl; throw; }
	};

    class Sequence::Impl : public Resource {
    protected:
        Sequence& base;
    public:
        inline Impl(Sequence& base) : base(base) {}
        virtual ~Impl();

		inline Pattern* getRoot() const { return base.getRoot(); }
		inline const uint32_t& getId() const { return base.getId(); }
		inline const Type& getType() const { return base.getType(); }
    };

    class Sequence::Clone : public Sequence::Impl {
    protected:
        uint32_t targetId;
    public:
        inline Clone(Sequence& base) : Impl(base) {}
        void visit(ResourceVisitor* visitor) override;

        inline const uint32_t& getTargetId() const { return targetId; }
        inline void setTargetId(const uint32_t value) { targetId = value; }

        // TODO Object Handling
    };

    class Sequence::Animation : public Sequence::Impl {
    protected:
        uint32_t index;
        uint8_t loop;
        std::vector<Frame*> frames;
    public:
        inline Animation(Sequence& base, uint32_t index) : Impl(base), index(index) {}
        virtual ~Animation();
        void visit(ResourceVisitor* visitor) override;

        inline const uint32_t& getIndex() const { return index; }
        inline const uint8_t& getLoop() const { return loop; }
        inline void setLoop(const uint8_t value) { loop = value; }
		inline const uint32_t getFrameCount() const { return frames.size(); }
		inline const Frame& getFrame(int i) const { return *frames[i]; }
		inline Frame& getFrame(int i) { return *frames[i]; }
		inline Frame& createFrame() { frames.push_back(new Frame(base.getRoot())); return *frames.back(); }
    };

    class Sequence::Move : public Sequence::Animation {
	protected:
		uint16_t moveLock;
		uint16_t actionLock;
	public:
        inline Move(Sequence& base, uint32_t index) : Animation(base, index) {}
        void visit(ResourceVisitor* visitor) override;

		inline const uint16_t& getMoveLock() const { return moveLock; }
		inline const uint16_t& getActionLock() const { return actionLock; }
		inline void setMoveLock(const uint16_t value) { moveLock = value; }
		inline void setActionLock(const uint16_t value) { actionLock = value; }
    };

	class Pattern : public Resource {
	protected:
		bool animationOnly;
		std::vector<Sequence*> sequences;
	public:
        Pattern(bool);
		virtual ~Pattern();
		void initialize(bool);
		void visit(ResourceVisitor* visitor) override;

        inline const bool isAnimationOnly() const { return animationOnly; }
		inline const uint32_t getSequenceCount() const { return sequences.size(); }
		inline const Sequence& getSequence(uint32_t i) const { return *sequences[i]; }
		inline Sequence& getSequence(uint32_t i) { return *sequences[i]; }
		inline Sequence& createSequence() { sequences.push_back(new Sequence(this)); return *sequences.back(); }
        inline void eraseSequence(uint32_t index) { delete sequences[index]; sequences.erase(sequences.begin() + index); }
        inline void eraseSequence(uint32_t indexBegin, uint32_t indexEnd) {
            for (uint32_t i = indexBegin; i < indexEnd; ++i) delete sequences[i];
            sequences.erase(sequences.begin() + indexBegin, sequences.begin() + indexEnd); }
	};

    class ResourceVisitor {
	public: class TempData { public: virtual ~TempData() {} };
	protected: TempData* tempData = 0;
    public:
		virtual ~ResourceVisitor() { if (tempData) delete tempData; }
		virtual void accept(TextResource &) = 0;
		virtual void accept(LabelResource &) = 0;
		virtual void accept(Palette &) = 0;
        virtual void accept(Image &) = 0;
		virtual void accept(Sfx &) = 0;
		virtual void accept(GuiRoot &) = 0;
		virtual void accept(GuiImage &) = 0;
		virtual void accept(GuiView &) = 0;
		virtual void accept(GuiView::Static &) = 0;
        virtual void accept(GuiView::Mutable &) = 0;
		virtual void accept(GuiView::Number &) = 0;
		virtual void accept(Pattern &) = 0;
        virtual void accept(Sequence &) = 0;
        virtual void accept(Sequence::Clone &) = 0;
        virtual void accept(Sequence::Animation &) = 0;
        virtual void accept(Sequence::Move &) = 0;
        virtual void accept(Frame &) = 0;
        virtual void accept(Frame::Animation &) = 0;
        virtual void accept(Frame::Move &) = 0;
        virtual void accept(BlendOptions &) = 0;
        virtual void accept(MoveTraits &) = 0;
        virtual void accept(BBoxList &) = 0;
        virtual void accept(MoveEffect &) = 0;
	};
}
