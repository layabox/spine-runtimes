var meshesDemo = function (canvas, bgColor) {
	var canvas, gl, renderer, input, assetManager;
	var skeleton, bounds;
	var timeKeeper;
	var skeletons = {};
	var activeSkeleton = "Orange Girl";
	var playButton, timeline, isPlaying = true;

	if (!bgColor) bgColor = new spine.Color(235 / 255, 239 / 255, 244 / 255, 1);

	function init() {
		gl = canvas.context.gl;
		renderer = new spine.SceneRenderer(canvas, gl);
		renderer.skeletonDebugRenderer.drawRegionAttachments = false;
		assetManager = new spine.AssetManager(gl, spineDemos.path, spineDemos.downloader);
		assetManager.loadTextureAtlas("atlas2.atlas");
		assetManager.loadJson("demos.json");
		timeKeeper = new spine.TimeKeeper();
	}

	function loadingComplete() {
		timeKeeper.update();
		skeletons["Orange Girl"] = loadSkeleton("orangegirl", "animation");
		skeletons["Green Girl"] = loadSkeleton("greengirl", "animation");
		skeletons["Armor Girl"] = loadSkeleton("armorgirl", "animation");
		setupUI();
	}

	function setupUI() {
		playButton = $("#meshes-playbutton");
		var playButtonUpdate = function () {
			isPlaying = !isPlaying;
			if (isPlaying)
				playButton.addClass("pause").removeClass("play");
			else
				playButton.addClass("play").removeClass("pause");
		}
		playButton.click(playButtonUpdate);
		playButton.addClass("pause");

		timeline = $("#meshes-timeline").data("slider");
		timeline.changed = function (percent) {
			if (isPlaying) playButton.click();
			if (!isPlaying) {
				var active = skeletons[activeSkeleton];
				var animationDuration = active.state.getCurrent(0).animation.duration;
				var time = animationDuration * percent;
				active.state.update(time - active.playTime);
				active.state.apply(active.skeleton);
				active.skeleton.updateWorldTransform(spine.Physics.update);
				active.playTime = time;
			}
		};

		var list = $("#meshes-skeleton");
		for (var skeletonName in skeletons) {
			var option = $("<option></option>");
			option.attr("value", skeletonName).text(skeletonName);
			if (skeletonName === activeSkeleton) option.attr("selected", "selected");
			list.append(option);
		}
		list.change(function () {
			activeSkeleton = $("#meshes-skeleton option:selected").text();
			var active = skeletons[activeSkeleton];
			var animationDuration = active.state.getCurrent(0).animation.duration;
			timeline.set(active.playTime / animationDuration);
		})

		renderer.skeletonDebugRenderer.drawBones = false;
		$("#meshes-drawbonescheckbox").click(function () {
			renderer.skeletonDebugRenderer.drawBones = this.checked;
		})

		renderer.skeletonDebugRenderer.drawMeshHull = false;
		renderer.skeletonDebugRenderer.drawMeshTriangles = false;
		$("#meshes-drawmeshtrianglescheckbox").click(function () {
			renderer.skeletonDebugRenderer.drawMeshHull = this.checked;
			renderer.skeletonDebugRenderer.drawMeshTriangles = this.checked;
		})
	}

	function loadSkeleton(name, animation, sequenceSlots) {
		var atlasLoader = new spine.AtlasAttachmentLoader(assetManager.get("atlas2.atlas"));
		var skeletonJson = new spine.SkeletonJson(atlasLoader);
		var skeletonData = skeletonJson.readSkeletonData(assetManager.get("demos.json")[name]);
		var skeleton = new spine.Skeleton(skeletonData);
		skeleton.setSkinByName("default");

		var state = new spine.AnimationState(new spine.AnimationStateData(skeletonData));
		state.setAnimation(0, animation, true);
		state.apply(skeleton);
		skeleton.updateWorldTransform(spine.Physics.update);
		var offset = new spine.Vector2();
		var size = new spine.Vector2();
		skeleton.getBounds(offset, size, []);

		return {
			skeleton: skeleton,
			state: state,
			playTime: 0,
			bounds: {
				offset: offset,
				size: size
			}
		};
	}

	function render() {
		timeKeeper.update();
		var delta = timeKeeper.delta;

		var active = skeletons[activeSkeleton];
		var skeleton = active.skeleton;
		var state = active.state;
		var offset = active.bounds.offset;
		var size = active.bounds.size;

		renderer.camera.position.x = offset.x + size.x / 2;
		renderer.camera.position.y = offset.y + size.y / 2 + 20;
		renderer.camera.viewportWidth = size.x * 1.1;
		renderer.camera.viewportHeight = size.y * 1.1;
		renderer.resize(spine.ResizeMode.Fit);

		gl.clearColor(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
		gl.clear(gl.COLOR_BUFFER_BIT);

		if (isPlaying) {
			var animationDuration = state.getCurrent(0).animation.duration;
			active.playTime += delta;
			while (active.playTime >= animationDuration)
				active.playTime -= animationDuration;
			timeline.set(active.playTime / animationDuration);

			state.update(delta);
			state.apply(skeleton);
			skeleton.updateWorldTransform(spine.Physics.update);
		}

		renderer.begin();
		renderer.drawSkeleton(skeleton, true);
		renderer.drawSkeletonDebug(skeleton);
		renderer.end();
	}

	init();
	meshesDemo.assetManager = assetManager;
	meshesDemo.loadingComplete = loadingComplete;
	meshesDemo.render = render;
};